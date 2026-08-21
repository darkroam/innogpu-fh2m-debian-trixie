/*************************************************************************/ /*!
@File			innodpu_drm_fb.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#ifndef __INNODPU_DRM_FB_H
#define __INNODPU_DRM_FB_H

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/drm_framebuffer.h>
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
struct drm_gem_object;

struct inno_framebuffer {
	struct drm_framebuffer base;
	struct drm_gem_object *obj[1];
};

#define to_inno_framebuffer(fb) container_of(fb, struct inno_framebuffer, base)
#define to_drm_framebuffer(fb) (&(fb)->base)
#else
#define inno_framebuffer drm_framebuffer
#define to_inno_framebuffer(fb) (fb)
#define to_drm_framebuffer(fb) (fb)
#endif

#if defined(CONFIG_DRM_FBDEV_EMULATION)
struct inno_fbdev {
	struct drm_fb_helper helper;
	struct inno_framebuffer fb;
	struct innodpu_drm_private *priv;
	u8 preferred_bpp;
};
#endif

extern void inno_fbdev_destroy(struct inno_fbdev *inno_fbdev);

extern int innodpu_fbdev_init(struct drm_device *drm_dev);
extern void innodpu_fbdev_fini(struct drm_device *drm_dev);

extern struct drm_framebuffer *innodpu_fb_create(struct drm_device *dev, struct drm_file *file,
#if (DRM_VERSION >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && \
		  (DRM_VERSION >= KERNEL_VERSION(3, 18, 0)))
												 const
#endif
												 struct drm_mode_fb_cmd2 *mode_cmd);

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
struct drm_framebuffer *inno_framebuffer_create(struct drm_device *drm_dev, struct drm_file *file,
#if (DRM_VERSION >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && (DRM_VERSION >= KERNEL_VERSION(3, 18, 0)))
												const
#endif
												struct drm_mode_fb_cmd2 *mode_cmd);
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
#define drm_gem_fb_create(...) inno_framebuffer_create(__VA_ARGS__)
#endif

#endif //__INNODPU_DRM_FB_H
