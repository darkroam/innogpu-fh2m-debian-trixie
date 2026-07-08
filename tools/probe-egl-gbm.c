#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct gbm_device;

typedef struct gbm_device *(*gbm_create_device_fn)(int fd);
typedef void (*gbm_device_destroy_fn)(struct gbm_device *gbm);
typedef const char *(*gbm_device_get_backend_name_fn)(struct gbm_device *gbm);
typedef EGLDisplay (*egl_get_platform_display_ext_fn)(EGLenum platform, void *native_display, const EGLAttrib *attrib_list);

#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_NO_ERROR 0

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLubyte;
typedef float GLfloat;

typedef const GLubyte *(*gl_get_string_fn)(GLenum name);
typedef void (*gl_clear_color_fn)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (*gl_clear_fn)(GLbitfield mask);
typedef GLenum (*gl_get_error_fn)(void);

static int probe_node(const char *node)
{
    printf("== node %s ==\n", node);

    int fd = open(node, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("open failed: errno=%d (%s)\n", errno, strerror(errno));
        return 1;
    }
    printf("open ok: fd=%d\n", fd);

    void *gbm_lib = dlopen("libgbm.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!gbm_lib) {
        printf("dlopen libgbm failed: %s\n", dlerror());
        close(fd);
        return 1;
    }

    gbm_create_device_fn gbm_create_device = (gbm_create_device_fn)dlsym(gbm_lib, "gbm_create_device");
    gbm_device_destroy_fn gbm_device_destroy = (gbm_device_destroy_fn)dlsym(gbm_lib, "gbm_device_destroy");
    gbm_device_get_backend_name_fn gbm_device_get_backend_name =
        (gbm_device_get_backend_name_fn)dlsym(gbm_lib, "gbm_device_get_backend_name");
    if (!gbm_create_device || !gbm_device_destroy) {
        printf("missing gbm symbols\n");
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    struct gbm_device *gbm = gbm_create_device(fd);
    if (!gbm) {
        printf("gbm_create_device failed\n");
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }
    printf("gbm_create_device ok: %p\n", (void *)gbm);
    if (gbm_device_get_backend_name) {
        const char *name = gbm_device_get_backend_name(gbm);
        printf("gbm backend: %s\n", name ? name : "(null)");
    }

    void *egl_lib = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!egl_lib) {
        printf("dlopen libEGL failed: %s\n", dlerror());
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    PFNEGLGETPROCADDRESSPROC egl_get_proc_address =
        (PFNEGLGETPROCADDRESSPROC)dlsym(egl_lib, "eglGetProcAddress");
    PFNEGLINITIALIZEPROC egl_initialize = (PFNEGLINITIALIZEPROC)dlsym(egl_lib, "eglInitialize");
    PFNEGLBINDAPIPROC egl_bind_api = (PFNEGLBINDAPIPROC)dlsym(egl_lib, "eglBindAPI");
    PFNEGLCHOOSECONFIGPROC egl_choose_config = (PFNEGLCHOOSECONFIGPROC)dlsym(egl_lib, "eglChooseConfig");
    PFNEGLCREATECONTEXTPROC egl_create_context = (PFNEGLCREATECONTEXTPROC)dlsym(egl_lib, "eglCreateContext");
    PFNEGLCREATEPBUFFERSURFACEPROC egl_create_pbuffer_surface =
        (PFNEGLCREATEPBUFFERSURFACEPROC)dlsym(egl_lib, "eglCreatePbufferSurface");
    PFNEGLMAKECURRENTPROC egl_make_current = (PFNEGLMAKECURRENTPROC)dlsym(egl_lib, "eglMakeCurrent");
    PFNEGLGETERRORPROC egl_get_error = (PFNEGLGETERRORPROC)dlsym(egl_lib, "eglGetError");
    PFNEGLQUERYSTRINGPROC egl_query_string = (PFNEGLQUERYSTRINGPROC)dlsym(egl_lib, "eglQueryString");
    PFNEGLDESTROYSURFACEPROC egl_destroy_surface = (PFNEGLDESTROYSURFACEPROC)dlsym(egl_lib, "eglDestroySurface");
    PFNEGLDESTROYCONTEXTPROC egl_destroy_context = (PFNEGLDESTROYCONTEXTPROC)dlsym(egl_lib, "eglDestroyContext");
    PFNEGLTERMINATEPROC egl_terminate = (PFNEGLTERMINATEPROC)dlsym(egl_lib, "eglTerminate");
    PFNEGLGETDISPLAYPROC egl_get_display = (PFNEGLGETDISPLAYPROC)dlsym(egl_lib, "eglGetDisplay");
    if (!egl_get_proc_address || !egl_initialize || !egl_bind_api ||
        !egl_choose_config || !egl_create_context || !egl_create_pbuffer_surface ||
        !egl_make_current || !egl_get_error || !egl_query_string ||
        !egl_destroy_surface || !egl_destroy_context || !egl_terminate) {
        printf("missing EGL symbols\n");
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    egl_get_platform_display_ext_fn egl_get_platform_display_ext =
        (egl_get_platform_display_ext_fn)egl_get_proc_address("eglGetPlatformDisplayEXT");
    EGLDisplay dpy = EGL_NO_DISPLAY;
    if (egl_get_platform_display_ext) {
        dpy = egl_get_platform_display_ext(EGL_PLATFORM_GBM_KHR, gbm, NULL);
        printf("eglGetPlatformDisplayEXT(GBM) = %p\n", (void *)dpy);
    } else if (egl_get_display) {
        dpy = egl_get_display((EGLNativeDisplayType)gbm);
        printf("eglGetDisplay(gbm) = %p\n", (void *)dpy);
    } else {
        printf("no EGL display entrypoint\n");
    }

    if (dpy == EGL_NO_DISPLAY) {
        printf("EGL_NO_DISPLAY error=0x%x\n", egl_get_error());
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    EGLint major = 0, minor = 0;
    if (!egl_initialize(dpy, &major, &minor)) {
        printf("eglInitialize failed: error=0x%x\n", egl_get_error());
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    printf("eglInitialize ok: %d.%d\n", major, minor);
    printf("EGL vendor: %s\n", egl_query_string(dpy, EGL_VENDOR));
    printf("EGL version: %s\n", egl_query_string(dpy, EGL_VERSION));
    printf("EGL client APIs: %s\n", egl_query_string(dpy, EGL_CLIENT_APIS));

    if (!egl_bind_api(EGL_OPENGL_ES_API)) {
        printf("eglBindAPI(GLES) failed: error=0x%x\n", egl_get_error());
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE
    };
    EGLConfig config = NULL;
    EGLint num_configs = 0;
    if (!egl_choose_config(dpy, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        printf("eglChooseConfig failed: error=0x%x configs=%d\n", egl_get_error(), num_configs);
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = egl_create_context(dpy, config, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx == EGL_NO_CONTEXT) {
        printf("eglCreateContext failed: error=0x%x\n", egl_get_error());
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    EGLSurface surface = egl_create_pbuffer_surface(dpy, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        printf("eglCreatePbufferSurface failed: error=0x%x\n", egl_get_error());
        egl_destroy_context(dpy, ctx);
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    if (!egl_make_current(dpy, surface, surface, ctx)) {
        printf("eglMakeCurrent failed: error=0x%x\n", egl_get_error());
        egl_destroy_surface(dpy, surface);
        egl_destroy_context(dpy, ctx);
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    gl_get_string_fn gl_get_string = (gl_get_string_fn)egl_get_proc_address("glGetString");
    gl_clear_color_fn gl_clear_color = (gl_clear_color_fn)egl_get_proc_address("glClearColor");
    gl_clear_fn gl_clear = (gl_clear_fn)egl_get_proc_address("glClear");
    gl_get_error_fn gl_get_error = (gl_get_error_fn)egl_get_proc_address("glGetError");
    if (!gl_get_string || !gl_clear_color || !gl_clear || !gl_get_error) {
        printf("missing GLES2 symbols\n");
        egl_make_current(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        egl_destroy_surface(dpy, surface);
        egl_destroy_context(dpy, ctx);
        egl_terminate(dpy);
        dlclose(egl_lib);
        gbm_device_destroy(gbm);
        dlclose(gbm_lib);
        close(fd);
        return 1;
    }

    printf("GL vendor: %s\n", gl_get_string(GL_VENDOR));
    printf("GL renderer: %s\n", gl_get_string(GL_RENDERER));
    printf("GL version: %s\n", gl_get_string(GL_VERSION));
    gl_clear_color(0.0f, 0.5f, 0.25f, 1.0f);
    gl_clear(GL_COLOR_BUFFER_BIT);
    GLenum glerr = gl_get_error();
    printf("glClear error: 0x%x\n", glerr);

    egl_make_current(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl_destroy_surface(dpy, surface);
    egl_destroy_context(dpy, ctx);
    egl_terminate(dpy);

    dlclose(egl_lib);
    gbm_device_destroy(gbm);
    dlclose(gbm_lib);
    close(fd);
    return glerr == GL_NO_ERROR ? 0 : 1;
}

int main(int argc, char **argv)
{
    int failures = 0;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            failures += probe_node(argv[i]) != 0;
        }
    } else {
        failures += probe_node("/dev/dri/card0") != 0;
        failures += probe_node("/dev/dri/renderD128") != 0;
    }
    return failures ? 1 : 0;
}
