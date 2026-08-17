#define _GNU_SOURCE
#include <dlfcn.h>
#include <drm/drm.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct drm_pdp_gem_cpu_prep_trace {
    uint32_t handle;
    uint32_t flags;
};

struct drm_pdp_gem_create_trace {
    uint64_t size;
    uint32_t flags;
    uint32_t handle;
};

struct drm_pdp_gem_inv_get_trace {
    uint32_t handle;
    uint32_t is_invisible;
};

struct drm_pdp_gem_cpu_fini_trace {
    uint32_t handle;
    uint32_t pad;
};

#define PDP_GEM_CPU_PREP_READ (1U << 0)
#define PDP_GEM_CPU_PREP_WRITE (1U << 1)
#define PDP_GEM_CPU_PREP_NOWAIT (1U << 2)
#define PDP_GEM_INVISIBLE (1U << 28)
#define DRM_IOCTL_PDP_GEM_CREATE_TRACE \
    DRM_IOWR(DRM_COMMAND_BASE + 0x20, struct drm_pdp_gem_create_trace)
#define DRM_IOCTL_PDP_GEM_CPU_PREP_TRACE \
    DRM_IOW(DRM_COMMAND_BASE + 0x22, struct drm_pdp_gem_cpu_prep_trace)
#define DRM_IOCTL_PDP_GEM_CPU_FINI_TRACE \
    DRM_IOW(DRM_COMMAND_BASE + 0x23, struct drm_pdp_gem_cpu_fini_trace)
#define DRM_IOCTL_PDP_GEM_INV_GET_TRACE \
    DRM_IOWR(DRM_COMMAND_BASE + 0x29, struct drm_pdp_gem_inv_get_trace)

static int emulate_cpu_prep_usage(void)
{
    static int initialized;
    static int enabled;

    if (!initialized) {
        const char *value = getenv("INNO_TRACE_EMULATE_CPU_PREP_USAGE");
        enabled = value && strcmp(value, "0");
        initialized = 1;
    }
    return enabled;
}

static int force_visible_gem(void)
{
    static int initialized;
    static int enabled;

    if (!initialized) {
        const char *value = getenv("INNO_TRACE_FORCE_VISIBLE_GEM");
        enabled = value && strcmp(value, "0");
        initialized = 1;
    }
    return enabled;
}

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

    uint32_t original_create_flags = 0;
    uint32_t original_flags = 0;
    int emulated = 0;
    int forced_visible = 0;
    if (request == DRM_IOCTL_PDP_GEM_CREATE_TRACE && arg) {
        struct drm_pdp_gem_create_trace *create = arg;
        original_create_flags = create->flags;
        if (force_visible_gem() && (create->flags & PDP_GEM_INVISIBLE)) {
            create->flags &= ~PDP_GEM_INVISIBLE;
            forced_visible = 1;
        }
    } else if (request == DRM_IOCTL_PDP_GEM_CPU_PREP_TRACE && arg) {
        struct drm_pdp_gem_cpu_prep_trace *prep = arg;
        original_flags = prep->flags;
        if (emulate_cpu_prep_usage() &&
            (prep->flags & PDP_GEM_CPU_PREP_READ) &&
            !(prep->flags & PDP_GEM_CPU_PREP_WRITE)) {
            prep->flags |= PDP_GEM_CPU_PREP_WRITE;
            emulated = 1;
        }
    }

    int ret = real_ioctl(fd, request, arg);
    int saved = errno;
    if (request == DRM_IOCTL_PDP_GEM_CREATE_TRACE && arg) {
        struct drm_pdp_gem_create_trace *create = arg;
        uint32_t effective_flags = create->flags;
        create->flags = original_create_flags;
        dprintf(2, "[trace-loader] PDP_GEM_CREATE fd=%d size=%llu flags=0x%x "
                "effective=0x%x forced_visible=%d invisible=%u handle=%u "
                "ret=%d errno=%d\n", fd, (unsigned long long)create->size,
                original_create_flags, effective_flags, forced_visible,
                !!(effective_flags & PDP_GEM_INVISIBLE), create->handle, ret,
                ret < 0 ? saved : 0);
    } else if (request == DRM_IOCTL_PDP_GEM_CPU_PREP_TRACE && arg) {
        struct drm_pdp_gem_cpu_prep_trace *prep = arg;
        uint32_t effective_flags = prep->flags;
        prep->flags = original_flags;
        dprintf(2, "[trace-loader] PDP_GEM_CPU_PREP fd=%d handle=%u flags=0x%x "
                "effective=0x%x emulated=%d read=%u write=%u nowait=%u "
                "ret=%d errno=%d\n", fd, prep->handle, original_flags,
                effective_flags, emulated,
                !!(original_flags & PDP_GEM_CPU_PREP_READ),
                !!(original_flags & PDP_GEM_CPU_PREP_WRITE),
                !!(original_flags & PDP_GEM_CPU_PREP_NOWAIT), ret,
                ret < 0 ? saved : 0);
    } else if (request == DRM_IOCTL_PDP_GEM_CPU_FINI_TRACE && arg) {
        const struct drm_pdp_gem_cpu_fini_trace *fini = arg;
        dprintf(2, "[trace-loader] PDP_GEM_CPU_FINI fd=%d handle=%u ret=%d "
                "errno=%d\n", fd, fini->handle, ret, ret < 0 ? saved : 0);
    } else if (request == DRM_IOCTL_PDP_GEM_INV_GET_TRACE && arg) {
        const struct drm_pdp_gem_inv_get_trace *inv = arg;
        dprintf(2, "[trace-loader] PDP_GEM_INV_GET fd=%d handle=%u "
                "invisible=%u ret=%d errno=%d\n", fd, inv->handle,
                inv->is_invisible, ret, ret < 0 ? saved : 0);
    } else if (ret < 0) {
        dprintf(2, "[trace-loader] ioctl(fd=%d, req=0x%lx) = %d errno=%d (%s)\n",
                fd, request, ret, saved, strerror(saved));
    }
    errno = saved;
    return ret;
}
