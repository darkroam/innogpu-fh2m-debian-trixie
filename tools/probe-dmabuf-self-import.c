#define _POSIX_C_SOURCE 200809L

/*
 * probe-dmabuf-self-import.c - DRM PRIME same-device self-import probe for FH2M.
 *
 * Opens the CARD node (KMS ioctls), creates a GEM object via the generic DRM
 * dumb-buffer path honoring the requested byte size (32bpp, height=1,
 * width=ceil(size/4)), exports it to a DMA-BUF fd (DRM_IOCTL_PRIME_HANDLE_TO_FD
 * with flags = DRM_CLOEXEC), re-imports the fd on the SAME innogpu DRM device
 * (DRM_IOCTL_PRIME_FD_TO_HANDLE), and strictly verifies object/handle/fd
 * lifecycle over multiple rounds with no process fd leak.
 *
 * The exported fd MUST carry FD_CLOEXEC (requested via DRM_CLOEXEC); the probe
 * does not repair a missing flag. Self-import returning the SAME handle is
 * allowed and that handle is closed exactly once.
 *
 * This proves the in-kernel PRIME self-import fast path on the local device. It
 * does NOT exercise foreign import (other exporter drivers), cross-device GTT
 * export, or V4L2/second-GPU paths; those remain UNVERIFIED on this single-GPU
 * machine (see docs/planning/webkit-dmabuf-investigation.md).
 *
 * Exit codes: 0=all rounds exported/imported/closed cleanly, no fd leak
 *             1=ioctl/execution/verification failure (incl. missing CLOEXEC,
 *               dumb size below request)
 *             2=usage/parameter error
 *             3=device or capability missing (open failed, or driver lacks
 *               dumb-buffer / PRIME support; capability printed to stdout)
 *
 * Resource order: per round the dma-buf fd, then the imported handle (when
 * distinct from the original), then the original GEM handle; the DRM fd is
 * closed last and open-fd counts are taken before open and after the final
 * close. A summary line is always printed (including device-open failure and
 * capability paths, but not usage errors) so contract tests can verify
 * fds_before == fds_after and consistent round counts.
 */

#include <drm/drm.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int parse_u64(const char *value, uint64_t limit, uint64_t *result)
{
	char *end = NULL;
	unsigned long long parsed;

	errno = 0;
	parsed = strtoull(value, &end, 0);
	if (errno || !end || *end != '\0' || !parsed || parsed > limit)
		return -1;
	*result = parsed;
	return 0;
}

static int count_open_fds(void)
{
	struct dirent *entry;
	DIR *dir;
	int count = 0;

	dir = opendir("/proc/self/fd");
	if (!dir)
		return -1;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		count++;
	}
	closedir(dir);
	return count;
}

static int close_gem_handle(int fd, uint32_t handle)
{
	struct drm_gem_close close_handle = {.handle = handle};

	return ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_handle);
}

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/dri/card0";
	uint64_t requested_size = 7646720;
	uint64_t iterations_u64 = 3;
	const char *capability = NULL;
	int fd = -1;
	int fds_before = -1;
	int fds_after = -1;
	unsigned int success = 0;
	unsigned int failures = 0;
	int result = 0;

	if ((argc > 2 && parse_u64(argv[2], 1ULL << 30, &requested_size)) ||
	    (argc > 3 && parse_u64(argv[3], 1000, &iterations_u64)) ||
	    argc > 4) {
		fprintf(stderr,
			"usage: %s [card-device] [size-bytes] [iterations]\n",
			argv[0]);
		return 2;
	}

	fds_before = count_open_fds();
	if (fds_before < 0) {
		fprintf(stderr, "count_open_fds before failed\n");
		return 1;
	}

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
		/* 设备打开失败也输出 summary；但 fd 计数不可用或变化时不得宣称无泄漏（遵循
		 * “计数不可用不得宣称无泄漏”契约，同正常路径一致）。
		 * 仅测试构建（-DINNOGPU_DMABUF_FIXTURE_HOOKS）暴露 INNOGPU_DMABUF_FIXTURE_OPEN_FAIL_FDCOUNT
		 * 注入钩子：强制 open 失败路径的 after 计数，验证 -1（unknown）或变化（yes）均不得 rc=3。
		 * 生产构建不定义该宏，钩子被完全编译剔除，环境变量无任何效果。 */
#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
		const char *fixture_fdcount =
			getenv("INNOGPU_DMABUF_FIXTURE_OPEN_FAIL_FDCOUNT");
		fds_after = fixture_fdcount ? atoi(fixture_fdcount)
					  : count_open_fds();
#else
		fds_after = count_open_fds();
#endif
		if (fds_after < 0 || fds_after != fds_before) {
			printf("summary self_import rounds=0 success=0 failures=0 "
			       "fds_before=%d fds_after=%d fd_leak=%s\n",
			       fds_before, fds_after,
			       fds_after < 0 ? "unknown" : "yes");
			return 1;   /* 计数不可用/泄漏：不能宣称无泄漏 */
		}
		printf("summary self_import rounds=0 success=0 failures=0 "
		       "fds_before=%d fds_after=%d fd_leak=no\n",
		       fds_before, fds_after);
		return 3;
	}

	printf("self_import device=%s size=%" PRIu64 " iterations=%" PRIu64
	       "\n", device, requested_size, iterations_u64);

	for (uint64_t round = 1; round <= iterations_u64; round++) {
		struct drm_mode_create_dumb create = {0};
		struct drm_prime_handle export_fd = {0};
		struct drm_prime_handle import_handle = {0};
		struct drm_gem_close close_import = {0};
		int created = 0;
		int exported = 0;
		int imported = 0;
		int imported_same = 0;
		int cloexec = 0;
		int saved_errno;

		/* 按请求尺寸创建：32bpp、height=1、width=ceil(size/4) */
		create.width = (uint32_t)((requested_size + 3) / 4);
		create.height = 1;
		create.bpp = 32;
		create.flags = 0;
		if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create)) {
			if (errno == ENOTTY || errno == EINVAL) {
				capability = "no-dumb-buffer";
				goto round_out;
			}
			fprintf(stderr, "round %" PRIu64 " CREATE_DUMB failed: %s\n",
				round, strerror(errno));
			result = 1;
			goto round_out;
		}
		created = 1;
		if (create.size < requested_size) {
			fprintf(stderr, "round %" PRIu64
				" dumb buffer size %" PRIu64 " < requested %" PRIu64 "\n",
				round, (uint64_t)create.size, requested_size);
			result = 1;
			goto round_out;
		}

		export_fd.handle = create.handle;
		export_fd.flags = DRM_CLOEXEC;
		if (ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &export_fd)) {
			if (errno == ENOTTY || errno == EINVAL) {
				capability = "no-prime-export";
				goto round_out;
			}
			fprintf(stderr, "round %" PRIu64 " PRIME export failed: %s\n",
				round, strerror(errno));
			result = 1;
			goto round_out;
		}
		exported = 1;

		{
			int flags = fcntl(export_fd.fd, F_GETFD);
			if (flags < 0) {
				fprintf(stderr, "round %" PRIu64 " F_GETFD failed: %s\n",
					round, strerror(errno));
				result = 1;
				goto round_out;
			}
			cloexec = (flags & FD_CLOEXEC) != 0;
			if (!cloexec) {
				fprintf(stderr, "round %" PRIu64
					" exported dma-buf fd missing FD_CLOEXEC\n",
					round);
				result = 1;
				goto round_out;
			}
		}

		import_handle.fd = export_fd.fd;
		if (ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &import_handle)) {
			if (errno == ENOTTY || errno == EINVAL) {
				capability = "no-prime-import";
				goto round_out;
			}
			fprintf(stderr, "round %" PRIu64 " PRIME import failed: %s\n",
				round, strerror(errno));
			result = 1;
			goto round_out;
		}
		imported = 1;
		imported_same = (import_handle.handle == create.handle);

		printf("round=%" PRIu64 " handle=%u create_size=%" PRIu64 " exported_fd=%d "
		       "cloexec=yes imported_handle=%u imported_same=%s ok\n",
		       round, create.handle, (uint64_t)create.size, export_fd.fd,
		       import_handle.handle, imported_same ? "yes" : "no");

		/* 成功路径逆序释放：fd → 导入 handle（若非同一）→ 原 handle */
		if (close(export_fd.fd)) {
			fprintf(stderr, "round %" PRIu64 " close dma-buf fd failed: %s\n",
				round, strerror(errno));
			exported = 0;
			result = 1;
			goto round_out;
		}
		exported = 0;
		if (!imported_same) {
			close_import.handle = import_handle.handle;
			if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_import)) {
				fprintf(stderr, "round %" PRIu64
					" close imported handle failed: %s\n",
					round, strerror(errno));
				imported = 0;
				result = 1;
				goto round_out;
			}
			imported = 0;
		}
		if (close_gem_handle(fd, create.handle)) {
			fprintf(stderr, "round %" PRIu64
				" close original handle failed: %s\n",
				round, strerror(errno));
			created = 0;
			result = 1;
			goto round_out;
		}
		created = 0;
		success++;
		continue;

round_out:
		saved_errno = errno;
		if (exported)
			close(export_fd.fd);
		if (imported && !imported_same) {
			close_import.handle = import_handle.handle;
			ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_import);
		}
		if (created)
			close_gem_handle(fd, create.handle);
		errno = saved_errno;
		/* 执行失败（含 result 已置位）一律计入 failures，保证 summary rounds 与退出码一致 */
		if (capability)
			goto out;
		failures++;
		goto out;
	}

out:
	if (fd >= 0)
		close(fd);
#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
	/* 测试构建专用：强制正常路径 after 计数（-1=不可用 / 变化=泄漏），验证 fd_leak=unknown/yes */
	{
		const char *fixture_fdcount =
			getenv("INNOGPU_DMABUF_FIXTURE_FDCOUNT_AFTER");
		if (fixture_fdcount)
			fds_after = atoi(fixture_fdcount);
		else
			fds_after = count_open_fds();
	}
#else
	fds_after = count_open_fds();
#endif
	if (fds_after < 0) {
		fprintf(stderr, "count_open_fds after failed\n");
		result = 1;   /* 无法完成泄漏检查，不能宣称 PASS */
	}
	if (fds_before < 0) {
		fprintf(stderr, "count_open_fds before failed\n");
		result = 1;
	}
	/* fd 计数不可用（<0）时不得宣称无泄漏：与 open 失败路径一致输出 unknown */
	const char *leak =
		(fds_after < 0 || fds_before < 0) ? "unknown"
		: (fds_after != fds_before) ? "yes" : "no";
	printf("summary self_import rounds=%u success=%u failures=%u "
	       "fds_before=%d fds_after=%d fd_leak=%s\n",
	       success + failures, success, failures, fds_before, fds_after, leak);

	if (capability) {
		printf("capability=%s\n", capability);
		return 3;
	}
	if (result || failures || (fds_after >= 0 && fds_before >= 0 &&
				   fds_after != fds_before))
		return 1;
	return 0;
}
