/*************************************************************************/ /*!
@File			innodpu_drm_drv.h
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
#ifndef __INNODPU_DRM_DRV_H
#define __INNODPU_DRM_DRV_H
#include "innogpu_drm_init.h"
#include "innodpu_common.h"
#include "innodpu_drm_modeset.h"
#include "innodpu_drm_gem.h"
#include "innodpu_drm_debugfs.h"
#include "innodpu_compatibility.h"

#ifndef CONFIG_VIDEOMODE_HELPERS
#include <linux/fb.h>
extern void drm_display_mode_to_videomode(
		const struct drm_display_mode *dmode,struct videomode *vm);
#endif //CONFIG_VIDEOMODE_HELPERS


extern int innodpu_early_load(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, struct device *dev);

extern void innodpu_early_unload(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern int innodpu_late_load(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern void innodpu_late_unload(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern void innodpu_poll_logo_execute(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, int nr);

extern void innodpu_bind_atomic(struct drm_device * drm_dev, bool lock);

extern int innodpu_gem_dumb_create(struct drm_file *drm_file,
			struct drm_device *drm_dev, struct drm_mode_create_dumb *args);

extern void innodpu_bind_atomic(struct drm_device * drm_dev, bool lock);

extern int innodpu_fb_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern void innodpu_fb_fini(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern int innodpu_pm_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern void innodpu_pm_fini(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv);

extern int inno_gem_object_plane_fd_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file);

extern int innodpu_drm_chipinfo_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file);

extern int innodpu_gem_object_create_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file);

extern int inno_gem_object_mmap_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file);

extern int innodpu_drm_gem_addr_ioctl(struct drm_device *drm_dev, void *data,
	struct drm_file *drm_file);

#endif //__INNODPU_DRM_DRV_H
