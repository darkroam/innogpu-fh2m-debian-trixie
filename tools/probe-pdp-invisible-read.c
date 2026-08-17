#define _POSIX_C_SOURCE 200809L

#include <drm/drm.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

struct drm_pdp_gem_create {
	uint64_t size;
	uint32_t flags;
	uint32_t handle;
};

struct drm_pdp_gem_mmap {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

struct drm_pdp_gem_cpu_prep {
	uint32_t handle;
	uint32_t flags;
};

struct drm_pdp_gem_cpu_fini {
	uint32_t handle;
	uint32_t pad;
};

struct drm_pdp_gem_inv_get {
	uint32_t handle;
	uint32_t is_invisible;
};

#define PDP_GEM_CPU_PREP_READ (1U << 0)
#define PDP_GEM_CPU_PREP_WRITE (1U << 1)
#define PDP_GEM_INVISIBLE (1U << 28)
#define DRM_IOCTL_PDP_GEM_CREATE \
	DRM_IOWR(DRM_COMMAND_BASE + 0x20, struct drm_pdp_gem_create)
#define DRM_IOCTL_PDP_GEM_MMAP \
	DRM_IOWR(DRM_COMMAND_BASE + 0x21, struct drm_pdp_gem_mmap)
#define DRM_IOCTL_PDP_GEM_CPU_PREP \
	DRM_IOW(DRM_COMMAND_BASE + 0x22, struct drm_pdp_gem_cpu_prep)
#define DRM_IOCTL_PDP_GEM_CPU_FINI \
	DRM_IOW(DRM_COMMAND_BASE + 0x23, struct drm_pdp_gem_cpu_fini)
#define DRM_IOCTL_PDP_GEM_INV_GET \
	DRM_IOWR(DRM_COMMAND_BASE + 0x29, struct drm_pdp_gem_inv_get)

struct timing {
	struct timespec wall;
	struct rusage usage;
};

enum mapping_operation {
	MAPPING_READ,
	MAPPING_WRITE,
	MAPPING_VERIFY,
};

static volatile uint64_t read_checksum;

static double timespec_delta_ms(const struct timespec *start,
				const struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
	       (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static double timeval_delta_ms(const struct timeval *start,
			       const struct timeval *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
	       (double)(end->tv_usec - start->tv_usec) / 1000.0;
}

static int timing_now(struct timing *timing)
{
	if (clock_gettime(CLOCK_MONOTONIC, &timing->wall))
		return -1;
	return getrusage(RUSAGE_SELF, &timing->usage);
}

static void print_timing(const char *operation, const char *phase,
			 unsigned int iteration,
			 const struct timing *start, const struct timing *end)
{
	printf("iteration=%u phase=%s_%s wall_ms=%.3f user_ms=%.3f system_ms=%.3f\n",
	       iteration, operation, phase,
	       timespec_delta_ms(&start->wall, &end->wall),
	       timeval_delta_ms(&start->usage.ru_utime, &end->usage.ru_utime),
	       timeval_delta_ms(&start->usage.ru_stime, &end->usage.ru_stime));
}

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

static unsigned char page_pattern(unsigned int iteration, size_t page_index)
{
	return (unsigned char)(((iteration * 131U + page_index * 17U) % 251U) + 1U);
}

static const char *operation_name(enum mapping_operation operation)
{
	switch (operation) {
	case MAPPING_READ:
		return "read";
	case MAPPING_WRITE:
		return "write";
	case MAPPING_VERIFY:
		return "verify";
	}
	return "unknown";
}

static int exercise_mapping(int fd, uint32_t handle, uint64_t map_offset,
			    size_t map_size, size_t page_size,
			    unsigned int iteration,
			    enum mapping_operation operation)
{
	const char *name = operation_name(operation);
	struct drm_pdp_gem_cpu_prep prep = {
		.handle = handle,
		.flags = operation == MAPPING_WRITE ? PDP_GEM_CPU_PREP_WRITE :
			PDP_GEM_CPU_PREP_READ,
	};
	struct drm_pdp_gem_cpu_fini fini = {.handle = handle};
	struct timing touch_start;
	struct timing touch_end;
	struct timing unmap_start;
	struct timing unmap_end;
	volatile unsigned char *mapping = MAP_FAILED;
	size_t mismatches = 0;
	int prot = PROT_READ;
	int saved_errno;
	int prepped = 0;

	if (operation == MAPPING_WRITE)
		prot |= PROT_WRITE;
	if (ioctl(fd, DRM_IOCTL_PDP_GEM_CPU_PREP, &prep)) {
		fprintf(stderr, "iteration %u %s CPU_PREP failed: %s\n",
			iteration, name, strerror(errno));
		return -1;
	}
	prepped = 1;

	mapping = mmap(NULL, map_size, prot, MAP_SHARED, fd, (off_t)map_offset);
	if (mapping == MAP_FAILED) {
		fprintf(stderr, "iteration %u %s mmap failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}

	if (timing_now(&touch_start)) {
		fprintf(stderr, "iteration %u %s timing failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}
	for (size_t offset = 0; offset < map_size; offset += page_size) {
		size_t page_index = offset / page_size;

		if (operation == MAPPING_WRITE)
			mapping[offset] = page_pattern(iteration, page_index);
		else if (operation == MAPPING_VERIFY) {
			unsigned char expected = page_pattern(iteration, page_index);

			if (mapping[offset] != expected)
				mismatches++;
		} else {
			read_checksum += mapping[offset];
		}
	}
	if (timing_now(&touch_end)) {
		fprintf(stderr, "iteration %u %s timing failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}

	if (ioctl(fd, DRM_IOCTL_PDP_GEM_CPU_FINI, &fini)) {
		fprintf(stderr, "iteration %u %s CPU_FINI failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}
	prepped = 0;

	if (timing_now(&unmap_start)) {
		fprintf(stderr, "iteration %u %s timing failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}
	if (munmap((void *)mapping, map_size)) {
		fprintf(stderr, "iteration %u %s munmap failed: %s\n",
			iteration, name, strerror(errno));
		goto fail;
	}
	mapping = MAP_FAILED;
	if (timing_now(&unmap_end)) {
		fprintf(stderr, "iteration %u %s timing failed: %s\n",
			iteration, name, strerror(errno));
		return -1;
	}

	print_timing(name, "touch", iteration, &touch_start, &touch_end);
	print_timing(name, "munmap", iteration, &unmap_start, &unmap_end);
	if (mismatches) {
		fprintf(stderr,
			"iteration %u verify failed: %zu of %zu pages differ\n",
			iteration, mismatches, map_size / page_size);
		errno = EIO;
		return -1;
	}
	if (operation == MAPPING_VERIFY)
		printf("iteration=%u verify=pass pages=%zu\n", iteration,
		       map_size / page_size);
	return 0;

fail:
	saved_errno = errno;
	if (mapping != MAP_FAILED)
		munmap((void *)mapping, map_size);
	if (prepped)
		ioctl(fd, DRM_IOCTL_PDP_GEM_CPU_FINI, &fini);
	errno = saved_errno;
	return -1;
}

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/dri/renderD128";
	const char *access = argc > 4 ? argv[4] : "read";
	uint64_t requested_size = 7646720;
	uint64_t iterations_u64 = 3;
	struct drm_pdp_gem_create create = {.flags = PDP_GEM_INVISIBLE};
	struct drm_pdp_gem_mmap map = {0};
	struct drm_pdp_gem_inv_get inv = {0};
	struct drm_gem_close gem_close = {0};
	long page_size;
	size_t map_size;
	enum mapping_operation operation;
	int fd;
	int result = 1;

	if ((argc > 2 && parse_u64(argv[2], 1ULL << 30, &requested_size)) ||
	    (argc > 3 && parse_u64(argv[3], 100, &iterations_u64)) || argc > 5) {
		fprintf(stderr,
			"usage: %s [render-device] [size-bytes] [iterations] [read|write]\n",
			argv[0]);
		return 2;
	}
	if (!strcmp(access, "read"))
		operation = MAPPING_READ;
	else if (!strcmp(access, "write"))
		operation = MAPPING_WRITE;
	else {
		fprintf(stderr, "invalid access mode: %s (expected read or write)\n",
			access);
		return 2;
	}

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		fprintf(stderr, "sysconf(_SC_PAGESIZE) failed\n");
		return 1;
	}
	map_size = (size_t)((requested_size + (uint64_t)page_size - 1) &
			    ~((uint64_t)page_size - 1));

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
		return 1;
	}

	create.size = requested_size;
	if (ioctl(fd, DRM_IOCTL_PDP_GEM_CREATE, &create)) {
		fprintf(stderr, "PDP_GEM_CREATE failed: %s\n", strerror(errno));
		goto out_close_fd;
	}

	inv.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_PDP_GEM_INV_GET, &inv)) {
		fprintf(stderr, "PDP_GEM_INV_GET failed: %s\n", strerror(errno));
		goto out_close_gem;
	}
	if (!inv.is_invisible) {
		fprintf(stderr, "driver did not allocate an invisible GEM\n");
		goto out_close_gem;
	}

	map.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_PDP_GEM_MMAP, &map)) {
		fprintf(stderr, "PDP_GEM_MMAP failed: %s\n", strerror(errno));
		goto out_close_gem;
	}

	printf("device=%s handle=%u requested_size=%" PRIu64
	       " map_size=%zu pages=%zu offset=0x%" PRIx64 " iterations=%" PRIu64
	       " access=%s\n", device, create.handle, requested_size, map_size,
	       map_size / (size_t)page_size, map.offset, iterations_u64, access);

	for (unsigned int iteration = 1; iteration <= iterations_u64; iteration++) {
		if (exercise_mapping(fd, create.handle, map.offset, map_size,
				     (size_t)page_size, iteration, operation))
			goto out_close_gem;
		if (operation == MAPPING_WRITE &&
		    exercise_mapping(fd, create.handle, map.offset, map_size,
				     (size_t)page_size, iteration, MAPPING_VERIFY))
			goto out_close_gem;
	}

	printf("checksum=%" PRIu64 "\n", read_checksum);
	result = 0;

out_close_gem:
	gem_close.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &gem_close) && !result)
		result = 1;
out_close_fd:
	close(fd);
	return result;
}
