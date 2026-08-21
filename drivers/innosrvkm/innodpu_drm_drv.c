/*************************************************************************/ /*!
@File			innodpu_drm_drv.c
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
#include <linux/platform_device.h>
#include "syscommon.h"
#include "innodpu_drm_gem.h"
#include "innodpu_drm_drv.h"
#include "innodpu_drm_fb.h"
#include "innodpu_common.h"
#include "innodpu_dp_common.h"
#include "innodpu_connector.h"
#include "innodpu_drm_pm.h"
#include "pdp_drm.h"
#include "innogpu_drm.h"
#include "kernel_compatibility.h"

#define SWAP_SHMEM_ZEO
#define DPU_BACKUP_ALL_VRAM

/*
 * g0m/g3 pdp combine function, must set during driver installation
 * bit-0 : pdp0 and pdp1 combine, s_dpu_nums[1:0]=2'b01
 * bit-1 : pdp2 and pdp3 combine, s_dpu_nums[3:2]=2'b01
 * bit-2 : pdp2 and pdp3 dual link, s_dpu_nums[3:2]=2'b01
 * bit-3 : pdp4 and pdp5 combine, s_dpu_nums[4:5]=2'b01
 */
unsigned int s_combi_dual_sel = 0x00;
module_param(s_combi_dual_sel, uint, 0444);
MODULE_PARM_DESC(s_combi_dual_sel, "dpu combine driver register(default: 0x00)");

unsigned int s_dpu_nums = 0xf;
module_param(s_dpu_nums, uint, 0444);
MODULE_PARM_DESC(s_dpu_nums, "dpu driver register(default: 0x0f)");

unsigned int s_pmbus_nums = 0x1f;
module_param(s_pmbus_nums, uint, 0600);
MODULE_PARM_DESC(s_pmbus_nums,"pmbus driver register(default: 3)");

unsigned int s_hdmi_nums = 0x3;
module_param(s_hdmi_nums, uint, 0444);
MODULE_PARM_DESC(s_hdmi_nums,"hdmi driver register(default: 3)");

unsigned int s_dp_nums = 1;
module_param(s_dp_nums, uint, 0444);
MODULE_PARM_DESC(s_dp_nums, "dp driver register(default: 1)");

unsigned int s_lvds_nums = 1;
module_param(s_lvds_nums, uint, 0444);
MODULE_PARM_DESC(s_lvds_nums, "lvds driver register(default: 1)");

unsigned int s_vga_nums = 1;
module_param(s_vga_nums, uint, 0444);
MODULE_PARM_DESC(s_vga_nums, "vga driver register(default: 0)");

unsigned int s_logo_extime = 0;
module_param(s_logo_extime, uint, 0644);
MODULE_PARM_DESC(s_logo_extime, "Controls the minimum execution"
			" time of processes displayed the logo(default: 0s)");

unsigned int s_hw_cursor = 0x00;
module_param(s_hw_cursor, uint, 0444);
MODULE_PARM_DESC(s_hw_cursor, "hw cursor driver register(default: 0x00)");

/*
+ * G0M
+ * 0-4K+4K
+ *
+ */
unsigned int s_dpu_match = 0x0;
module_param(s_dpu_match, uint, 0444);
MODULE_PARM_DESC(s_dpu_match, "dpu match register(default: 0x00)");

int innodpu_gem_dumb_create(struct drm_file *drm_file,
		struct drm_device *drm_dev, struct drm_mode_create_dumb *args)
{
	int ret = 0;
	uint32_t flags = 0;
	struct innodpu_drm_private *dev_priv = NULL;
	innodpu_shared_mem *pshared_mem = NULL;
	innodpu_mem_manager *mem_manager = NULL;
	innodpu_mem_positon pos = 0;
	innodpu_mem_class class = 0;
	bool visible = 0;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "innodpu dev_priv is NULL.\n");
		return -EINVAL;
	}

	flags = args->flags;
	visible = !((args->flags & DBM_GEM_INVISIBLE) >> 28);;
	pos = innodpu_gem_dbmflags_to_position(drm_dev, dev_priv, flags);
	class = innodpu_get_dmbflags_to_class(flags, pos);

	if (class == GTT && !dev_priv->has_gtt_mem) {
		gem_err(drm_dev->dev, "request gtt mem form dpu, but dpu not support\n");
		return -ENOMEM;
	} else if (!visible && !dev_priv->has_inv_mem) {
		gem_err(drm_dev->dev, "request invisible mem form dpu, but dpu not support\n");
		return -ENOMEM;
	}

	/* excute shmem first */
	if (pos == SHMEM_VRAM_POSITION) {
		pshared_mem = &dev_priv->shared_vram_info;
		if ((strncmp(current->comm, "Xorg", 4) == 0) || (strncmp(current->comm, "X", 1) == 0)) {
			innodpu_xorg_monitor_switch_user(drm_dev, pshared_mem);
		}
		if (pshared_mem->current_user) {
			ret = innodpu_gem_object_create(drm_file, drm_dev,
				pshared_mem->current_user->mem_manager, (void *)args, false);
			if (!ret) {
				return 0;
			}
		}
		pos = VRAM_POSITION;
	}

	if (pos == SYS_GTT_POSITION) {
		mem_manager = dev_priv->gtt_mem_manager;
	} else if (pos == VRAM_POSITION) {
		if (visible) {
			mem_manager = dev_priv->visible_mem_manager;
		} else {
			mem_manager = dev_priv->invisible_mem_manager;
		}
	}
#if defined(__G3_NE__) || defined(__G3_PAL__)
	args->flags = args->flags | DBM_GEM_NEED_CONTINUOUS;
#endif
	ret = innodpu_gem_object_create(drm_file, drm_dev, mem_manager, (void *)args, false);

	if (ret) {
		gem_err(drm_dev->dev, "Visible-%d Input: flags-%#x size-%llu error\n",
				mem_manager->visible, args->flags, args->size);
	}

	return ret;
}

int innodpu_gem_object_create_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file)
{
	int ret = 0;
	uint32_t flags = 0;
	struct drm_pdp_gem_create *args = data;
	struct innodpu_drm_private *dev_priv = NULL;
	innodpu_mem_manager *mem_manager = NULL;
	innodpu_shared_mem *pshared_mem = NULL;
	innodpu_mem_positon pos = 0;
	innodpu_mem_class class = 0;
	bool visible = 0;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "innodpu dev_priv is NULL.\n");
		return -EINVAL;
	}

	flags = args->flags;
	visible = !((args->flags & DBM_GEM_INVISIBLE) >> 28);
	pos = innodpu_gem_dbmflags_to_position(drm_dev, dev_priv, flags);
	class = innodpu_get_dmbflags_to_class(flags, pos);

	if (class == GTT && !dev_priv->has_gtt_mem) {
		gem_err(drm_dev->dev, "request gtt mem form dpu, but dpu not support\n");
		return -ENOMEM;
	} else if (!visible && !dev_priv->has_inv_mem) {
		gem_err(drm_dev->dev, "request invisible mem form dpu, but dpu not support\n");
		return -ENOMEM;
	}

	/* excute shmem first */
	if (pos == SHMEM_VRAM_POSITION) {
		pshared_mem = &dev_priv->shared_vram_info;
		if ((strncmp(current->comm, "Xorg", 4)==0) || (strncmp(current->comm, "X", 1)==0)) {
			innodpu_xorg_monitor_switch_user(drm_dev, pshared_mem);
		}
		if (pshared_mem->current_user) {
			ret = innodpu_gem_object_create(drm_file, drm_dev,
				pshared_mem->current_user->mem_manager, (void *)args, true);
			if (!ret) {
				return 0;
			}
		}
		pos = VRAM_POSITION;
	}

	if (pos == SYS_GTT_POSITION) {
		mem_manager = dev_priv->gtt_mem_manager;
	} else if (pos == VRAM_POSITION) {
		if (visible) {
			mem_manager = dev_priv->visible_mem_manager;
		} else {
			mem_manager = dev_priv->invisible_mem_manager;
		}
	}
#if defined(__G3_NE__) || defined(__G3_PAL__)
	args->flags = args->flags | DBM_GEM_NEED_CONTINUOUS;
#endif
	ret = innodpu_gem_object_create(drm_file, drm_dev, mem_manager, (void *)args, true);
	if (ret) {
		gem_err(drm_dev->dev, "Visible-%d Input: flags-%#x size-%llu error\n",
				mem_manager->visible, args->flags, args->size);
	}

	return ret;
}

int innodpu_drm_chipinfo_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file)
{
	struct drm_pdp_chip_info *args = data;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "innodpu dev_priv is NULL.\n");
		return -EINVAL;
	}

	args->chip_type = fh2m_hal_get_chiptype(drm_dev->dev);
	args->support_inv = fh2m_hal_has_inv_mem(drm_dev->dev);
	args->support_shmem = dev_priv->has_shared_mem;

	return 0;
}

int innodpu_drm_gem_addr_ioctl(struct drm_device *drm_dev, void *data,
	struct drm_file *drm_file)
{
	struct drm_pdp_gem_addr *args = (struct drm_pdp_gem_addr*)data;
	struct drm_gem_object *obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	uint64_t addr = 0;

	obj = fh2m_inno_drm_gem_object_lookup(drm_file, args->handle);
	if (obj == NULL) {
		gem_err(drm_dev->dev, "find gem_obj by handle[%d] failed\n", args->handle);
		return -EINVAL;
	}
	innodpu_obj = to_innodpu_obj(obj);

	if (args->addr_type == DRM_DEV_PADDR) {
		addr = fh2m_innodpu_gem_get_dev_paddr(obj);
	} else if (args->addr_type == DRM_DEV_VADDR) {
		addr = innodpu_gem_get_dev_vaddr(obj);
	}
	fh2m_inno_drm_gem_object_put(obj);

	if (addr) {
		args->addr = addr;
		return 0;
	} else {
		gem_err(drm_dev->dev, "invalid handle: %d, invalid addr-type: %d\n",
				args->handle, args->addr_type);
	}

	return -EINVAL;;
}

int inno_gem_object_mmap_ioctl(struct drm_device *drm_dev, void *data,
		struct drm_file *drm_file)
{
	struct drm_pdp_gem_mmap *args = (struct drm_pdp_gem_mmap *)data;

	return drm_gem_dumb_map_offset(drm_file, drm_dev, args->handle, &args->offset);
}

int inno_gem_object_plane_fd_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	struct innodpu_drm_private *dev_priv = NULL;
	struct drm_pdp_base_fd *args = (struct drm_pdp_base_fd *)data;
	struct drm_plane *plane = NULL;
	struct drm_crtc *crtc = NULL;
	struct drm_gem_object *obj = NULL;
#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
	struct inno_framebuffer *innofb = NULL;
#endif
	struct drm_framebuffer *bfb = NULL;
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_display_mode mode;
	unsigned int vblank_us = 16700;
	int ret = -EINVAL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		dpu_err(drm_dev->dev, "innodpu dev_priv is NULL.\n");
		return -1;
	}

	if (!s_dpu_support_plane_fd || !dev_priv->drm_nulldisplay) {
		return 0;
	}
	if (args == NULL) {
		return -1;
	}

	drm_for_each_plane(plane, drm_dev) {
		if (plane->base.id == args->plane_id) {
			ret = 0;
			break;
		}
	}
	if (ret) {
		dpu_err(drm_dev->dev, "Not found plane id %d\n", args->plane_id);
		return ret;
	}

	memset(&mode, 0, sizeof(mode));
	if (plane->state && plane->state->crtc) {
		crtc = plane->state->crtc;
		mode = crtc->state->mode;
	} else if (!plane->state && plane->crtc) {
		crtc = plane->crtc;
		mode = crtc->mode;
	} else {
		dpu_err(drm_dev->dev, "Plane %d not crtc\n", args->plane_id);
		return -EIO;
	}

	/* calc wait time us, by frame_id and frame rate */
	pdp0_drm = crtc_to_pdp0_device(crtc);
	if (mode.clock) {
		vblank_us = 1000000 / drm_mode_vrefresh(&mode);
	}

	if (args->frame_id > atomic64_read(&pdp0_drm->active_count)) {
		vblank_us *= (atomic64_read(&pdp0_drm->active_count) - args->frame_id);
	} else {
		/* at lease wait next active region comming */
		args->frame_id = atomic64_read(&pdp0_drm->active_count) + 1;
	}

	/* wait target active region access */
	pdp0_drm = crtc_to_pdp0_device(crtc);
	if (pdp0_drm->active_wq != NULL) {
		ret = inno_wait_event_interruptible_timeout(pdp0_drm->active_wq,
			atomic64_read(&pdp0_drm->active_count) >= args->frame_id,
			usecs_to_jiffies(vblank_us + 100000)); /* add 100ms as margin */
		if (ret < 0) {
			dpu_err(drm_dev->dev, "wait %d img failed\n", args->plane_id);
			return ret;
		} else if (ret == 0) {
			dpu_err(drm_dev->dev, "wait %d img valid timeout\n", args->plane_id);
			return -ETIMEDOUT;
		}
	}

	/* get bfb here */
	if (plane->state && plane->state->fb) {
		bfb = plane->state->fb;
	} else if (!plane->state && plane->fb) {
		bfb = plane->fb;
	} else {
		dpu_err(drm_dev->dev, "Plane %d has not fb\n", args->plane_id);
		return -EIO;
	}

	ret = 0;

	/* plane obj */
#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
	innofb = to_inno_framebuffer(bfb);
	obj = innofb->obj[0];
#else
	obj = bfb->obj[0];
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	args->format = bfb->format->format;
#else
	args->format = bfb->pixel_format;
#endif
	args->frame_size = obj->size;
	args->width = bfb->width;
	args->height = bfb->height;
	args->obj_idr = obj->name;
	args->frame_id = atomic64_read(&pdp0_drm->active_count);

	/* todo: support cursor_obj */
	args->cursor_obj_idr = 0;
	args->cursor_x = 0;
	args->cursor_y = 0;
	args->cursor_width = 0;
	args->cursor_height = 0;
	args->cursor_format = 0;

	if (args->obj_idr <= 0) {
		dpu_err(drm_dev->dev, "unknow err of get plane[%d] obj_name[%d]\n",
				args->plane_id, args->obj_idr);
	}

	return ret;
}


#ifndef CONFIG_VIDEOMODE_HELPERS
/**
 * drm_display_mode_to_videomode - fill in @vm using @dmode,
 * @dmode: drm_display_mode structure to use as source
 * @vm: videomode structure to use as destination
 *
 * Fills out @vm using the display mode specified in @dmode.
 */
void drm_display_mode_to_videomode(
		const struct drm_display_mode *dmode,struct videomode *vm)
{
	vm->hactive = dmode->hdisplay;
	vm->hfront_porch = dmode->hsync_start - dmode->hdisplay;
	vm->hsync_len = dmode->hsync_end - dmode->hsync_start;
	vm->hback_porch = dmode->htotal - dmode->hsync_end;

	vm->vactive = dmode->vdisplay;
	vm->vfront_porch = dmode->vsync_start - dmode->vdisplay;
	vm->vsync_len = dmode->vsync_end - dmode->vsync_start;
	vm->vback_porch = dmode->vtotal - dmode->vsync_end;

	vm->pixelclock = dmode->clock * 1000;

	vm->flags = 0;
	if (dmode->flags & DRM_MODE_FLAG_PHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_HIGH;
	else if (dmode->flags & DRM_MODE_FLAG_NHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_LOW;
	if (dmode->flags & DRM_MODE_FLAG_PVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_HIGH;
	else if (dmode->flags & DRM_MODE_FLAG_NVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_LOW;
	if (dmode->flags & DRM_MODE_FLAG_INTERLACE)
		vm->flags |= DISPLAY_FLAGS_INTERLACED;
	if (dmode->flags & DRM_MODE_FLAG_DBLSCAN)
		vm->flags |= DISPLAY_FLAGS_DOUBLESCAN;
	if (dmode->flags & DRM_MODE_FLAG_DBLCLK)
		vm->flags |= DISPLAY_FLAGS_DOUBLECLK;
}
#endif

int innodpu_fb_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	int ret = 0;

	if (!dev_priv->drm_nulldisplay) {
		inno_drm_info(drm_dev->dev, "Start init fbdev device");
		ret = innodpu_fbdev_init(drm_dev);
		if (ret) {
			inno_drm_err(drm_dev->dev, "Failed to Init fbdev(%d)", ret);
			return ret;
		}
	}

	return 0;
}

void innodpu_fb_fini(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	if (!dev_priv->drm_nulldisplay) {
		innodpu_fbdev_fini(drm_dev);
	}
}

int innodpu_pm_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	int ret = 0;

	if (!dev_priv->drm_nulldisplay) {
		dev_priv->pm_nb.notifier_call = innodpu_pm_notifier;
		ret = register_pm_notifier(&dev_priv->pm_nb);
	}

	return ret;
}

void innodpu_pm_fini(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	if (!dev_priv->drm_nulldisplay) {
		unregister_pm_notifier(&dev_priv->pm_nb);
	}
}

static int innodpu_chip_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	return innodpu_custom_init(drm_dev, dev_priv);
}

int innodpu_early_load(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, struct device *dev)
{
	int err = 0;
	dev_priv->dev = dev;
	dev_priv->drm_dev = drm_dev;
	dev_priv->display_enabled = true;
	dev_priv->has_shared_mem = false;
	dev_priv->has_gtt_mem = false;
	dev_priv->has_pdp_vga_mem = false;
	dev_priv->has_uncontinuous_mem = false;
	dev_priv->logo_end_ktime = 0;

	inno_drm_info(dev, "Start early load innosilicon drm device");

	/* nulldisp info */
	err  = innogpu_drm_get_nulldisp(drm_dev, &dev_priv->drm_nulldisplay);
	if (err < 0) {
		inno_drm_err(dev, "Get nulldisp failed(%d)", err);
		return err;
	}

	/* sharemem info */
	if (!dev_priv->drm_nulldisplay && s_dpu_has_shared_mem) {
		dev_priv->has_shared_mem = true;
		dev_priv->shared_vram_info.size = s_dpu_shared_mem_size;
	}

	/* inv info */
	err = fh2m_hal_has_inv_mem(drm_dev->dev);
	if (err) {
		dev_priv->has_inv_mem = true;
	}

	/* gtt info */
	err = fh2m_hal_has_gtt_mem(drm_dev->dev);
	if (err && s_dpu_has_gtt_mem) {
		dev_priv->has_gtt_mem = true;
	}

	/* no continuous vram */
	if (s_dpu_has_nocontinuous_vram) {
		dev_priv->has_uncontinuous_mem = true;
	}

	/* pdp for vga info */
	if (s_vga_auto_adapt && !dev_priv->drm_nulldisplay) {
		dev_priv->has_pdp_vga_mem = true;
	}

	mutex_init(&dev_priv->init_lock);
	mutex_init(&dev_priv->logo_lock);

	err = innodpu_modeset_early_init(drm_dev, dev_priv);
	if (err) {
		inno_drm_err(dev, "Early modeset initialisation failed(%d)", err);
		goto err_modeset_early_init;
	}

	innodpu_chip_init(drm_dev, dev_priv);

	inno_drm_info(dev, "has_shared_mem = %d, has_inv_mem = %d",
					  dev_priv->has_shared_mem, dev_priv->has_inv_mem);
	inno_drm_info(dev, "hal_gtt_mem = %d, has_uncontinuous_mem = %d, has_pdp_vga_mem = %d",
						dev_priv->has_gtt_mem, dev_priv->has_uncontinuous_mem,
						dev_priv->has_pdp_vga_mem);
	inno_drm_info(dev, "Early modeset initialization completed");

	return 0;

err_modeset_early_init:
	return err;
}

void innodpu_early_unload(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
	/* Called by drm_dev_fini in Linux 4.11.0 and later */
	drm_vblank_cleanup(drm_dev);
#endif
	innodpu_custom_fini(dev_priv);
}

static void innodpu_vram_fini(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	if (dev_priv->zero_gem)
		innodpu_zero_mem_fini(drm_dev, dev_priv->zero_gem);
	if (dev_priv->pdp_vga_gem && dev_priv->has_pdp_vga_mem)
		innodpu_pdp_vga_mem_fini(drm_dev, dev_priv->pdp_vga_gem);
	if (dev_priv->visible_mem_manager)
		innodpu_mem_manager_fini(drm_dev, dev_priv->visible_mem_manager);
	if (dev_priv->invisible_mem_manager && dev_priv->has_inv_mem)
		innodpu_mem_manager_fini(drm_dev, dev_priv->invisible_mem_manager);
	if (dev_priv->gtt_mem_manager && dev_priv->has_gtt_mem)
		innodpu_mem_manager_fini(drm_dev, dev_priv->gtt_mem_manager);

	if (dev_priv->shared_vram_info.dev_paddr != 0 && dev_priv->has_shared_mem) {
		idr_destroy(&dev_priv->shared_vram_info.uer_idr);
		innodpu_gem_share_pre_free(drm_dev, &dev_priv->shared_vram_info);
	}
}

static int innodpu_vram_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	int err = 0;

	gem_info(drm_dev->dev, "vram init, display mode is %s, shmem is %s",
			 dev_priv->drm_nulldisplay ? "nulldisplay" : "display",
			 s_dpu_has_shared_mem ? "enabled" : "disabled");

	dev_priv->role.vram_role = HAL_VRAM_ROLE_DPU;
	dev_priv->role.id = dev_priv->role_id;
	dev_priv->role.sub_id = 0;

	/* zero vram init */
	dev_priv->zero_gem = innodpu_zero_mem_init(drm_dev, GEM_ZERO_SIZE);
	if (!dev_priv->zero_gem) {
		gem_err(drm_dev->dev, "Inno zero mem init failed. Short of memory");
		err = -ENOMEM;
		goto err_gem_init;
	}

	/* pdp vga mem init */
	if (dev_priv->has_pdp_vga_mem) {
		dev_priv->pdp_vga_gem = innodpu_pdp_vga_mem_init(drm_dev);
		if (!dev_priv->pdp_vga_gem) {
			gem_err(drm_dev->dev, "Inno pdp vga gem init failed. Short of memory");
			err = -ENOMEM;
			goto err_gem_init;
		}
	}

	/* visible mem */
	dev_priv->visible_mem_manager = innodpu_mem_manager_init(drm_dev, true, VRAM_POSITION, NULL);
	if (!dev_priv->visible_mem_manager) {
		gem_err(drm_dev->dev, "Inno visible gem init failed. Short of memory");
		err = -ENOMEM;
		goto err_gem_init;
	}

	/* invisible mem */
	if (dev_priv->has_inv_mem) {
		dev_priv->invisible_mem_manager = innodpu_mem_manager_init(drm_dev, false, VRAM_POSITION, NULL);
		if (!dev_priv->invisible_mem_manager) {
			gem_err(drm_dev->dev, "Inno invisible gem init failed. Short of memory");
			err = -ENOMEM;
			goto err_gem_init;
		}
	}

	/* gtt mem init */
	if (dev_priv->has_gtt_mem) {
		dev_priv->gtt_mem_manager = innodpu_mem_manager_init(drm_dev, true, SYS_GTT_POSITION, NULL);
		if (!dev_priv->gtt_mem_manager) {
			gem_err(drm_dev->dev, "Inno gtt gem init failed. Short of memory");
			err = - EINVAL;
			goto err_gem_init;
		}
	}

	/* shared mem init */
	if (dev_priv->has_shared_mem) {

		if (dev_priv->has_inv_mem) {
			dev_priv->shared_vram_info.is_visible = false;
		}

		err = innodpu_gem_share_pre_alloc(drm_dev, &dev_priv->shared_vram_info,
					s_dpu_shared_mem_size / fh2m_hal_get_gpu_core_nums());
		if (err < 0) {
			goto err_gem_init;
		}

		idr_init_base(&dev_priv->shared_vram_info.uer_idr, 0);
		spin_lock_init(&dev_priv->shared_vram_info.user_idr_lock);
		dev_priv->shared_vram_info.current_user = NULL;
	}

	return err;

err_gem_init:
	innodpu_vram_fini(drm_dev, dev_priv);

	return err;
}

int innodpu_late_load(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	int err = 0;

	inno_drm_info(drm_dev->dev, "Start late load innosilicon drm device");

	err = innodpu_vram_init(drm_dev, dev_priv);
	if (err < 0) {
		inno_drm_err(drm_dev->dev, "inno vram init failed(%d)", err);
		goto err_vram_init;
	}

	// drm_vblank_init：vblank自动释放 (drm_vblank_init_release)
	err = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (err < 0) {
		inno_drm_err(drm_dev->dev, "Failed to complete vblank init(%d)", err);
		goto err_vblank_init;
	}
#if (DRM_VERSION < KERNEL_VERSION(4, 7, 0))
	drm_dev->vblank_disable_allowed = 1;
#endif
	inno_drm_info(drm_dev->dev, "drm vblank init with crtc nums = %d, connector = %d",
				  drm_dev->mode_config.num_crtc, drm_dev->mode_config.num_connector);

	err = innodpu_modeset_late_init(drm_dev, dev_priv);
	if (err) {
		inno_drm_err(drm_dev->dev, "late modeset initialisation failed(%d)", err);
		goto err_modeset_late_init;
	}

	return err;

err_modeset_late_init:
#if (DRM_VERSION < KERNEL_VERSION(4, 7, 0))
	drm_dev->vblank_disable_allowed = 0;
#endif

err_vblank_init:
	innodpu_vram_fini(drm_dev, dev_priv);

err_vram_init:
	return err;
}

void innodpu_late_unload(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	drm_kms_helper_poll_fini(drm_dev);
	drm_mode_config_cleanup(drm_dev);
	innodpu_vram_fini(drm_dev, dev_priv);
}

void innodpu_bind_atomic(struct drm_device * drm_dev, bool lock)
{
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "innodpu_drm_private does not find");
		return;
	}

	if (lock) {
		mutex_lock(&dev_priv->init_lock);
	} else {
		mutex_unlock(&dev_priv->init_lock);
	}
}
