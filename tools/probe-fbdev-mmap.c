#define _POSIX_C_SOURCE 200809L

#include <linux/fb.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/*
 * tools/probe-fbdev-mmap.c — F4 新探针（validation-plan §〇 F4）。
 *
 * 非破坏性契约：open /dev/fb0 → FBIOGET_FSCREENINFO → 选单页测试区 →
 * 保存原始字节 → mmap → 写入测试 → 读回比对 → 恢复原始字节 → munmap。
 * atexit 清理兜底（异常退出也恢复测试页）；HUP/INT/TERM 信号处理器只置
 * volatile sig_atomic_t 标志（异步信号安全），主流程检查点恢复测试页后以
 * 128+signum 退出；长度/偏移越界测试（mmap 失败即 PASS，成功即 FAIL）。
 *
 * 单一源码、双构建用途：
 *   - 生产构建（无夹具宏）→ 真机 R2 权威判定（权威行无 fixture_ 前缀）；
 *   - -DINNOGPU_DMABUF_FIXTURE_HOOKS 夹具构建 + 环境变量
 *     INNOGPU_FBDEV_FIXTURE=1 → 仅静态自测（进程内伪造 framebuffer 页，
 *     输出使用 fixture_ 命名空间，绝不产出真机权威行）。
 * 生产构建不定义该宏，环境变量无任何效果。
 *
 * 覆盖边界（见 validation-plan「F4 覆盖边界声明」）：静态夹具只证明探针
 * 自身控制流/解析/保存→写→读回→恢复逻辑/清理兜底/越界拒绝/输出契约；
 * 内核 fb_io_mmap 语义只能由真机 R2 证明。
 */

static int fb_fd = -1;
static unsigned char *saved = NULL;
static unsigned char *mapped = NULL;
static size_t map_len = 0;
static size_t map_off = 0;   /* 测试区在 mapped 内的偏移（生产=0，夹具=区域偏移） */
static int restored = 0;
static int fixture_mapped = 0;
static const char *ns = "";   /* 生产：""；夹具：fixture_ 前缀由输出行携带 */

/* 信号处理契约（codex 复审 P1-4）：处理器只做 volatile sig_atomic_t 赋值
 * （异步信号安全），绝不调用 cleanup/memcpy/msync/munmap/close/free；恢复
 * 由主流程在检查点执行，退出码 128+signum（HUP/INT/TERM → 129/130/143）。 */
static volatile sig_atomic_t caught_signal = 0;

static void on_signal(int sig)
{
	caught_signal = sig;
}

static void restore_page(void)
{
	if (mapped && saved && !restored && map_len > 0) {
		memcpy(mapped + map_off, saved, map_len);
		if (!fixture_mapped)
			msync(mapped, map_len, MS_SYNC);
		restored = 1;
	}
}

static void cleanup(void)
{
	restore_page();
	if (mapped && mapped != MAP_FAILED && !fixture_mapped)
		munmap(mapped, map_len);
	mapped = NULL;
	if (fb_fd >= 0)
		close(fb_fd);
	fb_fd = -1;
	free(saved);
	saved = NULL;
}

/* 主流程检查点：信号已到 → 恢复测试页并带退出码返回（cleanup 幂等，由
 * atexit 兜底完成 munmap/close/free）；未到 → 0。 */
static int signal_checkpoint(void)
{
	if (!caught_signal)
		return 0;
	restore_page();
	fprintf(stderr, "%sfbdev_mmap=INTERRUPTED signal=%d page_restored=%s\n",
		ns, (int)caught_signal, restored ? "yes" : "no");
	return 128 + (int)caught_signal;
}

/* 单页测试区选择：页对齐偏移，默认取 smem 中部一页；不足一页 → FAIL */
static int select_region(size_t total, long page, size_t *off, size_t *len)
{
	if (total < (size_t)page) {
		fprintf(stderr, "%sfbdev_mmap=FAIL reason=framebuffer_smaller_than_page smem_len=%zu page=%ld\n",
			ns, total, page);
		return 1;
	}
	*len = (size_t)page;
	*off = (total / 2) & ~((size_t)page - 1);
	if (*off + *len > total)
		*off = total - *len;
	return 0;
}

/*
 * 单页测试主体（生产与夹具共用）：保存 → 写 → 全页读回比对 → 恢复 → 校验恢复。
 * 返回 0=全过 1=失败。base 为已映射的测试区指针。读回比对覆盖整个测试页
 * （codex 初审 P2-6：不得只校验 pattern 长度子集）。
 */
static int page_test(unsigned char *base, size_t off, size_t len)
{
	unsigned char *page = base + off;
	unsigned char pattern[256];
	int rc = 0;
	int mismatch = 0;

	saved = malloc(len);
	if (!saved) {
		fprintf(stderr, "%sfbdev_mmap=FAIL reason=save_alloc_failed\n", ns);
		return 1;
	}
	memcpy(saved, page, len);

	for (size_t i = 0; i < sizeof(pattern); i++)
		pattern[i] = (unsigned char)(0x5A + (i & 0x0F));
	for (size_t i = 0; i < len; i++)
		page[i] = pattern[i & 0xFF];

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
	/* 夹具负向注入：写后读回前篡改后半页一字节（仅夹具构建有效） */
	if (getenv("INNOGPU_FBDEV_FIXTURE_CORRUPT") && len > 1)
		page[len / 2] ^= 0xFF;

	/* 夹具同步钩子（codex 复审 P1-4 真实信号中断测试）：写坏测试页后进入
	 * 持留窗口（页面处于脏态）——先创建 hold 标记文件供测试进程同步，再按
	 * 10ms 步长等待预算耗尽或信号标志置位；信号处理器只置标志，恢复由
	 * 主流程（本函数后续 restore_page 或检查点）执行。 */
	{
		const char *hold_ms = getenv("INNOGPU_FBDEV_FIXTURE_HOLD_MS");
		const char *hold_file = getenv("INNOGPU_FBDEV_FIXTURE_HOLD_FILE");
		if (hold_file && *hold_file) {
			FILE *f = fopen(hold_file, "w");
			if (f) {
				fprintf(f, "dirty\n");
				fclose(f);
			}
		}
		if (hold_ms && *hold_ms) {
			long budget = strtol(hold_ms, NULL, 10);
			if (budget < 0)
				budget = 0;
			if (budget > 60000)
				budget = 60000;
			while (budget > 0 && !caught_signal) {
				struct timespec ts = { .tv_sec = 0,
						       .tv_nsec = 10000000 };
				nanosleep(&ts, NULL);
				budget -= 10;
			}
		}
	}
#endif

	for (size_t i = 0; i < len && !mismatch; i++)
		if (page[i] != pattern[i & 0xFF])
			mismatch = 1;
	if (mismatch) {
		fprintf(stderr, "%sfbdev_mmap=FAIL reason=write_readback_mismatch bytes=%zu\n",
			ns, len);
		rc = 1;
	}

	restore_page();
	if (!restored) {
		fprintf(stderr, "%sfbdev_mmap=FAIL reason=restore_not_applied\n", ns);
		rc = 1;
	} else if (memcmp(page, saved, len) != 0) {
		fprintf(stderr, "%sfbdev_mmap=FAIL reason=restore_bytes_mismatch\n", ns);
		rc = 1;
	}

	if (rc == 0) {
		printf("%sfbdev_mmap_write_readback=PASS bytes=%zu\n", ns, len);
		printf("%sfbdev_mmap_restore=PASS bytes=%zu original_bytes_match=yes\n", ns, len);
	} else {
		/* 失败路径也尽力恢复：page_test 内已尝试；再兜底一次 */
		if (!restored && mapped && saved)
			memcpy(page, saved, len);
	}
	return rc;
}

/*
 * 越界拒绝测试：off+len 超出 smem_len 的 mmap 必须失败（EINVAL）。
 * 返回 0=PASS（全部拒绝）1=FAIL（出现越界成功）。fixture 模式用
 * fake_mmap 模拟内核拒绝语义。
 */
#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
static unsigned char *fake_base;
static size_t fake_total;

static void *fake_mmap(void *addr, size_t length, int prot, int flags,
		       int fd, off_t offset)
{
	(void)addr; (void)prot; (void)flags; (void)fd;
	if (offset < 0 || (size_t)offset >= fake_total ||
	    (size_t)offset + length > fake_total || length == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}
	return fake_base + (size_t)offset;
}
#define do_mmap fake_mmap
#else
#define do_mmap mmap
#endif

static int bounds_test(int fd, size_t total, long page)
{
	struct { off_t off; size_t len; const char *what; } cases[] = {
		{ (off_t)total, (size_t)page, "offset_at_smem_end" },
		{ (off_t)(total - (size_t)page + 1), (size_t)page, "offset_crossing_end" },
		{ 0, 0, "zero_length" },
	};
	size_t ncases = sizeof(cases) / sizeof(cases[0]);

	for (size_t i = 0; i < ncases; i++) {
		void *p = do_mmap(NULL, cases[i].len, PROT_READ | PROT_WRITE,
				  MAP_SHARED, fd, cases[i].off);
		if (p == MAP_FAILED)
			continue;
		fprintf(stderr,
			"%sfbdev_mmap=FAIL reason=bounds_violation case=%s "
			"(out-of-range mmap unexpectedly succeeded)\n",
			ns, cases[i].what);
		munmap(p, cases[i].len ? cases[i].len : 1);
		return 1;
	}
	printf("%sfbdev_mmap_bounds=PASS cases=%zu (out-of-range mmap rejected)\n",
	       ns, ncases);
	return 0;
}

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
/* 夹具运行：进程内伪造 framebuffer（3 页），驱动生产解析/恢复代码路径 */
static int fixture_run(void)
{
	long page = sysconf(_SC_PAGESIZE);
	size_t total = (size_t)page * 3;
	size_t off, len;
	int rc;

	ns = "fixture_";
	fake_base = calloc(total, 1);
	if (!fake_base) {
		fprintf(stderr, "fixture_fbdev_mmap=FAIL reason=fixture_alloc_failed\n");
		return 1;
	}
	fake_total = total;
	fixture_mapped = 1;

	printf("fixture_fbdev_mmap=START mode=fixture total=%zu page=%ld\n", total, page);
	if (select_region(total, page, &off, &len))
		return 1;
	printf("fixture_fbdev_mmap_fix=PASS smem_len=%zu line_length=%zu mode=fixture\n",
	       total, (size_t)page);
	printf("fixture_fbdev_mmap_region=PASS offset=%zu length=%zu mode=fixture\n",
	       off, len);

	mapped = fake_base;
	map_len = len;
	map_off = off;
	rc = page_test(fake_base, off, len);
	if (rc)
		return 1;
	{
		int sc = signal_checkpoint();
		if (sc)
			return sc;
	}

	if (bounds_test(-1, total, page))
		return 1;

	/* 注入「写后异常退出」：清理兜底必须恢复测试页（再次写坏后直接触发
	 * cleanup 路径，恢复后逐字节一致） */
	{
		unsigned char *page = fake_base + off;
		saved = malloc(len);
		if (!saved)
			return 1;
		memcpy(saved, page, len);
		for (size_t i = 0; i < len; i++)
			page[i] = (unsigned char)(i & 0xFF);
		restored = 0;
		restore_page();
		if (memcmp(page, saved, len) != 0) {
			fprintf(stderr, "fixture_fbdev_mmap=FAIL reason=cleanup_restore_failed\n");
			return 1;
		}
		free(saved);
		saved = NULL;
		printf("fixture_fbdev_mmap_cleanup=PASS abnormal_exit_restore_verified mode=fixture\n");
	}

	{
		int sc = signal_checkpoint();
		if (sc)
			return sc;
	}
	printf("fixture_fbdev_mmap_overall=PASS mode=fixture\n");
	return 0;
}
#endif

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/fb0";
	struct fb_fix_screeninfo fix;
	long page;
	size_t total, off, len;
	void *map;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [fbdev-device]\n", argv[0]);
		return 2;
	}

	if (atexit(cleanup) != 0)
		return 1;
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
	if (getenv("INNOGPU_FBDEV_FIXTURE"))
		return fixture_run();
#endif

	fb_fd = open(device, O_RDWR | O_CLOEXEC);
	if (fb_fd < 0) {
		fprintf(stderr, "fbdev_mmap=FAIL reason=open_failed device=%s errno=%s\n",
			device, strerror(errno));
		return 1;
	}
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) != 0) {
		fprintf(stderr, "fbdev_mmap=FAIL reason=FBIOGET_FSCREENINFO_failed errno=%s\n",
			strerror(errno));
		return 1;
	}
	if (fix.smem_len <= 0 || fix.line_length <= 0) {
		fprintf(stderr,
			"fbdev_mmap=FAIL reason=invalid_fix_info smem_len=%u line_length=%u\n",
			fix.smem_len, fix.line_length);
		return 1;
	}
	total = (size_t)fix.smem_len;
	page = sysconf(_SC_PAGESIZE);
	if (page <= 0) {
		fprintf(stderr, "fbdev_mmap=FAIL reason=pagesize_unavailable\n");
		return 1;
	}

	printf("fbdev_mmap=START device=%s\n", device);
	printf("fbdev_mmap_fix=PASS smem_len=%zu line_length=%u\n",
	       total, fix.line_length);
	if (select_region(total, page, &off, &len))
		return 1;
	printf("fbdev_mmap_region=PASS offset=%zu length=%zu (single page)\n", off, len);

	map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)off);
	if (map == MAP_FAILED) {
		fprintf(stderr, "fbdev_mmap=FAIL reason=mmap_failed offset=%zu errno=%s\n",
			off, strerror(errno));
		return 1;
	}
	mapped = map;
	map_len = len;

	if (page_test(map, 0, len))
		return 1;
	{
		int sc = signal_checkpoint();
		if (sc)
			return sc;
	}
	if (bounds_test(fb_fd, total, page))
		return 1;
	{
		int sc = signal_checkpoint();
		if (sc)
			return sc;
	}

	printf("fbdev_mmap_overall=PASS\n");
	return 0;
}
