#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int interesting_path(const char *path)
{
    if (!path) {
        return 0;
    }
    return strstr(path, "/dev/dri") ||
           strstr(path, "/dev/fb") ||
           strstr(path, "innogpu") ||
           strstr(path, "inno") ||
           strstr(path, "_dri") ||
           strstr(path, "gbm") ||
           strstr(path, "glvnd") ||
           strstr(path, "EGL");
}

static void log_path_result(const char *fn, const char *path, int ret)
{
    if (interesting_path(path) || ret < 0) {
        int saved = errno;
        dprintf(2, "[trace-loader] %s(%s) = %d errno=%d (%s)\n",
                fn, path ? path : "NULL", ret, saved, strerror(saved));
        errno = saved;
    }
}

int open(const char *path, int flags, ...)
{
    static int (*real_open)(const char *, int, ...) = NULL;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (!real_open) {
        real_open = dlsym(RTLD_NEXT, "open");
    }
    int ret = (flags & O_CREAT) ? real_open(path, flags, mode) : real_open(path, flags);
    log_path_result("open", path, ret);
    return ret;
}

int open64(const char *path, int flags, ...)
{
    static int (*real_open64)(const char *, int, ...) = NULL;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (!real_open64) {
        real_open64 = dlsym(RTLD_NEXT, "open64");
    }
    int ret = (flags & O_CREAT) ? real_open64(path, flags, mode) : real_open64(path, flags);
    log_path_result("open64", path, ret);
    return ret;
}

int openat(int dirfd, const char *path, int flags, ...)
{
    static int (*real_openat)(int, const char *, int, ...) = NULL;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (!real_openat) {
        real_openat = dlsym(RTLD_NEXT, "openat");
    }
    int ret = (flags & O_CREAT) ? real_openat(dirfd, path, flags, mode) : real_openat(dirfd, path, flags);
    log_path_result("openat", path, ret);
    return ret;
}

void *dlopen(const char *filename, int flags)
{
    static void *(*real_dlopen)(const char *, int) = NULL;
    if (!real_dlopen) {
        real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    }
    void *ret = real_dlopen(filename, flags);
    if (interesting_path(filename) || !ret) {
        dprintf(2, "[trace-loader] dlopen(%s) = %p err=%s\n",
                filename ? filename : "NULL", ret, dlerror());
    }
    return ret;
}

int ioctl(int fd, unsigned long request, ...)
{
    static int (*real_ioctl)(int, unsigned long, ...) = NULL;
    void *arg = NULL;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);
    if (!real_ioctl) {
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    }
    int ret = real_ioctl(fd, request, arg);
    if (ret < 0) {
        int saved = errno;
        dprintf(2, "[trace-loader] ioctl(fd=%d, req=0x%lx) = %d errno=%d (%s)\n",
                fd, request, ret, saved, strerror(saved));
        errno = saved;
    }
    return ret;
}
