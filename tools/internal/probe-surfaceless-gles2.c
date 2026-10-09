#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_NO_ERROR 0

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLubyte;
typedef float GLfloat;

typedef EGLDisplay (*egl_get_platform_display_ext_fn)(EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
typedef const GLubyte *(*gl_get_string_fn)(GLenum name);
typedef void (*gl_clear_color_fn)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (*gl_clear_fn)(GLbitfield mask);
typedef GLenum (*gl_get_error_fn)(void);

static void *require_symbol(void *lib, const char *name)
{
    void *sym = dlsym(lib, name);
    if (!sym) {
        fprintf(stderr, "missing symbol %s: %s\n", name, dlerror());
    }
    return sym;
}

int main(void)
{
    void *egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!egl) {
        fprintf(stderr, "dlopen libEGL.so.1 failed: %s\n", dlerror());
        return 1;
    }

    PFNEGLGETPROCADDRESSPROC eglGetProcAddress = require_symbol(egl, "eglGetProcAddress");
    PFNEGLGETERRORPROC eglGetError = require_symbol(egl, "eglGetError");
    PFNEGLINITIALIZEPROC eglInitialize = require_symbol(egl, "eglInitialize");
    PFNEGLBINDAPIPROC eglBindAPI = require_symbol(egl, "eglBindAPI");
    PFNEGLCHOOSECONFIGPROC eglChooseConfig = require_symbol(egl, "eglChooseConfig");
    PFNEGLCREATECONTEXTPROC eglCreateContext = require_symbol(egl, "eglCreateContext");
    PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface = require_symbol(egl, "eglCreatePbufferSurface");
    PFNEGLMAKECURRENTPROC eglMakeCurrent = require_symbol(egl, "eglMakeCurrent");
    PFNEGLQUERYSTRINGPROC eglQueryString = require_symbol(egl, "eglQueryString");
    PFNEGLDESTROYSURFACEPROC eglDestroySurface = require_symbol(egl, "eglDestroySurface");
    PFNEGLDESTROYCONTEXTPROC eglDestroyContext = require_symbol(egl, "eglDestroyContext");
    PFNEGLTERMINATEPROC eglTerminate = require_symbol(egl, "eglTerminate");
    if (!eglGetProcAddress || !eglGetError || !eglInitialize || !eglBindAPI ||
        !eglChooseConfig || !eglCreateContext || !eglCreatePbufferSurface ||
        !eglMakeCurrent || !eglQueryString || !eglDestroySurface ||
        !eglDestroyContext || !eglTerminate) {
        return 1;
    }

    egl_get_platform_display_ext_fn eglGetPlatformDisplayEXT =
        (egl_get_platform_display_ext_fn)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        fprintf(stderr, "eglGetPlatformDisplayEXT unavailable\n");
        return 1;
    }

    EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetPlatformDisplayEXT(surfaceless) failed: 0x%x\n", eglGetError());
        return 1;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
        return 1;
    }
    printf("EGL initialized: %d.%d\n", major, minor);
    printf("EGL vendor: %s\n", eglQueryString(dpy, EGL_VENDOR));
    printf("EGL version: %s\n", eglQueryString(dpy, EGL_VERSION));

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "eglBindAPI(GLES) failed: 0x%x\n", eglGetError());
        eglTerminate(dpy);
        return 1;
    }

    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE
    };
    EGLConfig config = NULL;
    EGLint num_configs = 0;
    if (!eglChooseConfig(dpy, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        fprintf(stderr, "eglChooseConfig failed: 0x%x configs=%d\n", eglGetError(), num_configs);
        eglTerminate(dpy);
        return 1;
    }

    EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed: 0x%x\n", eglGetError());
        eglTerminate(dpy);
        return 1;
    }

    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(dpy, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreatePbufferSurface failed: 0x%x\n", eglGetError());
        eglDestroyContext(dpy, ctx);
        eglTerminate(dpy);
        return 1;
    }

    if (!eglMakeCurrent(dpy, surface, surface, ctx)) {
        fprintf(stderr, "eglMakeCurrent pbuffer failed: 0x%x\n", eglGetError());
        eglDestroySurface(dpy, surface);
        eglDestroyContext(dpy, ctx);
        eglTerminate(dpy);
        return 1;
    }

    gl_get_string_fn glGetString = (gl_get_string_fn)eglGetProcAddress("glGetString");
    gl_clear_color_fn glClearColor = (gl_clear_color_fn)eglGetProcAddress("glClearColor");
    gl_clear_fn glClear = (gl_clear_fn)eglGetProcAddress("glClear");
    gl_get_error_fn glGetError = (gl_get_error_fn)eglGetProcAddress("glGetError");
    if (!glGetString || !glClearColor || !glClear || !glGetError) {
        fprintf(stderr, "missing GLES2 symbols\n");
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(dpy, surface);
        eglDestroyContext(dpy, ctx);
        eglTerminate(dpy);
        return 1;
    }

    printf("GL vendor: %s\n", glGetString(GL_VENDOR));
    printf("GL renderer: %s\n", glGetString(GL_RENDERER));
    printf("GL version: %s\n", glGetString(GL_VERSION));
    glClearColor(0.0f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLenum err = glGetError();
    printf("glClear error: 0x%x\n", err);

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surface);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    dlclose(egl);

    if (err != GL_NO_ERROR) {
        return 1;
    }
    return 0;
}
