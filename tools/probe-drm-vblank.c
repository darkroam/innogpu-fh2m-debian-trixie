#define _POSIX_C_SOURCE 200809L

#include <drm/drm.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static double elapsed_ms(const struct timespec *start, const struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
	       (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static volatile sig_atomic_t wait_timed_out;

static void timeout_handler(int signal_number)
{
	(void)signal_number;
	wait_timed_out = 1;
}

static int arm_timeout(unsigned int timeout_ms)
{
	struct itimerval timer = {0};

	timer.it_value.tv_sec = timeout_ms / 1000;
	timer.it_value.tv_usec = (suseconds_t)(timeout_ms % 1000) * 1000;
	return setitimer(ITIMER_REAL, &timer, NULL);
}

static int parse_uint(const char *value, unsigned int limit, unsigned int *result)
{
	char *end = NULL;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(value, &end, 10);
	if (errno || !end || *end != '\0' || parsed > limit)
		return -1;

	*result = (unsigned int)parsed;
	return 0;
}

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/dri/card0";
	unsigned int crtc = 0;
	unsigned int samples = 20;
	unsigned int timeout_ms = 1000;
	unsigned int success = 0;
	unsigned int failures = 0;
	unsigned int fast_returns = 0;
	unsigned int nonadvancing = 0;
	unsigned int previous_sequence = 0;
	double previous_kernel_ms = 0.0;
	double total_ms = 0.0;
	double min_ms = 0.0;
	double max_ms = 0.0;
	int fd;
	struct sigaction action = {0};

	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc > 2 && parse_uint(argv[2], 31, &crtc)) {
		fprintf(stderr, "invalid CRTC index: %s\n", argv[2]);
		return 2;
	}
	if (argc > 3 && parse_uint(argv[3], 10000, &samples)) {
		fprintf(stderr, "invalid sample count: %s\n", argv[3]);
		return 2;
	}
	if (argc > 4 && parse_uint(argv[4], 60000, &timeout_ms)) {
		fprintf(stderr, "invalid timeout: %s\n", argv[4]);
		return 2;
	}
	if (argc > 5 || samples == 0 || timeout_ms == 0) {
		fprintf(stderr,
			"usage: %s [card-device] [crtc-index] [samples] [timeout-ms]\n",
			argv[0]);
		return 2;
	}

	action.sa_handler = timeout_handler;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGALRM, &action, NULL)) {
		fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
		return 1;
	}

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
		return 1;
	}

	printf("device=%s crtc=%u samples=%u timeout_ms=%u\n", device, crtc,
	       samples, timeout_ms);
	printf("sample sequence wait_ms kernel_time_ms sequence_delta kernel_delta_ms result\n");

	for (unsigned int i = 0; i < samples; i++) {
		union drm_wait_vblank wait = {0};
		struct timespec start;
		struct timespec end;
		double wait_ms;
		double kernel_ms;
		unsigned int sequence_delta = 0;
		double kernel_delta_ms = 0.0;
		int saved_errno;
		int ret;

		wait.request.type = (enum drm_vblank_seq_type)(
			_DRM_VBLANK_RELATIVE |
			((crtc << _DRM_VBLANK_HIGH_CRTC_SHIFT) & _DRM_VBLANK_HIGH_CRTC_MASK));
		wait.request.sequence = 1;

		if (clock_gettime(CLOCK_MONOTONIC, &start)) {
			fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}

		wait_timed_out = 0;
		if (arm_timeout(timeout_ms)) {
			fprintf(stderr, "setitimer failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, &wait);
		saved_errno = errno;
		if (arm_timeout(0)) {
			fprintf(stderr, "clear setitimer failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		if (clock_gettime(CLOCK_MONOTONIC, &end)) {
			fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}

		wait_ms = elapsed_ms(&start, &end);
		if (ret) {
			failures++;
			printf("%u - %.3f - - - %s:%s\n", i + 1, wait_ms,
			       wait_timed_out ? "timeout" : "error", strerror(saved_errno));
			continue;
		}

		kernel_ms = (double)wait.reply.tval_sec * 1000.0 +
			    (double)wait.reply.tval_usec / 1000.0;
		if (success) {
			sequence_delta = wait.reply.sequence - previous_sequence;
			kernel_delta_ms = kernel_ms - previous_kernel_ms;
			if (sequence_delta == 0)
				nonadvancing++;
		}
		if (wait_ms < 1.0)
			fast_returns++;

		if (!success || wait_ms < min_ms)
			min_ms = wait_ms;
		if (!success || wait_ms > max_ms)
			max_ms = wait_ms;
		total_ms += wait_ms;
		success++;
		previous_sequence = wait.reply.sequence;
		previous_kernel_ms = kernel_ms;

		printf("%u %u %.3f %.3f %u %.3f ok\n", i + 1,
		       wait.reply.sequence, wait_ms, kernel_ms, sequence_delta,
		       kernel_delta_ms);
	}

	close(fd);
	printf("summary success=%u failures=%u avg_wait_ms=%.3f min_wait_ms=%.3f "
	       "max_wait_ms=%.3f fast_returns=%u nonadvancing=%u\n",
	       success, failures, success ? total_ms / success : 0.0, min_ms,
	       max_ms, fast_returns, nonadvancing);

	return failures || !success || fast_returns || nonadvancing ? 1 : 0;
}
