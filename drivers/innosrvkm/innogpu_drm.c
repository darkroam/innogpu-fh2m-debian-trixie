/*************************************************************************/ /*!
@File			innogpu_drm.c
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
#include "hal_interface.h"
#include "inno_mm.h"
#include "innogpu_drm.h"
#include "pvr_sync_ioctl_drm.h"
#include "innodpu_drm_drv.h"
#include "innodpu_drm_pm.h"
#include "innodpu_common.h"
#include "innodpu_connector.h"
#include "module_common.h"
#include <linux/platform_device.h>
#include "pvr_drm.h"
#include "pvrsrv.h"
#include "innodpu_drm_debugfs.h"
#include "pdp0_crtc.h"
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
#include <drm/drm_ioctl.h>
#endif
#include "inno_debug.h"

/*
 * Casue pvr/auio/dpu merge as pvr.ko,and dpu depend on pmbus,
 * so when make split, insmod ko should follow this order:
 * innogpu.ko innodma.ko innopmbus.ko innosrv.ko;
 */

unsigned int s_dpu_driver_group = 0x0;
module_param(s_dpu_driver_group, uint, 0444);
MODULE_PARM_DESC(s_dpu_driver_group, "dpu sub drivers group select(default: 0)");

extern struct platform_driver g_innogpu_pvrsrvkm_driver;
#ifdef CONFIG_DRM_INNO_AUDIO
extern struct platform_driver g_innoaudio_playback_driver;
#endif

extern void innodpu_poll_logo_execute(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, int nr);

// sync from function innodpu_gem_object_create_ioctl
static int innogpu_drm_gem_create_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_mmap_ioctl
static int innogpu_drm_gem_mmap_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_cpu_prep_ioctl
static int innogpu_drm_gem_cpu_prep_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_cpu_fini_ioctl
static int innogpu_drm_gem_cpu_fini_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_dump_vram_ioctl
static int innogpu_drm_gem_dump_vram_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_inv_get_ioctl
static int innogpu_drm_gem_inv_get_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function inno_gem_object_plane_fd_ioctl
static int innogpu_drm_gem_plane_fd_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function innodpu_drm_chipinfo_ioctl
static int innogpu_drm_chipinfo_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

static int innogpu_drm_gem_addr_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

static int innogpu_drm_get_common_info(struct drm_device *drm_dev,
		void *data, struct drm_file *file);

// sync from function innodpu_component_bind
static int innogpu_drm_component_bind(struct device *dev);

// sync from function innodpu_component_unbind
static void innogpu_drm_component_unbind(struct device *dev);

#if (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
static int innogpu_drm_debugfs_init(struct drm_minor *minor);
#else
static void innogpu_drm_debugfs_init(struct drm_minor *minor);
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
void innogpu_drm_debugfs_cleanup(struct drm_minor *minor);
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
/**
 * drm_gem_dumb_map_offset - return the fake mmap offset for a gem object
 * @file: drm file-private structure containing the gem object
 * @dev: corresponding drm_device
 * @handle: gem object handle
 * @offset: return location for the fake mmap offset
 *
 * This implements the &drm_driver.dumb_map_offset kms driver callback for
 * drivers which use gem to manage their backing storage.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
			    u32 handle, u64 *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	/* Don't allow imported objects to be mapped */
	if (obj->import_attach) {
		ret = -EINVAL;
		goto out;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
out:
	drm_gem_object_put(obj);

	return ret;
}
#endif

#define SUB_DRIVER_GROUP_NUMBER   (8U)
static struct platform_driver *s_innogpu_drm_sub_drivers[][SUB_DRIVER_GROUP_NUMBER] = {
	{
		&g_innogpu_pvrsrvkm_driver,
#ifdef CONFIG_DRM_INNO_AUDIO
		&g_innoaudio_playback_driver,
#endif
#if defined(__G3_NE__)
		&g_inno_pmbus_driver,
#endif
		&g_innogpu_dp_driver,
		&g_innogpu_hdmi_driver,
		&g_innogpu_vga_driver,
		//&g_innogpu_lvds_driver,
		&g_innogpu_vkms_driver,
		&g_innogpu_dpu_driver,
	},

	{
		&g_innogpu_pvrsrvkm_driver,
#ifdef CONFIG_DRM_INNO_AUDIO
		&g_innoaudio_playback_driver,
#endif
		&g_innogpu_vga_driver,
		&g_innogpu_hdmi_driver,
		&g_innogpu_dp_driver,
		//&g_innogpu_lvds_driver,
		&g_innogpu_vkms_driver,
		&g_innogpu_dpu_driver,
	},
};

static const struct component_master_ops s_innogu_drm_component_ops = {
	.bind = innogpu_drm_component_bind,
	.unbind = innogpu_drm_component_unbind,
};

int drm_pvr_srvkm_init(struct drm_device *dev, void *arg, struct drm_file *psDRMFile)
{
	struct drm_pvr_srvkm_init_data *data = arg;
	struct pvr_drm_private *priv = innogpu_drm_to_pvr_private(dev);
	int iErr = 0;

	switch (data->init_module)
	{
		case PVR_SRVKM_SYNC_INIT:
		{
			iErr = PVRSRVDeviceSyncOpen(priv->dev_node, psDRMFile);
			break;
		}
		case PVR_SRVKM_SERVICES_INIT:
		{
			iErr = PVRSRVDeviceServicesOpen(priv->dev_node, psDRMFile);
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: invalid init_module (%d)",
			        __func__, data->init_module));
			iErr = -EINVAL;
		}
	}

	return iErr;
}

/*
 * private ioctls:
 * merge pvr_drm_ioctls and s_innodpu_ioctls
 */
static const struct drm_ioctl_desc s_innogpu_drm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(PVR_SRVKM_CMD, PVRSRV_BridgeDispatchKM,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SRVKM_INIT, drm_pvr_srvkm_init,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
#if defined(SUPPORT_NATIVE_FENCE_SYNC) && !defined(USE_PVRSYNC_DEVNODE)
	DRM_IOCTL_DEF_DRV(PVR_SYNC_RENAME_CMD, pvr_sync_rename_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SYNC_FORCE_SW_ONLY_CMD, pvr_sync_force_sw_only_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SW_SYNC_CREATE_FENCE_CMD, pvr_sw_sync_create_fence_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SW_SYNC_INC_CMD, pvr_sw_sync_inc_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SW_SYNC_CREATE_FENCE_WITHOUT_TIMELINE_CMD, pvr_sw_sync_create_fence_without_timeline_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PVR_SW_SYNC_SIGNAL_FENCE_CMD, pvr_sw_sync_signal_fence_ioctl,
			DRM_RENDER_ALLOW | INNOGPU_DRM_UNLOCKED | DRM_RENDER_ALLOW),
#endif

	DRM_IOCTL_DEF_DRV(PDP_GEM_CREATE, innogpu_drm_gem_create_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_MMAP, innogpu_drm_gem_mmap_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_PREP, innogpu_drm_gem_cpu_prep_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_FINI, innogpu_drm_gem_cpu_fini_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_GET, innogpu_drm_gem_dump_vram_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_INV_GET, innogpu_drm_gem_inv_get_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_BASE_FD, innogpu_drm_gem_plane_fd_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_CHIP_INFO, innogpu_drm_chipinfo_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(PDP_GEM_ADDR, innogpu_drm_gem_addr_ioctl,
			INNODPU_IOCTL_FLAGS),
	DRM_IOCTL_DEF_DRV(INNO_COMMON_INFO, innogpu_drm_get_common_info,
			INNODPU_IOCTL_FLAGS),
};

static long innogpu_drm_ioctl_check(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct drm_file *drm_file = filp->private_data;
	struct drm_device *drm_dev = drm_file->minor->dev;
	struct innodpu_drm_private *display_priv = NULL;

	display_priv = innogpu_drm_to_display_private(drm_dev);

	if (fh2m_hal_gpuchip_is_ovheat(drm_dev->dev)) {
		inno_drm_info(drm_dev->dev, "ovheat, no timing settings");
		return -EFAULT;
	}

	if (display_priv && drm_file->minor && (drm_file->minor->type == DRM_MINOR_PRIMARY))
		innodpu_poll_logo_execute(drm_dev, display_priv, DRM_IOCTL_NR(cmd));

	if (cmd == DRM_IOCTL_MODE_SETCRTC) {
		conn_info(drm_dev->dev, "[ioctl]: DRM_IOCTL_MODE_SETCRTC");
	}
	if (cmd == DRM_IOCTL_SET_MASTER) {
		conn_info(drm_dev->dev, "[ioctl]: DRM_IOCTL_SET_MASTER");
	}
	if (cmd == DRM_IOCTL_DROP_MASTER) {
		conn_info(drm_dev->dev, "[ioctl]: DRM_IOCTL_DROP_MASTER");
	}
	return 0;
}

static long innogpu_drm_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	ret = innogpu_drm_ioctl_check(filp, cmd, arg);
	if (ret >= 0)
		return drm_ioctl(filp, cmd, arg);

	return ret;
}

#ifdef CONFIG_COMPAT
static long innogpu_drm_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	ret = innogpu_drm_ioctl_check(filp, cmd, arg);
	if (ret >= 0)
		return drm_compat_ioctl(filp, cmd, arg);

	return ret;
}
#endif

/*
 *
 * Because a system tries to open DRM before the driver registration is complete.
 * it causes the sytem to crash.
 */
static int innogpu_drm_check_bind_finish(struct drm_device *drm_dev)
{
	innodpu_bind_atomic(drm_dev, true);
	innodpu_bind_atomic(drm_dev, false);
	return 0;
}

static int innogpu_pvr_drm_open(struct drm_device *drm_dev, struct drm_file *dfile)
{
#if (PVRSRV_DEVICE_INIT_MODE != PVRSRV_LINUX_DEV_INIT_ON_CONNECT) || defined(__INNO_CONTAINER__)
	struct pvr_drm_private *priv = innogpu_drm_to_pvr_private(drm_dev);
	int err = 0;
#endif

#if (PVRSRV_DEVICE_INIT_MODE != PVRSRV_LINUX_DEV_INIT_ON_CONNECT) || defined(__INNO_CONTAINER__)
	err = PVRSRVDeviceServicesOpen(priv->dev_node, dfile);

	return err;
#else
	return 0;
#endif
}

static void innogpu_pvr_drm_release(struct drm_device *drm_dev, struct drm_file *dfile)
{
	struct pvr_drm_private *priv = drm_dev->dev_private;

	PVRSRVDeviceRelease(priv->dev_node, dfile);
}

static int innogpu_drm_file_open (struct drm_device *drm_dev,
		struct drm_file *drm_file)
{
	int ret = 0;

	/*
	 * Xorg starts opening the DRM device before fbdev initialization is complete,
	 * leading to kernel panic.
	 */
	ret = innogpu_drm_check_bind_finish(drm_dev);
	ret = innogpu_pvr_drm_open(drm_dev, drm_file);

	return ret;
}

static void innogpu_drm_file_release (struct drm_device *drm_dev,
		struct drm_file *drm_file)
{
	innogpu_pvr_drm_release(drm_dev,drm_file);

	return;
}

static bool innogpu_pvrsrv_mmap_judge(u64 offset)
{
	/*
	 * Use the formula: vm_pgoff * page_size / 4k < MMAP_THRESHOLD;
	 * to judge this mmap gem_obj belong to dpu or pvr.
	 * But this judge formula is based on empirical values (test on x86/LoonArch/Phytium),
	 * which may not be scientific.
	 */
	if (((offset * fh2m_inno_page_size) >> 12) < MMAP_THRESHOLD) {
		return true;
	}

	return false;
}

static __maybe_unused bool innogpu_judge_gtt_mem(struct drm_gem_object *obj)
{
	innodpu_gem_object *innodpu_obj = NULL;

	if (obj == NULL) {
		gem_err(NULL, "gem_obj pointer invalid");
		return false;
	}

	innodpu_obj = to_innodpu_obj(obj);
	if (innodpu_obj->mem_manager->pos == SYS_GTT_POSITION) {
		return true;
	}

	return false;
}

/*
 * private mmap:
 * merge PVRSRV_MMap and inno_drm_gem_mmap
 */
static int innogpu_drm_mmap(struct file *filp,
		struct vm_area_struct *vma)
{
	int ret = 0;

	if (innogpu_pvrsrv_mmap_judge(vma->vm_pgoff)) {
		ret = PVRSRV_MMap(filp, vma);
	} else {
		pgprot_t vm_page_prot = vma->vm_page_prot;
		unsigned long vm_flags = vma->vm_flags;

		ret = drm_gem_mmap(filp, vma);
		if (ret == 0) {
			innodpu_drm_fix_vma_flags(vma, vm_page_prot, vm_flags);
		}
	}

	return ret;
}

const struct file_operations s_innogpu_drm_fops = {\
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	/*.unlocked_ioctl = drm_ioctl,*/
	/*.compat_ioctl = drm_compat_ioctl,*/
	.unlocked_ioctl = innogpu_drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = innogpu_drm_compat_ioctl,
#endif
	.poll = drm_poll,
	.read = drm_read,
	.llseek = noop_llseek,
	.mmap = innogpu_drm_mmap,
#if (DRM_VERSION < KERNEL_VERSION(3, 12, 0))
	.fasync = drm_fasync,
#endif
};

static int innogpu_drm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
	u32 handle, u64 *offset)
{
	int ret = 0;

	ret = drm_gem_dumb_map_offset(file, dev, handle, offset);

	if (ret == 0) {
		if (innogpu_pvrsrv_mmap_judge(*offset >> PAGE_SHIFT)) {
			gem_err(NULL, "map dumb offset invalid[%lld], will cause mmap err", *offset);
		}
	}

	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
static struct drm_crtc *inno_crtc_from_pipe(struct drm_device *drm_dev,
						int pipe)
{
	struct drm_crtc *crtc;
	int i = 0;

	list_for_each_entry(crtc, &drm_dev->mode_config.crtc_list, head)
		if (i++ == pipe)
			return crtc;

	return NULL;
}

static int inno_drm_crtc_enable_vblank(struct drm_device *drm_dev,
					   unsigned int pipe)
{
	struct drm_crtc *crtc = inno_crtc_from_pipe(drm_dev, pipe);
	return pdp0_crtc_enable_vblank(crtc);
}

static void inno_drm_crtc_disable_vblank(struct drm_device *drm_dev,
						 unsigned int pipe)
{
	struct drm_crtc *crtc = inno_crtc_from_pipe(drm_dev, pipe);
	pdp0_crtc_disable_vblank(crtc);
}

/**
 * drm_atomic_helper_shutdown - shutdown all CRTC
 * @dev: DRM device
 *
 * This shuts down all CRTC, which is useful for driver unloading. Shutdown on
 * suspend should instead be handled with drm_atomic_helper_suspend(), since
 * that also takes a snapshot of the modeset state to be restored on resume.
 *
 * This is just a convenience wrapper around drm_atomic_helper_disable_all(),
 * and it is the atomic version of drm_crtc_force_disable_all().
 */
void drm_atomic_helper_shutdown(struct drm_device *dev)
{
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	drm_modeset_acquire_init(&ctx, 0);
	while (1) {
		ret = drm_modeset_lock_all_ctx(dev, &ctx);
		if (!ret)
			ret = drm_atomic_helper_disable_all(dev, &ctx);

		if (ret != -EDEADLK)
			break;

		drm_modeset_backoff(&ctx);
	}

	if (ret)
		inno_drm_err(dev->dev, "Disabling all crtc's during unload failed(%d)", ret);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}
#endif

/*
 * drm management:
 * merge pvr_drm_generic_driver and s_innodpu_drm_driver
 */
struct drm_driver g_innogpu_drm_driver = {
	// drm management
	.name = INNOGPU_DRM_DRIVER_NAME,
	.desc = INNOGPU_DRM_DRIVER_DESC,
	.date = INNOGPU_DRM_DRIVER_DATE,
	.major = INNOGPU_DRM_DRIVER_MAJ,
	.minor = INNOGPU_DRM_DRIVER_MIN,
	.patchlevel = INNOGPU_DRM_DRIVER_BUILD,
	.driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_PRIME | DRIVER_RENDER,

	.fops = &s_innogpu_drm_fops,
	.ioctls = s_innogpu_drm_ioctls,
	.num_ioctls = ARRAY_SIZE(s_innogpu_drm_ioctls),
	.open = innogpu_drm_file_open,
	.postclose = innogpu_drm_file_release,
	.debugfs_init = innogpu_drm_debugfs_init, // TBD innodpu_debugfs_init

	#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
	.debugfs_cleanup = innogpu_drm_debugfs_cleanup,
	#endif

	// vram management
	.gem_create_object = NULL,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd, // drm_gem_prime_handle_to_fd()
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle, // drm_gem_prime_fd_to_handle()
	.dumb_create = innodpu_gem_dumb_create,
#if (DRM_VERSION >= KERNEL_VERSION(5, 12, 0))
//	.dumb_destroy = drm_gem_dumb_destroy,
#else
	.dumb_destroy = drm_gem_dumb_destroy,
#endif
	.dumb_map_offset = innogpu_drm_gem_dumb_map_offset,
	.gem_prime_import = innodpu_gem_prime_import,
	// TBD: gem_prime_mmap current not used, because s_inno_gem_prime_dmabuf_ops.mmap not drm_gem_prime_dmabuf_ops
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(6,5,0))
	.gem_prime_mmap = drm_gem_prime_mmap, // called by default drm_gem_prime_dmabuf_ops.drm_gem_dmabuf_mmap
#endif

	// used gem_object.func when version > 5.2.0
#if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
	/*
	 * commit 805dc614d58a8fb069ed079005e591247df85246
	 * Refs: v5.2-rc5-864-g805dc614d58a
	 * gem_free_object/gem_free_object_unlocked => &drm_gem_object_funcs.free instead.
	 * gem_open_object => &drm_gem_object_funcs.open instead.
	 * gem_close_object => &drm_gem_object_funcs.close instead.
	 * gem_print_info => &drm_gem_object_funcs.print_info instead.
	 * gem_prime_get_sg_table =>&drm_gem_object_funcs.get_sg_table instead.
	 * gem_vm_ops => &drm_gem_object_funcs.vm_ops instead.
	 * gem_prime_export=>&drm_gem_object_funcs.export
	 */
	.gem_prime_get_sg_table = NULL, // called by drm_gem_prime_dmabuf_ops->map_dma_buf, TBD
	.gem_prime_res_obj = innodpu_gem_prime_res_obj, // drm_gem_prime_export called to get resv


#if (DRM_VERSION <= KERNEL_VERSION(5, 7, 0))
	/*
	 * commit ad0f449bebc79b01583c711684fefcdc9620320a
	 * Refs: v5.7-rc1-615-gad0f449bebc7
	 * - gem_print_info
	 * commit 45d58b40292b16ab847497dcd299e315a2ad7956
	 * Refs: v4.14-rc3-611-g45d58b40292b
	 * + gem_print_info
	 */
#if (DRM_VERSION >= KERNEL_VERSION(4, 15, 0))
	/* .gem_print_info = NULL, */
#endif
	/*
	 * commit 1a9458aeb8eb48bfa5f9b3e7682bddc28fd0b85e
	 * Refs: v5.7-rc1-489-g1a9458aeb8eb
	*/
	.gem_free_object = innodpu_gem_object_free,
#endif
	/*
	 * commit d693def4fd1c23f1ca5aed1afb9993b3a2069ad2
	 * Refs: v5.9-rc5-1077-gd693def4fd1c
	*/
	.gem_open_object = NULL,
	.gem_close_object = NULL,
	.gem_prime_pin = NULL, // &drm_gem_object_funcs.pin, drm_gem_prime_dmabuf_ops.attach
	.gem_prime_unpin = NULL, // &drm_gem_object_funcs.unpin, drm_gem_prime_dmabuf_ops.detach
	.gem_prime_vunmap = innodpu_gem_prime_vunmap, // &drm_gem_object_funcs.vmap, drm_gem_prime_dmabuf_ops.vunmap
	.gem_prime_vmap = innodpu_gem_prime_vmap, // &drm_gem_object_funcs.vunmap, drm_gem_prime_dmabuf_ops.vmap
	.gem_vm_ops = &innodpu_gem_vm_ops,

	/*
	 * commit e4fa8457b2197118538a1400b75c898f9faaf164
	 * Refs: v5.2-rc5-870-ge4fa8457b219
	 * -struct dma_buf * (*gem_prime_export)(struct drm_device *dev, struct drm_gem_object *obj, int flags);
	 * +struct dma_buf * (*gem_prime_export)(struct drm_gem_object *obj, int flags);
	 */
	//.gem_prime_export = drm_gem_prime_export, // gem_ops is drm_gem_prime_dmabuf_ops
	.gem_prime_export = innodpu_gem_prime_export,

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	.get_vblank_counter	= drm_vblank_no_hw_counter,
	.enable_vblank		= inno_drm_crtc_enable_vblank,
	.disable_vblank		= inno_drm_crtc_disable_vblank,
#endif

#endif // end if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
};

#if (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
static int innogpu_drm_debugfs_init(struct drm_minor *minor)
#else
static void innogpu_drm_debugfs_init(struct drm_minor *minor)
#endif
{
#if (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
	int ret = 0;
	ret = innodpu_debugfs_init(minor);
	return ret;
#else
	innodpu_debugfs_init(minor);
	return;
#endif
}

#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
void innogpu_drm_debugfs_cleanup(struct drm_minor *minor)
{
	innodpu_debugfs_cleanup(minor);
}
#endif


// sync innodpu_gem_object_create_ioctl
static int innogpu_drm_gem_create_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return innodpu_gem_object_create_ioctl(drm_dev, data, file);
}

// sync inno_gem_object_mmap_ioctl
static int innogpu_drm_gem_mmap_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	struct drm_pdp_gem_mmap *args = (struct drm_pdp_gem_mmap *)data;

	return innogpu_drm_gem_dumb_map_offset(file, drm_dev, args->handle, &args->offset);
}

// sync inno_gem_object_cpu_prep_ioctl
static int innogpu_drm_gem_cpu_prep_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return inno_gem_object_cpu_prep_ioctl(drm_dev, data, file);
}

// sync inno_gem_object_cpu_fini_ioctl
static int innogpu_drm_gem_cpu_fini_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return inno_gem_object_cpu_fini_ioctl(drm_dev, data, file);
}

// sync inno_gem_object_dump_vram_ioctl
static int innogpu_drm_gem_dump_vram_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return inno_gem_object_dump_vram_ioctl(drm_dev, data, file);
}

// sync inno_gem_object_inv_get_ioctl
static int innogpu_drm_gem_inv_get_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return inno_gem_object_inv_get_ioctl(drm_dev, data, file);
}

// sync inno_gem_object_plane_fd_ioctl
static int innogpu_drm_gem_plane_fd_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return inno_gem_object_plane_fd_ioctl(drm_dev, data, file);
}

// sync innodpu_drm_chipinfo_ioctl
static int innogpu_drm_chipinfo_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return innodpu_drm_chipinfo_ioctl(drm_dev, data, file);
}

static int innogpu_drm_gem_addr_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	return innodpu_drm_gem_addr_ioctl(drm_dev, data, file);
}

static int innogpu_drm_get_common_info(struct drm_device *drm_dev,
		void *data, struct drm_file *file)
{
	struct innogpu_drm_private *drm_private = drm_dev->dev_private;
	struct drm_common_info *inno_info = &drm_private->comm_info;
	struct drm_common_info *info = (struct drm_common_info *)data;

	info->swinfo.version = inno_info->swinfo.version;
	info->swinfo.gem_flags = inno_info->swinfo.gem_flags;
	info->hwinfo.gtt_support = inno_info->hwinfo.gtt_support;
	info->hwinfo.smmu_support = inno_info->hwinfo.smmu_support;
	info->hwinfo.chip_type = inno_info->hwinfo.chip_type;
	info->hwinfo.dpu_bus_align = inno_info->hwinfo.dpu_bus_align;

	return 0;
}

static void innogpu_drm_common_info_init(struct drm_device *drm_dev,
	struct innogpu_drm_private *drm_private)
{
	struct drm_common_info *info = &drm_private->comm_info;
	struct innodpu_drm_private *display_priv = innogpu_drm_to_display_private(drm_dev);
	unsigned int major = 4, minor = 0;

	/* hwinfo */
	info->hwinfo.chip_type = fh2m_hal_get_chiptype(drm_dev->dev);
	info->hwinfo.gtt_support = fh2m_hal_get_gtt_support(drm_dev->dev);
	info->hwinfo.smmu_support = fh2m_hal_get_smmu_support(drm_dev->dev);
	if (drm_private->drm_nulldisplay) {
		info->hwinfo.dpu_bus_align = 1;
	} else {
		info->hwinfo.dpu_bus_align = 256;
	}

	/* swinfo */
	info->swinfo.version = (major << 24 | minor << 0);
	info->swinfo.gem_flags |= BIT(0); /* support visible varm */
	info->swinfo.gem_flags |= display_priv->has_inv_mem << 1; /* support invisible mem */
	info->swinfo.gem_flags |= display_priv->has_shared_mem << 2; /* support shared mem */
	info->swinfo.gem_flags |= s_dpu_support_smmu << 3; /* support smmu todo */
	info->swinfo.gem_flags |= display_priv->has_gtt_mem << 4; /* gtt mem */
	info->swinfo.gem_flags |= display_priv->has_uncontinuous_mem << 5; /* no continuous vram */

	return;
}

static struct device_dma_parameters *innogpu_drm_get_dma_params(struct drm_device * drm_dev)
{
	return &(((struct pvr_drm_private *)innogpu_drm_to_pvr_private(drm_dev))->dma_parms);
}

static int innogpu_drm_component_bind(struct device *dev) //platform_device->device
{
	int ret = 0;
	bool drm_nulldisplay = false;
	struct innogpu_drm_private *drm_private = NULL;
	struct pvr_drm_private *pvr_priv = NULL;
	struct innodpu_drm_private *display_priv = NULL;
	struct drm_device *drm_dev = NULL;
	struct device *parent = dev->parent;
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
	plat_data_t *drm_plat_data = dev_get_platdata(dev);

	if (!parent) {
		inno_drm_err(dev, "pcie device does not find");
		return -EINVAL;
	}

	fh2m_hal_module_loadtime_register(parent, "drm_component_bind");

#ifndef SUPPORT_DMA_TRANSFER
	if (fh2m_hal_has_inv_mem(parent)) {
		inno_drm_err(dev, "env not support dma, but card support inv memory");
	} else {
		inno_drm_info(dev, "env not support dma, innodpu does not clear/back/recover vram");
	}
#endif
	if (fh2m_hal_get_nulldisplay()) {
		drm_nulldisplay = true;
	} else {
		drm_nulldisplay = (drm_plat_data->dev_idx > 0) ? true : false;
	}

	inno_drm_info(dev, "Start init innosilicon gpu with %s. Version is 1.0.0",
				  drm_nulldisplay ? "nulldisp" : "display");

	// drm_dev->dev needs pcie_dev->device
	drm_dev = drm_dev_alloc(&g_innogpu_drm_driver, parent);
	if (IS_ERR(drm_dev)) {
		inno_drm_err(dev, "drm_dev_alloc failed");
		return PTR_ERR(drm_dev);
	}
	fh2m_inno_platform_set_drvdata(pdev, drm_dev);	// platform_device pdev->dev->driver_data = drm_dev;

	drm_private = kzalloc(sizeof(struct innogpu_drm_private), GFP_KERNEL);
	if (!drm_private) {
		inno_drm_err(dev, "Alloc innogpu_drm_private failed");
		ret = -ENOMEM;
		goto err_drm_private_alloc;
	}
	drm_dev->dev_private = drm_private;

#if (DRM_VERSION < KERNEL_VERSION(5, 14, 0))
	drm_dev->pdev = drm_plat_data->pdev_rsrc->pdev;
#endif

	drm_private->drm_plat_data = drm_plat_data;
	drm_private->drm_nulldisplay = drm_nulldisplay;
	pvr_priv = innogpu_drm_to_pvr_private(drm_dev);
	display_priv = innogpu_drm_to_display_private(drm_dev);
#ifdef SUPPORT_DMA_TRANSFER
	drm_private->drm_support_dmatrans = true;
#else
	drm_private->drm_support_dmatrans = false;
#endif

	ret = innodpu_early_load(drm_dev, display_priv, dev);
	if (ret) {
		inno_drm_err(dev, "Early load innosilicon drm device Failed");
		goto err_drm_early_load;
	}

	inno_drm_info(dev, "Start bind all component device");

	ret = component_bind_all(dev, drm_dev);
	if (ret) {
		inno_drm_err(dev, "bind all innosilicon dpu device failed, ret: %d\n", ret);
		goto err_drm_bind_all;
	}

	if (!dev->dma_parms)
		dev->dma_parms = innogpu_drm_get_dma_params(drm_dev); // TBD: &priv->dma_parms;
	dma_set_max_seg_size(dev, DMA_BIT_MASK(32)); // TBD: maybe risk!!!

	ret = innodpu_late_load(drm_dev, display_priv);
	if (ret) {
		inno_drm_err(dev, "Late load innosilicon drm device Failed");
		goto err_drm_late_load;
	}
	innogpu_drm_set_irq(drm_dev, true);

	return 0;

err_drm_late_load:
	component_unbind_all(dev, drm_dev);
err_drm_bind_all:
	innodpu_early_unload(drm_dev, display_priv);
err_drm_early_load:
	kfree(drm_private);
	drm_private = NULL;
	drm_dev->dev_private = NULL;
err_drm_private_alloc:
	drm_dev_put(drm_dev);		// drm_dev_put = drm_dev_fini + kfree(drm_dev)
	fh2m_inno_platform_set_drvdata(pdev, NULL);

	return ret;
}

static void innogpu_drm_component_unbind(struct device *dev)
{
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
	struct drm_device *drm_dev = fh2m_inno_platform_get_drvdata(pdev);
	struct innogpu_drm_private *drm_private = drm_dev->dev_private;
	struct innodpu_drm_private *display_priv = NULL;

	display_priv = innogpu_drm_to_display_private(drm_dev);
	if (display_priv) {
		innodpu_late_unload(drm_dev, display_priv);
		component_unbind_all(dev, drm_dev);
		innodpu_early_unload(drm_dev, display_priv);

		kfree(drm_private);
		drm_dev->dev_private = NULL;
	}
	drm_dev_put(drm_dev);		// drm_dev_put = drm_dev_fini + kfree(drm_dev)
	fh2m_inno_platform_set_drvdata(pdev, NULL);

	inno_drm_info(dev, "drm unbind finished");
}

static int innogpu_drm_compare_dev(struct device *dev, void *data)
{
	return (dev == (struct device *)data);
}

/*
 * TBD!!!
 * dev ：sub device
 * data : struct innogpu_drm_match * infomation。
 * tmatch.id : drm index
 * tmatch.name : s_innogpu_drm_sub_drivers subdevice platform name
 * tmatch.subdev:  not used
 * tmatch.pdev : pcie device ( current dev->parent)
 * return 0 if the device doesn't match and non-zero if it does
 */
static int innogpu_drm_match_dev(struct device *dev, void *data)
{
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
	struct innogpu_drm_match *tmatch = data;
	plat_data_t *plat_data = dev_get_platdata(dev);
	int reg_vkms_nums = fh2m_hal_get_dev_nums(tmatch->pdev, DEV_VKMS);
	int nulldisplay_pipe_num = fh2m_hal_get_nulldisplay_drm_pipe_num();
	bool nulldisplay = fh2m_hal_get_nulldisplay();
	int display_nums = fh2m_hal_get_dev_nums(tmatch->pdev, DEV_DPU) - reg_vkms_nums;
	bool display_device = false;
	bool pvr_device = false;
	int ret = 0;
	int did = 0;
#if defined(NO_HARDWARE) || !defined(CONFIG_DRM_INNO_SRVKM)
	int gpu_core_num = 1;
	int gpu_core_index = 0;
#else
	int gpu_core_index = fh2m_hal_get_gpu_core_index();
	int gpu_core_num = fh2m_hal_get_gpu_core_nums();
#endif
	/* filter out non-drm devices */
	if (IS_ERR_OR_NULL(plat_data) || strcmp(pdev->name, tmatch->name)) {
		return 0;
	}

#ifdef CONFIG_DRM_INNO_AUDIO
	if(!strcmp(pdev->name, INNO_AUDIO_DEVICE_NAME))
		return 1;
#endif

	display_device = !strcmp(pdev->name, INNO_DPU_DEVICE_NAME);
	pvr_device = !strcmp(pdev->name, INNO_GPU_DEVICE_NAME);

	/* pvr */
	if (pvr_device) {
		did = plat_data->dev_idx;
		if (did == tmatch->id || (gpu_core_num == 1 && gpu_core_index == did)) {
			return 1;
		} else {
			return 0;
		}
	}

	/* display */
	if (nulldisplay) { /* nulldisplay crtc + vkms */
			did = plat_data->dev_idx;
			if (did >= 0 && ((did / nulldisplay_pipe_num) == tmatch->id))
				ret = 1;
	} else {
		if (tmatch->id > 0) { /* nulldisplay drm_device */
			if (display_device) /*dpu device*/
				did = plat_data->dev_idx - display_nums;
			else	/*vkms device*/
				did = plat_data->dev_idx;
			if ((did >= 0) && ((did / nulldisplay_pipe_num) == (tmatch->id - 1)))
				ret = 1;
		} else {
			ret = 1;
			if (display_device) { /* nulldisplay / display drm_device */
				if (plat_data->dev_idx < display_nums)	/* really display dpu_device */
					ret = 1;
				else /* nulldisplay dpu_device */
					ret = 0;
			}
		}
	}

	inno_drm_info(dev, "card%d match %s_%d, %s match %s %s\n", tmatch->id, pdev->name, plat_data->dev_idx,
				  pdev->name, tmatch->name, ret ? "success" : "failed");

	return ret;
}


int innogpu_drm_irq_enabled(struct drm_device *drm_dev, bool *enable)
{
	__maybe_unused struct innogpu_drm_private *drm_private = NULL;

	if (!drm_dev->dev_private)
		return -EINVAL;

#if (DRM_VERSION >= KERNEL_VERSION(5, 15, 0))
	drm_private = drm_dev->dev_private;
	*enable = drm_private->irq_enabled;
#else
	*enable = drm_dev->irq_enabled;
#endif

	return 0;
}

/*
 * struct platform_device *pdev : drm platform device
 */
static int innogpu_drm_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	struct device *dev = &pdev->dev;
	int i;
	struct device *tmpdev = NULL;
	struct device *subdev = NULL;
	struct platform_driver *drv = NULL;
	struct innogpu_drm_match tmatch;
	struct drm_device *drm_dev = NULL;
	struct innodpu_drm_private *display_priv = NULL;
	struct innogpu_drm_private *drm_private = NULL;
	int ret = 0;

	plat_data_t *drm_plat_data = dev_get_platdata(dev);
	plat_data_t *dev_plat_data = NULL;

	ret = fh2m_innodpu_log_init();
	if (ret < 0) {
		inno_drm_err(dev, "fh2m_innodpu_log_init fail, ret: %d\n", ret);
		return ret;
	}

	inno_drm_info(dev, "Git Current Branch: %s", GIT_BRANCH);
	inno_drm_info(dev, "Git Current CommitID: %s", GIT_COMMIT_ID);
	inno_drm_info(dev, "Git Current CommitID(dirty): %s", GIT_DIRTY);

	{
		struct hw_board_info *match_board = NULL;
		struct hw_board_info board[] = {
			HW_BOARD_INFO_ITEM("ZY",   "YF27_1"),           //VGA+HDMI2VGA+DP2LVDS
			HW_BOARD_INFO_ITEM("ZY",   "YF27_2"),           //VGA+HDMI2VGA+DP2VGA
			HW_BOARD_INFO_ITEM("WZ",   "WZ-IFS04C V0.0"),   //VGA+HDMI2VGA+HDMI
			HW_BOARD_INFO_ITEM("SCKJ", "DIGICITI-IP-2304"), //VGA+HDMI2VGA+HDMI
		};

		match_board = innodpu_odm_pcb_match(dev->parent, board, ARRAY_SIZE(board));
		if (match_board) {
			s_dpu_driver_group = 1;
			inno_drm_info(dev, "match board[%s/%s]", match_board->odm_vendor,
						  match_board->pcb_version);
			inno_drm_info(dev, "s_dpu_driver_group = %d", s_dpu_driver_group);
		}
	}

	/* match compnoent device */
	for (i = 0; i < SUB_DRIVER_GROUP_NUMBER; i++) {
		drv = s_innogpu_drm_sub_drivers[s_dpu_driver_group][i];
		if (!drv)
			continue;

		tmpdev = NULL;
		tmatch.id = drm_plat_data->dev_idx;
		tmatch.name = drv->driver.name;
		tmatch.pdev = dev->parent;

		inno_drm_info(dev, "component_%d, %s_%d will match", i, tmatch.name, tmatch.id);

		if (tmatch.id > 0 || fh2m_hal_get_nulldisplay()) {
			if (((&g_innogpu_vkms_driver != drv)) && (&g_innogpu_dpu_driver != drv) && (&g_innogpu_pvrsrvkm_driver != drv))
					continue;
		} else {
			if (((&g_innogpu_hdmi_driver == drv)) && (s_hdmi_nums == 0)) {
				inno_drm_info(dev, "hdmi_disabled");
				continue;
			}

			if ((&g_innogpu_dp_driver == drv) && (s_dp_nums == 0)) {
				inno_drm_info(dev, "dp_disabled");
				continue;
			}

			if (((&g_innogpu_vga_driver == drv)) && (s_vga_nums == 0)) {
				inno_drm_info(dev, "vga_disabled");
				continue;
			}

			if ((&g_innogpu_vkms_driver == drv))
				continue;
		}

		do {
			subdev = bus_find_device(&platform_bus_type,
									 tmpdev, &tmatch, (void *)innogpu_drm_match_dev);
			put_device(tmpdev);
			tmpdev = subdev;
			if (!subdev) {
				break;
			}

			dev_plat_data = (plat_data_t *)dev_get_platdata(subdev);
			if (drm_plat_data->pdev_idx != dev_plat_data->pdev_idx) {
				continue;
			}

#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
			device_link_add(dev, subdev, DL_FLAG_STATELESS);
#endif
			component_match_add(dev, &match, innogpu_drm_compare_dev, subdev);
		} while (true);
	}

	if (match == NULL || dev == NULL) {
		inno_drm_err(dev, "match: %#llx, dev: %#llx, maybe subdev unregister?\n", match, dev);
		ret = -EPROBE_DEFER;
		goto err_component_match_add;
	}

	ret = component_master_add_with_match(dev, &s_innogu_drm_component_ops, match);
	if (ret < 0) {
		inno_drm_err(dev, "component master add match fail, ret: %d\n", ret);
		goto err_component_master_add;
	}

	drm_dev = fh2m_inno_platform_get_drvdata(pdev);	// platform_device pdev->dev->driver_data = drm_dev;
	if (drm_dev == NULL) {
		inno_drm_err(dev, "drm_dev is null, maybe subdevice driver register after master device driver\n");
		ret = -ENODEV;
		goto err_platform_get_drvdata;
	}

	display_priv = innogpu_drm_to_display_private(drm_dev);
	drm_private = drm_dev->dev_private;

	/*
	 * Note:
	 * 1. Workaround bug#12419, abba deadlock when innogpu and amdgpu init concurrently on kylin2203/2403.
	 * 2. This is a kernel bug when kernel version less than v5.7.
	 * 3. Workaround method:
	 *           old code                                        new code
	 *    innogpu_drm_probe()                             innogpu_drm_probe()
	 *        -> component_master_add_with_match()            -> component_master_add_with_match()
	 *            -> master_ops.bind()                            -> master_ops.bind()
	 *                -> ...                                          -> ...
	 *                -> drm_dev_register()                   -> drm_dev_register()
	 * 4. Known limits, also see comments #14 in bug#12419:
	 *    subdevice platform_driver register must before master device platform_driver.
	 * */
	ret = drm_dev_register(drm_dev, 0);
	fh2m_hal_module_loadtime_register(dev->parent, "drm_dev_register");
	if (ret) {
		inno_drm_err(dev, "drm device register failed, ret: %d\n", ret);
		goto err_drm_dev_register;
	}

	ret = innodpu_fb_init(drm_dev, display_priv);
	if (ret) {
		inno_drm_err(dev, "fbdev register failed, ret: %d\n", ret);
		goto err_fb_init;
	}

	ret = innodpu_pm_init(drm_dev, display_priv);
	if (ret) {
		inno_drm_err(dev, "pm init failed, ret: %d\n", ret);
		goto err_pm_init;
	}

	innogpu_drm_common_info_init(drm_dev, drm_private);
	fh2m_hal_module_loadtime_register(dev->parent, "drm_common_info");

	return 0;

err_pm_init:
	innodpu_fb_fini(drm_dev, display_priv);
err_fb_init:
	drm_atomic_helper_shutdown(drm_dev);
	drm_dev_unregister(drm_dev);
err_drm_dev_register:
err_platform_get_drvdata:
	component_master_del(dev, &s_innogu_drm_component_ops);
err_component_master_add:
err_component_match_add:
	fh2m_innodpu_log_fini();

	return ret;
}

static int innogpu_drm_remove(struct platform_device *pdev)
{
	struct drm_device *drm_dev = fh2m_inno_platform_get_drvdata(pdev);
	struct innodpu_drm_private *display_priv = NULL;

	display_priv = innogpu_drm_to_display_private(drm_dev);
	if (display_priv) {
		innodpu_pm_fini(drm_dev, display_priv);
		innodpu_fb_fini(drm_dev, display_priv);
		drm_atomic_helper_shutdown(drm_dev);
		drm_dev_unregister(drm_dev);
	}

	component_master_del(&pdev->dev, &s_innogu_drm_component_ops);

	fh2m_innodpu_log_fini();

	return 0;
}
static void platform_subdrivers_unload(void)
{
	int i;

	for (i = SUB_DRIVER_GROUP_NUMBER - 1; i >= 0; i--) {
		if (s_innogpu_drm_sub_drivers[0][i] != NULL) {
			platform_driver_unregister(s_innogpu_drm_sub_drivers[0][i]);
		}
	}
}
static struct platform_driver s_innogpu_drm_platform_driver = {
	.probe = innogpu_drm_probe,
	.remove = innogpu_drm_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = INNO_DRM_DEVICE_NAME,
		.pm = &innogpu_drm_pm_ops, // replace innodpu_drm_pm_ops
	},
};

#if defined(NO_HARDWARE) || !defined(CONFIG_DRM_INNO_SRVKM)
static int __init innogpu_drm_init(void)
#else
int innogpu_drm_init(void)
#endif
{
	int i = 0;
	int ret = 0;
	ret = PVRSRVDriverInit(); // TBD: support for multi-core
	if (ret)
		return ret;
	// TBD: pvr_init ???  PVRSRVDriverInit, need move to probe
	for (i = 0; i < SUB_DRIVER_GROUP_NUMBER; i++) {
		if (s_innogpu_drm_sub_drivers[0][i] != NULL) {
			platform_driver_register(s_innogpu_drm_sub_drivers[0][i]);
		}
	}

	ret = platform_driver_register(&s_innogpu_drm_platform_driver);
	if (ret) {
		goto err;
	}
	return 0;

err:
	platform_subdrivers_unload();
	PVRSRVDriverDeinit();
	return ret;
}

#if defined(NO_HARDWARE) || !defined(CONFIG_DRM_INNO_SRVKM)
static void __exit innogpu_drm_exit(void)
#else
void innogpu_drm_exit(void)
#endif
{
	PVRSRV_DATA *psPVRSRVData = fh2m_PVRSRVGetPVRSRVData();
	DRM_DEBUG_DRIVER("\n");
	psPVRSRVData->bUnload = IMG_TRUE;
	platform_driver_unregister(&s_innogpu_drm_platform_driver);
	platform_subdrivers_unload();
	PVRSRVDriverDeinit();
	DRM_DEBUG_DRIVER("done\n");
}

#if defined(NO_HARDWARE) || !defined(CONFIG_DRM_INNO_SRVKM)
late_initcall(innogpu_drm_init);
module_exit(innogpu_drm_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR(INNOGPU_DRM_DRIVER_AUTHOR);
MODULE_DESCRIPTION(INNOGPU_DRM_DRIVER_DESC);
#endif

/*************************************
*innogpu_get_drm_from_pdev
*let platform_device->dev convert to drm_device
*struct device : platfrom_device->dev
*return 		: struct drm_device * ddev or NULL
**************************************/
struct drm_device * innogpu_get_drm_from_pdev(struct device *plat_dev)
{
	plat_data_t *drm_plat_data = NULL;
	if (!plat_dev)
		return NULL;
	drm_plat_data = fh2m_inno_dev_get_platdata(plat_dev);

	if (!drm_plat_data)
		return NULL;
	else
		return  drm_plat_data->pdev_rsrc->ddev[drm_plat_data->dev_idx];
}

/****************
 *innogpu_get_ddev_from_plat_dev
 *let platform_device->dev convert to drm_device
 *struct device : platfrom_device->dev
 *return : drm_device->dev or NULL
 * */
void * innogpu_get_ddev_from_plat_dev(struct device * plat_dev)
{
	plat_data_t *drm_plat_data = NULL;
	struct drm_device * ddev = NULL;
	if (!plat_dev)
		return NULL;
	drm_plat_data = fh2m_inno_dev_get_platdata(plat_dev);

	if (!drm_plat_data)
		return NULL;
	else {
		ddev = drm_plat_data->pdev_rsrc->ddev[drm_plat_data->dev_idx];
		return ddev ? ddev->dev : NULL;
	}
}

int innogpu_drm_set_irq(struct drm_device *drm_dev, bool enable)
{
	__maybe_unused struct innogpu_drm_private *drm_private = NULL;

	if (!drm_dev->dev_private)
		return -EINVAL;

#if (DRM_VERSION >= KERNEL_VERSION(5, 15, 0))
	drm_private = drm_dev->dev_private;
	drm_private->irq_enabled = enable;
#else
	drm_dev->irq_enabled = enable;
#endif

	return 0;
}

int innogpu_drm_get_nulldisp(struct drm_device *drm_dev, bool *nulldisp)
{
	struct innogpu_drm_private *drm_private = NULL;

	if (!drm_dev->dev_private)
		return -EINVAL;

	drm_private = drm_dev->dev_private;
	*nulldisp = drm_private->drm_nulldisplay;

	return 0;
}

int innogpu_drm_get_platdata(struct drm_device *drm_dev, plat_data_t **drm_plat_data)
{
	__maybe_unused struct innogpu_drm_private *drm_private = NULL;

	if (!drm_dev->dev_private)
		return -EINVAL;

	drm_private = drm_dev->dev_private;
	*drm_plat_data = drm_private->drm_plat_data;

	return 0;
}

void * innogpu_drm_to_pvr_private(struct drm_device *drm_dev)
{
	__maybe_unused struct innogpu_drm_private *drm_private = NULL;

	if (!drm_dev->dev_private) {
		fh2m_inno_printk(KERN_ERR "%s %d  drm_dev:%p dev_private: %p \n", __func__, __LINE__, drm_dev, drm_dev->dev_private);
		return NULL;
	}
	drm_private = drm_dev->dev_private;
	return &(drm_private->pvr_priv);
}

void * innogpu_drm_to_display_private(struct drm_device *drm_dev)
{
	__maybe_unused struct innogpu_drm_private *drm_private = NULL;

	if (drm_dev == NULL) {
		inno_drm_err(NULL, "get drm_dev:%p", drm_dev);
		return NULL;
	}

	if (!drm_dev->dev_private) {
		inno_drm_err(drm_dev->dev, "drm_dev:%p dev_private: %p", drm_dev, drm_dev->dev_private);
		return NULL;
	}

	drm_private = drm_dev->dev_private;
	return &(drm_private->display_priv);
}
