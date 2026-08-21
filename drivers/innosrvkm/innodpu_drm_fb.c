/*************************************************************************/ /*!
@File			innodpu_drm_fb.c
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
#if defined(CONFIG_DRM_FBDEV_EMULATION)
#include "inno_drm_version.h"
#include <linux/export.h>
#include "inno_drm_version.h"
#include <linux/fb.h>
#include <linux/suspend.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "inno_timer.h"
#include "innodpu_drm_drv.h"

#include "innodpu_drm_fb.h"
#include "kernel_compatibility.h"
#include "osfunc_common.h"

#include "innogpu_drm.h"

#define FBDEV_NAME "innodrmfb"

#ifndef FBINFO_DEFAULT
#define FBINFO_DEFAULT (0)
#endif

static inline int drm_mode_fb_cmd2_validate(const struct drm_mode_fb_cmd2 *mode_cmd)
{
	switch (mode_cmd->pixel_format) {
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		break;
	default:
		DRM_ERROR_RATELIMITED("pixel format not supported (format = %u)\n", mode_cmd->pixel_format);
		return -EINVAL;
	}

	if (mode_cmd->flags & DRM_MODE_FB_INTERLACED) {
		DRM_ERROR("mode_cmd flags %#x interlaced framebuffers not supported\n", mode_cmd->flags);
		return -EINVAL;
	}
#if (DRM_VERSION >= KERNEL_VERSION(4, 1, 0))
	if (mode_cmd->flags & DRM_MODE_FB_MODIFIERS) {
#if 0
		if (mode_cmd->modifier[0] != INNODPU_FORMAT_MOD_AB24) {
			DRM_ERROR("format modifier 0x%llx is not supported\n", mode_cmd->modifier[0]);
		}
#endif
	}
#endif

	return 0;
}

static void innodpu_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct inno_framebuffer *inno_fb = to_inno_framebuffer(fb);
	int i;
	u8  obj_num = 0;

	inno_drm_info(fb->dev->dev, "[FB:%d DESTROY]\n", fb->base.id);

#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
	obj_num = 1;
#else
	obj_num = 4;
#endif

	for (i = 0; i < obj_num; i++) {
		if (inno_fb->obj[i]) {
			drm_gem_object_put(inno_fb->obj[i]);
			inno_fb->obj[i] = NULL;
		}
	}
	drm_framebuffer_cleanup(fb);
	kfree(inno_fb);
}

static int innodpu_framebuffer_create_handle(struct drm_framebuffer *fb,
											 struct drm_file *file, unsigned int *handle)
{
	struct inno_framebuffer *inno_fb = to_inno_framebuffer(fb);

	inno_drm_info(fb->dev->dev, "[FB:%d CREATE]\n", fb->base.id);

#ifdef CONFIG_KALLSYMS
	inno_drm_info(fb->dev->dev, "called \n");
#endif

	return drm_gem_handle_create(file, inno_fb->obj[0], handle);
}

static const struct drm_framebuffer_funcs s_inno_framebuffer_funcs = {
	.destroy = innodpu_framebuffer_destroy,
	.create_handle = innodpu_framebuffer_create_handle,
	.dirty = NULL,
};

static inline int inno_framebuffer_init(struct innodpu_drm_private *dev_priv,
#if (DRM_VERSION >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && (DRM_VERSION >= KERNEL_VERSION(3, 18, 0)))
										const
#endif
										struct drm_mode_fb_cmd2 *mode_cmd,
										struct inno_framebuffer *pdp_fb, struct drm_gem_object *obj)
{
	struct drm_framebuffer *fb;

	if (!pdp_fb)
		return -EINVAL;

	fb = to_drm_framebuffer(pdp_fb);
	pdp_fb->obj[0] = obj;

	drm_helper_mode_fill_fb_struct(dev_priv->drm_dev, fb, mode_cmd);

	return drm_framebuffer_init(dev_priv->drm_dev, fb, &s_inno_framebuffer_funcs);
}

static int inno_modeset_validate_init(struct innodpu_drm_private *dev_priv,
									  struct drm_mode_fb_cmd2 *mode_cmd,
									  struct inno_framebuffer *pdp_fb, struct drm_gem_object *obj)
{
	int err;

	err = drm_mode_fb_cmd2_validate(mode_cmd);
	if (err)
		return err;

	return inno_framebuffer_init(dev_priv, mode_cmd, pdp_fb, obj);
}

static int inno_fbdev_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_device *dev = helper->dev;

	if (info->state != FBINFO_STATE_RUNNING) {
		DRM_WARN("info->state != FBINFO_STATE_RUNNING\n");
		return -EINVAL;
	}

	/* BUG11472, 4K HDMI login interface display incomplete when waking up on
	* dragon core platform, due to the character interface waking up after desktop waking up,
	* the 4K resolution issued by Xorg is switched to 1080P caused. so before switching to the character
	* interface, first judge whether there is a master device on card node. if there is a master node. it
	* means that the fb is not bound to the Do not switch to the character interface.
	*/
	if (dev && READ_ONCE(dev->master)) {
		DRM_WARN("fb_helper_is_unbound\n");
		return -EINVAL;
	}

	return drm_fb_helper_check_var(var, info);
}

static struct fb_ops s_inno_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = inno_fbdev_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_mmap = fb_io_mmap,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
};

static struct fb_info *inno_fbdev_helper_alloc(struct drm_fb_helper *helper)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 3, 0))
	struct device *dev = helper->dev->dev;
	struct fb_info *info;
	int ret = 0;

	info = framebuffer_alloc(0, dev);
	if (!info)
		return ERR_PTR(-ENOMEM);

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret)
		goto err_release;

	info->apertures = alloc_apertures(1);
	if (!info->apertures) {
		ret = -ENOMEM;
		goto err_free_cmap;
	}

	helper->fbdev = info;

	return info;

err_free_cmap:
	fb_dealloc_cmap(&info->cmap);
err_release:
	framebuffer_release(info);
	return ERR_PTR(ret);
#else
#if (DRM_VERSION >= KERNEL_VERSION(6,2,0))
	return drm_fb_helper_alloc_info(helper);
#else
	return drm_fb_helper_alloc_fbi(helper);
#endif
#endif
}

static inline void
inno_fbdev_helper_fill_info(struct drm_fb_helper *helper,
							struct drm_fb_helper_surface_size *sizes,
							struct fb_info *info, struct drm_mode_fb_cmd2 __maybe_unused * mode_cmd)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 11, 0))
	drm_fb_helper_fill_fix(info, mode_cmd->pitches[0], helper->fb->depth);
	drm_fb_helper_fill_var(info, helper, sizes->fb_width, sizes->fb_height);
#elif (DRM_VERSION < KERNEL_VERSION(5, 2, 0))
	drm_fb_helper_fill_fix(info, mode_cmd->pitches[0], helper->fb->format->depth);
	drm_fb_helper_fill_var(info, helper, helper->fb->width, helper->fb->height);
#else
	drm_fb_helper_fill_info(info, helper, sizes);
#endif
}

static int inno_fbdev_probe(struct drm_fb_helper *helper, struct drm_fb_helper_surface_size *sizes)
{
	struct inno_fbdev *inno_fbdev = container_of(helper, struct inno_fbdev, helper);
	struct drm_framebuffer *fb = to_drm_framebuffer(&inno_fbdev->fb);
	innodpu_mem_manager *mem_manager = inno_fbdev->priv->visible_mem_manager; // must visiable
	struct drm_device *dev = helper->dev;
	struct drm_mode_fb_cmd2 mode_cmd;
	innodpu_gem_object *innodpu_obj;
	struct drm_gem_object *obj;
	struct fb_info *info;
	void __iomem *vaddr;
	size_t obj_size;
	int err;

	if (helper->fb)
		return 0;

	mutex_lock(&dev->struct_mutex);

	/* Create a framebuffer */
	info = inno_fbdev_helper_alloc(helper);
	if (!info) {
		err = -ENOMEM;
		goto err_unlock_dev;
	}

	inno_drm_info(dev->dev, "inno_fbdev_helper_alloc");

	memset(&mode_cmd, 0, sizeof(mode_cmd));
#ifdef HW_ALIGN
	mode_cmd.pitches[0] = ALIGN(DIV_ROUND_UP(sizes->surface_width * sizes->surface_bpp, 8), 16);
#else
	mode_cmd.pitches[0] = sizes->surface_width * DIV_ROUND_UP(sizes->surface_bpp, 8);
#endif
	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp, sizes->surface_depth);
#if (DRM_VERSION >= KERNEL_VERSION(5, 18, 0))
	if (!dev->mode_config.fb_modifiers_not_supported)
#else
	if (dev->mode_config.allow_fb_modifiers)
#endif
	{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
		mode_cmd.modifier[0] = fb->modifier;
#else
		mode_cmd.modifier[0] = fb->modifier[0];
#endif
		if (mode_cmd.modifier[0] == INNODPU_FORMAT_MOD_AB24)
			mode_cmd.flags |= DRM_MODE_FB_MODIFIERS;
	}
	obj_size = PAGE_ALIGN(mode_cmd.height * mode_cmd.pitches[0]);

	obj = innodpu_gem_object_create_priv(dev, mem_manager, obj_size, DBM_GEM_NEED_CONTINUOUS);
	if (!obj) {
		err = -EINVAL;
		goto err_unlock_dev;
	}
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto err_unlock_dev;
	}

	inno_drm_info(dev->dev, "obj_size=%zu(%dx%d, pitches=%d)\n", obj_size,
				  mode_cmd.width, mode_cmd.height, mode_cmd.pitches[0]);

	innodpu_obj = to_innodpu_obj(obj);
	innodpu_obj->is_fbdev_obj = true;

	vaddr = (void __iomem *)fh2m_inno_ioremap_wc_portable(innodpu_obj->cpu_paddr, obj->size);

	if (!vaddr) {
		err = PTR_ERR(vaddr);
		goto err_gem_destroy;
	}

	/* Zero fb memory, fb_memset accounts for iomem address space */
	OSDeviceMemSet(vaddr, 0, obj_size);

	inno_drm_info(dev->dev, "ioremap_wc, clear framebuffer...");

	err = inno_modeset_validate_init(inno_fbdev->priv, &mode_cmd, &inno_fbdev->fb, obj);
	if (err)
		goto err_gem_unmap;

	inno_drm_info(dev->dev, "framebuffer init finish");

	helper->fb = fb;
#if (DRM_VERSION < KERNEL_VERSION(6, 2, 0))
	helper->fbdev = info;
#else
	helper->info = info;
#endif
	/* Fill out the Linux framebuffer info */
	fh2m_inno_strlcpy(info->fix.id, FBDEV_NAME, sizeof(info->fix.id));
	inno_fbdev_helper_fill_info(helper, sizes, info, &mode_cmd);
	info->par = helper;
	info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_DISABLED;
#if (DRM_VERSION < KERNEL_VERSION(4, 20, 0))
	info->flags |= FBINFO_CAN_FORCE_OUTPUT;
#endif
	info->fbops = &s_inno_fbdev_ops;
	info->fix.smem_start = innodpu_obj->cpu_paddr;
	info->fix.smem_len = obj_size;
	info->screen_base = vaddr;
	info->screen_size = obj_size;

#if (DRM_VERSION < KERNEL_VERSION(6,3,0))
	info->apertures->ranges[0].base = innodpu_obj->cpu_paddr;
	info->apertures->ranges[0].size = obj_size;
#endif
	mutex_unlock(&dev->struct_mutex);

	inno_drm_info(dev->dev, "fbdev registered");

	return 0;

err_gem_unmap:
	iounmap(vaddr);

err_gem_destroy:
	innodpu_gem_object_free(obj);

err_unlock_dev:
	mutex_unlock(&dev->struct_mutex);

	inno_drm_err(dev->dev, "failed(err=%d)", err);
	return err;
}

static const struct drm_fb_helper_funcs s_inno_fbdev_helper_funcs = {
	.fb_probe = inno_fbdev_probe,
};

static struct inno_fbdev *inno_fbdev_create(struct innodpu_drm_private *dev_priv)
{
	struct device *dev = dev_priv->drm_dev->dev;
	struct inno_fbdev *inno_fbdev;
	int err;

	inno_fbdev = kzalloc(sizeof(*inno_fbdev), fh2m_hal_get_inno_gfp_kernel());
	if (!inno_fbdev) {
		inno_drm_err(dev, "short of memory");
		return NULL;
	}

	// inno_fbdev->helper.funcs = s_inno_fbdev_helper_funcs
#if (DRM_VERSION < KERNEL_VERSION(6,3,0))
	drm_fb_helper_prepare(dev_priv->drm_dev, &inno_fbdev->helper, &s_inno_fbdev_helper_funcs);
#else
	drm_fb_helper_prepare(dev_priv->drm_dev, &inno_fbdev->helper,
	inno_fbdev->preferred_bpp, &s_inno_fbdev_helper_funcs);
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 11, 0))
	err = drm_fb_helper_init(dev_priv->drm_dev, &inno_fbdev->helper,
							 dev_priv->drm_dev->mode_config.num_crtc,
							 dev_priv->drm_dev->mode_config.num_connector);
#elif (DRM_VERSION < KERNEL_VERSION(5, 7, 0))
	err = drm_fb_helper_init(dev_priv->drm_dev, &inno_fbdev->helper,
							 dev_priv->drm_dev->mode_config.num_connector);
#else
	err = drm_fb_helper_init(dev_priv->drm_dev, &inno_fbdev->helper);
#endif
	if (err) {
		inno_drm_err(dev, "short of memory %d", err);
		goto err_free_fbdev;
	}
	innodpu_bind_atomic(dev_priv->drm_dev, true);

	inno_fbdev->priv = dev_priv;
	dev_priv->fbdev = inno_fbdev;
#if (DRM_VERSION < KERNEL_VERSION(5, 3, 0))
	drm_fb_helper_single_add_all_connectors(&inno_fbdev->helper);
#endif
	inno_fbdev->preferred_bpp = 32;

	/* drm_fb_helper_initial_config=>drm_fb_helper_single_fb_probe=>Call ->fb_probe() */
	// s_inno_fbdev_helper_funcs.probe()
#if (DRM_VERSION >= KERNEL_VERSION(6,3,0))
	err = drm_fb_helper_initial_config(&inno_fbdev->helper);
#else
	err = drm_fb_helper_initial_config(&inno_fbdev->helper, inno_fbdev->preferred_bpp);
#endif
	if (err) {
		inno_drm_err(dev, "drm_fb_helper_initial_config init failed: %d", err);
		goto err_fb_helper_fini;
	}

	inno_drm_info(dev, "fbdev registered");
	return inno_fbdev;

err_fb_helper_fini:
	drm_fb_helper_fini(&inno_fbdev->helper);
	innodpu_bind_atomic(dev_priv->drm_dev, false);

err_free_fbdev:
	kfree(inno_fbdev);

	inno_drm_err(dev, "fb create failed(%d)", err);
	return ERR_PTR(err);
}

void inno_fbdev_destroy(struct inno_fbdev *inno_fbdev)
{
	struct inno_framebuffer *inno_fb;
	innodpu_gem_object *innodpu_obj;
	struct drm_framebuffer *fb;
	struct fb_info *info;
	struct innodpu_drm_private *dev_priv;
	struct device *dev;
	int i;
	struct drm_fb_helper *helper;

	if (!inno_fbdev)
		return;

	helper = &inno_fbdev->helper;
	dev_priv = inno_fbdev->priv;
	dev = dev_priv->dev;
#if (DRM_VERSION < KERNEL_VERSION(6, 2, 0))
	drm_fb_helper_unregister_fbi(&inno_fbdev->helper);
#else
	drm_fb_helper_unregister_info(&inno_fbdev->helper);
#endif
	inno_fb = &inno_fbdev->fb;

	innodpu_obj = to_innodpu_obj(inno_fb->obj[0]);
	if (innodpu_obj) {
#if (DRM_VERSION < KERNEL_VERSION(6,2,0))
		info = inno_fbdev->helper.fbdev;
#else
		info = inno_fbdev->helper.info;
#endif
		if (info && info->screen_base)
			iounmap((void __iomem *)info->screen_base);
	}

	for (i = 0; i < 4; i++) {
		drm_gem_object_put(inno_fb->obj[i]);
	}
	inno_drm_info(dev, "drm_gem_object_put");

#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
	for (i = 0; i < helper->connector_count; i++) {
		struct drm_connector *connector;
		struct drm_mode_object *obj;
		connector = helper->connector_info[i]->connector;
		obj = &connector->base;
		inno_drm_info(dev, "CONNECTOR OBJ ID: %d(%d)", obj->id, kref_read(&obj->refcount));
	}
#endif
	drm_fb_helper_fini(&inno_fbdev->helper);
	inno_drm_info(dev, "drm_fb_helper_fini");

	fb = to_drm_framebuffer(inno_fb);
	if (fb && fb->dev) {
		drm_framebuffer_cleanup(fb);
		inno_drm_info(dev, "drm_framebuffer_cleanup\n");
	}

	kfree(inno_fbdev);
}


#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
struct drm_framebuffer *inno_framebuffer_create(struct drm_device *drm_dev, struct drm_file *file,
#if (DRM_VERSION >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && (DRM_VERSION >= KERNEL_VERSION(3, 18, 0)))
												const
#endif
												struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);
	struct drm_gem_object *obj;
	struct inno_framebuffer *inno_fb;
	int err;

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		err = -EINVAL;
		goto err_out;
	}

	obj = drm_gem_object_lookup(file, mode_cmd->handles[0]);
	if (!obj) {
		DRM_ERROR("failed to find buffer with handle %u\n", mode_cmd->handles[0]);
		err = -ENOENT;
		goto err_out;
	}

	inno_fb = kzalloc(sizeof(*inno_fb), fh2m_hal_get_inno_gfp_kernel());
	if (!inno_fb) {
		err = -ENOMEM;
		goto err_obj_put;
	}

	err = inno_framebuffer_init(dev_priv, mode_cmd, inno_fb, obj);
	if (err) {
		DRM_ERROR("failed to initialise framebuffer (err=%d)\n", err);
		goto err_free_fb;
	}

	DRM_DEBUG_DRIVER("[FB:%d]\n", inno_fb->base.base.id);

	return &inno_fb->base;

err_free_fb:
	kfree(inno_fb);
err_obj_put:
	drm_gem_object_put(obj);

err_out:
	return ERR_PTR(err);
}

#endif /* (DRM_VERSION < KERNEL_VERSION(4, 14, 0)) */

//mode config的回调接口,用于创建framebuffer object 并绑定gem objects
struct drm_framebuffer *innodpu_fb_create(struct drm_device *drm_dev, struct drm_file *file,
#if (DRM_VERSION >= KERNEL_VERSION(4, 5, 0)) || \
		(defined(CHROMIUMOS_KERNEL) && (DRM_VERSION >= KERNEL_VERSION(3, 18, 0)))
										  const
#endif
										  struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_framebuffer *fb;
	int err;

	err = drm_mode_fb_cmd2_validate(mode_cmd);
	if (err) {
		fh2m_innodpu_err(drm_dev->dev, "%s, %d ret:%d\n", __func__, __LINE__, err);
		return ERR_PTR(err);
	}

	fb = drm_gem_fb_create(drm_dev, file, mode_cmd);
	if (IS_ERR(fb)) {
		fh2m_innodpu_err(drm_dev->dev, "%s	%d ret:%p\n", __func__, __LINE__, fb);
		goto out;
	}

	DRM_DEBUG_DRIVER("[FB:%d]\n", fb->base.id);
out:
	return fb;
}

int innodpu_fbdev_init(struct drm_device *drm_dev)
{
#if defined(CONFIG_DRM_FBDEV_EMULATION)
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);
	struct inno_fbdev *fbdev;
	int err;
	int retry = 4;

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "innodpu_drm_private does not find\n");
		return -EINVAL;
	}

	//创建fb_dev
	inno_drm_info(drm_dev->dev, "start create fbdev device\n");
	fbdev = inno_fbdev_create(dev_priv);
	if (!fbdev) {
		inno_drm_err(drm_dev->dev, "fbdev is NULL!\n");
		return -EINVAL;
	}
	if (IS_ERR(fbdev)) {
		inno_drm_err(drm_dev->dev, "failed to create a fb device\n");
		return PTR_ERR(fbdev);
	}
	dev_priv->fbdev = fbdev;

	/*
	 * pdpdrmfb is registered and available for userspace to use. If this
	 * is the only or primary device, fbcon has already bound a tty to it,
	 * and the following call will take no effect. However, this may be
	 * essential in order to sync the display when fbcon was already bound
	 * to a different tty (and fbdev). This triggers ->set_config() which
	 * will in turn set up a config and then do a modeset.
	 */
	inno_drm_info(drm_dev->dev, "drm_fb_helper_restore_fbdev_mode_unlocked\n");

	do {
		err = drm_fb_helper_restore_fbdev_mode_unlocked(&fbdev->helper);
		if (err) {
			inno_drm_err(drm_dev->dev, "failed to set mode (err=%d), master %#llx, retry-%d\n",
					err, drm_dev->master, retry);
			fh2m_inno_msleep(10);
		} else {
			break;
		}
	} while (--retry > 0);
	if (retry < 0) {
		inno_drm_err(drm_dev->dev, "failed to set mode (err=%d)\n", err);
		innodpu_bind_atomic(dev_priv->drm_dev, false);
		return err;
	}
	innodpu_bind_atomic(dev_priv->drm_dev, false);

#ifdef PM_VT_SWITCH_DISABLE
	pm_set_vt_switch(0);
#endif

#endif
	return 0;
}


void innodpu_fbdev_fini(struct drm_device *drm_dev)
{
#if defined(CONFIG_DRM_FBDEV_EMULATION)
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);

	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "innodpu_drm_private does not find");
		return;
	}
	inno_fbdev_destroy(dev_priv->fbdev);
#endif
	return;
}


#endif /* CONFIG_DRM_FBDEV_EMULATION */
