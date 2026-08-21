/*************************************************************************/ /*!
@File			innogpu_drm.h
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
#ifndef __INNOGPU_DRM_H
#define __INNOGPU_DRM_H

#ifdef CONFIG_DRM_INNO_DPU
#include "pdp_drm.h"
#include "innodpu_common.h"
#include "innodpu_drm_pm.h"
#include "innodpu_drm_drv.h"
#else
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "inno_drm_version.h"
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/component.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 1, 0))
#include <drm/drm_probe_helper.h>
#endif
#include <drm/drm_drv.h>
#include <drm/drm_device.h>

#include "inno_drm.h"
#include "pvrversion.h"
#include "img_defs.h"
#include "hal.h"
#include "hal_interface.h"
#include "innogpu.h"
#include "inno_debug.h"
#endif

#include "pvr_drm.h"
#include "pvr_drv.h"
#include "pdp_drm.h"

#define INNOGPU_DRM_DRIVER_NAME "innogpu"
#define INNOGPU_DRM_DRIVER_DATE "20210625"
#define INNOGPU_DRM_DRIVER_DESC "Innosilicon Technologies Gpu Driver"
#define INNOGPU_DRM_DRIVER_AUTHOR "Innosilicon Technologies Ltd. <support@innosilicon.com.cn>"

#define INNOGPU_DRM_DRIVER_MAJ (2U)
#define INNOGPU_DRM_DRIVER_MIN (19U)

#define INNOGPU_DRM_DRIVER_BUILD PVRVERSION_BUILD

#define MMAP_THRESHOLD	(0x100000UL)

#if (DRM_VERSION >= KERNEL_VERSION(5, 2, 0))
#define DRIVER_PRIME 0
#endif
extern const struct file_operations s_innogpu_drm_fops;
struct innogpu_drm_match {
	int id;
	const char *name;
	struct device *subdev;
	void *pdev;
};

struct innogpu_drm_private {
	// common damain filed
	struct device *dev; // pcie dev;
	struct drm_device *drm_dev;
	struct dev_rsrc *pdev_rsrc;
	plat_data_t *drm_plat_data;

	// innosrmkm domain field
	bool drm_support_dmatrans;
	struct pvr_drm_private pvr_priv;

#ifdef CONFIG_DRM_INNO_DPU
	// display domain field
	bool drm_nulldisplay;
	bool drm_has_invmem;
#if (DRM_VERSION >= KERNEL_VERSION(5, 15, 0))
	bool irq_enabled;
#endif
	struct innodpu_drm_private display_priv;
#endif

	struct drm_common_info comm_info;
};

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
int drm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
			    u32 handle, u64 *offset);
#endif

struct drm_device * innogpu_get_drm_from_pdev(struct device *plat_dev);
void * innogpu_get_ddev_from_plat_dev(struct device * plat_dev);
int innogpu_drm_get_platdata(struct drm_device *drm_dev, plat_data_t **drm_plat_data);
void * innogpu_drm_to_pvr_private(struct drm_device *drm_dev);
extern struct platform_driver g_innogpu_pvrsrvkm_driver;

#ifdef CONFIG_DRM_INNO_DPU
extern int innogpu_drm_irq_enabled(struct drm_device *drm_dev, bool *enable);
int innogpu_drm_set_irq(struct drm_device *drm_dev, bool enable);
int innogpu_drm_get_nulldisp(struct drm_device *drm_dev, bool *nulldisp);
void * innogpu_drm_to_display_private(struct drm_device *drm_dev);

#if defined(__G3_NE__)
extern struct platform_driver g_inno_pmbus_driver;
#endif
extern struct platform_driver g_innogpu_hdmi_driver;
extern struct platform_driver g_innogpu_dp_driver;
extern struct platform_driver g_innogpu_vga_driver;
extern struct platform_driver g_innogpu_vkms_driver;
extern struct platform_driver g_innogpu_dpu_driver;
extern struct vm_operations_struct innodpu_gem_vm_ops;
extern struct drm_gem_object *
		innodpu_gem_prime_import(struct drm_device *drm_dev, struct dma_buf *dma_buf);

#if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
extern void *innodpu_gem_prime_vmap(struct drm_gem_object *obj);
extern void innodpu_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
extern struct reservation_object * innodpu_gem_prime_res_obj(struct drm_gem_object *gem_obj);
#endif
#if (DRM_VERSION < KERNEL_VERSION(5, 4, 0))
extern struct dma_buf *
		innodpu_gem_prime_export(struct drm_device *drm_dev, struct drm_gem_object *obj, int flags);
#else
extern struct dma_buf *
		innodpu_gem_prime_export(struct drm_gem_object *obj, int flags);
#endif
extern int inno_gem_object_cpu_prep_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
extern int inno_gem_object_cpu_fini_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
extern int inno_gem_object_dump_vram_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
extern int inno_gem_object_inv_get_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
#endif

#if !defined(NO_HARDWARE) && defined(CONFIG_DRM_INNO_SRVKM)
extern int innogpu_drm_init(void);
extern void innogpu_drm_exit(void);
#endif
extern int drm_pvr_srvkm_init(struct drm_device *dev, void *arg, struct drm_file *psDRMFile);

#endif//__INNOGPU_DRM_H
