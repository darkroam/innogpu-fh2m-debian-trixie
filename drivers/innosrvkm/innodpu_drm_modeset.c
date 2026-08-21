#include <linux/version.h>
/*************************************************************************/ /*!
@File			innodpu_drm_modeset.c
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
#include "innodpu_common.h"
#include "innodpu_drm_fb.h"
#include "innodpu_drm_modeset.h"
#include "pdp0_drv.h"
#include "innogpu_drm.h"

static bool s_async_flip_enable = false;
module_param(s_async_flip_enable, bool, 0444);
MODULE_PARM_DESC(s_async_flip_enable, "Enable support for 'faked' async flipping (default: N)");

static bool s_allow_fb_modifiers = true;
module_param(s_allow_fb_modifiers, bool, 0444);
MODULE_PARM_DESC(s_allow_fb_modifiers, "Enable support fb modifier (default: Y)");


static int innodpu_drm_create_common_properties(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	struct drm_property *prop;

	// !!! Linux kernel 4.18 supports most blend_mode
#define CREATE_CRTC_PROPERTY(name, NAME, fnc, ...) do { \
		prop = drm_property_##fnc(drm_dev, 0, #name, \
			##__VA_ARGS__); \
		if(!prop) { \
			inno_drm_err(drm_dev->dev, "create crtc property failed"); \
			return -ENOMEM; \
		} \
		dev_priv->crtc_prop[INNODPU_CRTC_PROP_##NAME] = prop; \
	} while(0)
#define CREATE_CRTC_RANGE_PROPERTY(name, NAME, min, max) \
	CREATE_CRTC_PROPERTY(name, NAME, create_range, min, max);
#define CREATE_CRTC_BOOL_PROPERTY(name, NAME) \
	CREATE_CRTC_PROPERTY(name, NAME, create_bool);

#define CREATE_PLANE_PROPERTY(name, NAME, fnc, ...) do { \
		prop = drm_property_##fnc(drm_dev, 0, #name, \
			##__VA_ARGS__); \
		if(!prop) { \
			inno_drm_err(drm_dev->dev, "create plane property failed"); \
			return -ENOMEM; \
		} \
		dev_priv->plane_prop[INNODPU_PLANE_PROP_##NAME] = prop; \
	} while(0)
#define CREATE_PLANE_RANGE_PROPERTY(name, NAME, min, max) \
	CREATE_PLANE_PROPERTY(name, NAME, create_range, min, max)
#define CREATE_PLANE_BOOL_PROPERTY(name, NAME) \
	CREATE_PLANE_PROPERTY(name, NAME, create_bool)
#define CREATE_PLANE_BITMASK_PROPERTY(name, NAME, supported_bit) \
	CREATE_PLANE_PROPERTY(name, NAME, create_bitmask, name##_props, \
		INNO_ARRAY_SIZE(name##_props), supported_bit)

	CREATE_PLANE_RANGE_PROPERTY(color, COLOR, 0, 9);
#if 1
	CREATE_PLANE_RANGE_PROPERTY(pixel_alpha, PIXEL_ALPHA, 0, 256);
	CREATE_PLANE_RANGE_PROPERTY(pre_alpha, PRE_ALPHA, 0, 256);
	CREATE_PLANE_RANGE_PROPERTY(layer_alpha, LAYER_ALPHA, 0, 256);
	CREATE_PLANE_RANGE_PROPERTY(wh_scaler, WH_SCALER, 0x00190019, 0x04000400);
	CREATE_PLANE_RANGE_PROPERTY(rotate, ROTATE, 0, 5);
#else
	{
		unsigned long supported_rotations =
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_180 | DRM_MODE_ROTATE_270 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y;
		static const struct drm_prop_enum_list rotate_props[] = {
			{__builtin_ffs(DRM_MODE_ROTATE_0) - 1, "rotate-0"},
			{__builtin_ffs(DRM_MODE_ROTATE_90) - 1, "rotate-90"},
			{__builtin_ffs(DRM_MODE_ROTATE_180) - 1, "rotate-180"},
			{__builtin_ffs(DRM_MODE_ROTATE_270) - 1, "rotate-270"},
			{__builtin_ffs(DRM_MODE_REFLECT_X) - 1, "reflect-x"},
			{__builtin_ffs(DRM_MODE_REFLECT_Y) - 1, "reflect-y"},
		};

		// BY_TBD：这里要改为向后兼容版本，使用blend_mode和alpha共同表示alpha
		CREATE_PLANE_RANGE_PROPERTY(pixel_alpha, PIXEL_ALPHA, 0, 256);
		CREATE_PLANE_BITMASK_PROPERTY(rotate, ROTATE, supported_rotations);
	}
#endif

	CREATE_CRTC_RANGE_PROPERTY(wb_start, WB_START, 0x0, 0x8);
	CREATE_CRTC_RANGE_PROPERTY(wb_save_frames, WB_SAVE, 0x0, 0xffffffff);

	CREATE_CRTC_BOOL_PROPERTY(pvric_enable, PVR_ENABLE);
	CREATE_CRTC_BOOL_PROPERTY(pvric_type, PVR_TYPE);
	CREATE_CRTC_BOOL_PROPERTY(bist, BIST);
	CREATE_CRTC_BOOL_PROPERTY(yuv_fmt, ISYUV);
	CREATE_CRTC_RANGE_PROPERTY(display_id, DISPLAY_ID, 0x0, 0x03);
	CREATE_CRTC_BOOL_PROPERTY(uv_revs, UVREVS);
	CREATE_CRTC_BOOL_PROPERTY(comp_tile_4x16, COMP_TILE_4x16);
	CREATE_CRTC_RANGE_PROPERTY(set_wm_fd, SET_WM_FD, 0x0, 0xffffffff);
	CREATE_CRTC_RANGE_PROPERTY(decomp_type, DECOMP_TYPE, 0x0, 0x5);
	CREATE_CRTC_RANGE_PROPERTY(decomp_addr, DECOMP_ADDR, 0X0, 0xffffffff);

	/*DB9000 私有属性 */
	CREATE_CRTC_RANGE_PROPERTY(wb_prop, WB_PROP, 0x0, 0xffffffff);
	CREATE_CRTC_RANGE_PROPERTY(y_addr, Y_ADDR, 0x0, 0xffffffff);
	CREATE_CRTC_RANGE_PROPERTY(uv_addr, UV_ADDR, 0x0, 0xffffffff);
	CREATE_CRTC_RANGE_PROPERTY(ow_width, OW_WIDTH, 0x0, 0xffffffff);
	CREATE_CRTC_RANGE_PROPERTY(ow_height, OW_HEIGHT, 0x0, 0xffffffff);

#undef CREATE_PLANE_BOOL_PROPERTY
#undef CREATE_CRTC_BOOL_PROPERTY
#undef CREATE_PLANE_RANGE_PROPERTY
#undef CREATE_CRTC_RANGE_PROPERTY

	// connector_properties
	// encoder_properties
	return 0;
}

static int innodpu_atomic_check(struct drm_device *drm_dev, struct drm_atomic_state *state)
{
	int ret;
	int tmp_i = 0;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_crtc_state = NULL, *new_crtc_state = NULL;

#if 0
	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (new_crtc_state->enable) {
			fh2m_innodpu_err(state->dev->dev, "........\n");
			fh2m_innodpu_err(drm_dev->dev,	"%s mode: "DRM_MODE_FMT ", status:%d, crtc_clk = %d\n",
				crtc->name, DRM_MODE_ARG(&new_crtc_state->adjusted_mode), new_crtc_state->adjusted_mode.status, new_crtc_state->adjusted_mode.crtc_clock);
			if ( new_crtc_state->adjusted_mode.crtc_clock == 0)
				dump_stack();
		}
	}

	// 解决modetest出图结束后，背景仍然可出现问题：
	// http://confluence.srv/pages/viewpage.action?pageId=36767344
	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, tmp_i) {
		// 如果当前crtc的primary图层没有使能（没有crtc），那么关闭,否则crtc不会进入atomic_disable
		if (new_crtc_state->active && !new_crtc_state->plane_mask) {
			new_crtc_state->active = false;
		}
	}
#endif

	ret = drm_atomic_helper_check_modeset(drm_dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_normalize_zpos(drm_dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_helper_check_planes(drm_dev, state);
	if (ret)
		return ret;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, tmp_i) {
		// if (new_crtc_state->connectors_changed)
		if (new_crtc_state->connectors_changed || (new_crtc_state->active_changed && new_crtc_state->active))
			new_crtc_state->mode_changed = true;	// crtc_set_mode need mode_chaged
	}

	// 麒麟系统在仅HDMI显示时(比如仅DP显示->仅HDMI显示），只会get_mode，不一定会mode_changed
	// 这部分理应在上边解决，持续跟踪一段时间
	{
		int i = 0;
		struct drm_connector *connector = NULL;
		struct drm_crtc_state *crtc_state;
		struct drm_connector_state *old_connector_state = NULL, *new_connector_state = NULL;
		for_each_oldnew_connector_in_state(state, connector, old_connector_state,
										   new_connector_state, i) {
			if (old_connector_state->crtc != new_connector_state->crtc) {
				if (new_connector_state->crtc) {
					crtc_state = drm_atomic_get_new_crtc_state(state, new_connector_state->crtc);
					// 默认update_connector_routing 只开启了connectors_changed，但不会进HDMI/DP的modeset
					if (!crtc_state->mode_changed) {
						kms_info(drm_dev->dev, \
						"innodpu_atomic_check connector changed-%d, mode not changed, why?",
							 crtc_state->connectors_changed);
						crtc_state->mode_changed = true;
					}
				}
			}
		}
	}

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, tmp_i) {
		if (innodpu_modes_equal(&old_crtc_state->adjusted_mode, &new_crtc_state->adjusted_mode)) {
			new_crtc_state->mode_changed = false;
		}
		if (old_crtc_state->active == false && new_crtc_state->active == true) {
			new_crtc_state->mode_changed = true;
		}
	}

	return ret;
}

static int innodpu_drm_atomic_helper_commit(struct drm_device *dev,
											struct drm_atomic_state *state, bool nonblock)
{
	return drm_atomic_helper_commit(dev, state, nonblock);
}

static void __attribute__((unused)) innodpu_output_poll_changed(struct drm_device *drm_dev)
{
	struct innodpu_drm_private *dev_priv = NULL;
	struct fb_info *info = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv || !dev_priv->fbdev)
		return;
#if (DRM_VERSION < KERNEL_VERSION(6, 2, 0))
	info = dev_priv->fbdev->helper.fbdev;
#else
	info = dev_priv->fbdev->helper.info;
#endif
	if (info && info->state != FBINFO_STATE_RUNNING) {
		return;
	}

	drm_fb_helper_hotplug_event(&(dev_priv->fbdev->helper));
}

static const struct drm_mode_config_funcs inno_mode_config_funcs = {
	.fb_create = innodpu_fb_create,

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
	.output_poll_changed = innodpu_output_poll_changed,
#endif
#if defined(PDP_USE_ATOMIC)
	.atomic_check = innodpu_atomic_check,
	.atomic_commit = innodpu_drm_atomic_helper_commit,
#endif
};

static void inno_drm_atomic_helper_commit_hw_done(struct drm_atomic_state *old_state)
{
	int dpu_id;
	int i = 0, ret = 0;
	struct drm_pending_vblank_event *event;
	struct drm_device *drm = old_state->dev;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_crtc_state = NULL, *new_crtc_state = NULL;
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct innodpu_pdp0_hw_device *hwdev = NULL;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		dpu_id = innodpu_get_dpuid_bycrtc(crtc);
		if (dpu_id >= fh2m_hal_get_dev_nums(drm->dev, DEV_DB9000) || fh2m_hal_get_nulldisplay()) {
			if (crtc->enabled) {
				pdp0_drm = crtc_to_pdp0_device(crtc);
				hwdev = pdp0_drm->hwdev;
				if (crtc->state->event)
					ret = drm_crtc_vblank_get(crtc);
				/* only set config_valid if the CRTC is enabled */
				if (hwdev->set_and_wait_config_valid(hwdev))
					kms_info(crtc->dev->dev, "%s wait vblank timed out\n",
							 crtc->name);
				if (crtc->state->event && ret == 0)
					drm_crtc_vblank_put(crtc);
			}
			event = crtc->state->event;
			if (event) {
				// de_irq vsync => innodp_crtc_handle_page_flip wake
				// innodp->event[innodp_idx] = new_crtc_state->event;
				crtc->state->event = NULL;
				spin_lock_irq(&drm->event_lock);
				drm_crtc_send_vblank_event(crtc, event);
				spin_unlock_irq(&drm->event_lock);
			}
		}
	}
	drm_atomic_helper_commit_hw_done(old_state);
}

static void inno_atomic_state_dump(struct drm_atomic_state *old_state)
{
	{
		int i = 0;
		int dpu_id;

		struct drm_crtc *crtc = NULL;
		struct drm_crtc_state *old_crtc_state = NULL;
		struct drm_crtc_state *new_crtc_state = NULL;

		for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
			if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
				continue;

			dpu_id = innodpu_get_dpuid_bycrtc(crtc);
			kms_info(old_state->dev->dev, "dpu-%d %s, %s, %s(%d-%d-%d), %s",
					dpu_id, new_crtc_state->enable ? "enabled" : "disabled",
					new_crtc_state->active ? "actived" : "inactived",
					drm_atomic_crtc_needs_modeset(new_crtc_state) ? "modeset" : "no modeset",
					new_crtc_state->mode_changed, new_crtc_state->active_changed,
					new_crtc_state->connectors_changed,
					new_crtc_state->planes_changed ? "update_fb" : "keep_fb");
		}
	}
}

// called by commit_tail
static void inno_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	inno_atomic_state_dump(old_state);

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

#if 0
	kms_info(old_state->dev->dev, "set crtc writeback\n");
	// 向后兼容所以回写放在这里实现。如果不考虑兼容，可以在crtc->atomic_flush中更新回写

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		dpu_id = innodpu_get_dpuid_bycrtc(crtc);
		kms_info(old_state->dev->dev, "dpu%d set crtc writeback\n", dpu_id);
		if (dpu_id >= fh2m_hal_get_dev_nums(drm->dev, DEV_DB9000)) {
			pdp0_mw_atomic_commit(crtc, old_crtc_state);
		}
	}
#endif

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	inno_drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static void innodpu_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	inno_atomic_commit_tail(old_state);
}

static struct drm_mode_config_helper_funcs inno_mode_config_helpers = {
	.atomic_commit_tail = innodpu_atomic_commit_tail,
};

int innodpu_modeset_early_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	drm_mode_config_init(drm_dev);
	drm_dev->mode_config.funcs = &inno_mode_config_funcs;
	drm_dev->mode_config.helper_private = &inno_mode_config_helpers;
	drm_dev->mode_config.min_width = INNODPU_WIDTH_MIN;
	drm_dev->mode_config.min_height = INNODPU_HEIGHT_MIN;
	drm_dev->mode_config.cursor_width = INNODPU_CURSOR_WIDTH;
	drm_dev->mode_config.cursor_height = INNODPU_CURSOR_HEIGHT;
	drm_dev->mode_config.max_width = fh2m_hal_get_s_max_width();
	drm_dev->mode_config.max_height = fh2m_hal_get_s_max_height();

	inno_drm_info(drm_dev->dev, "max_width is %d, max_height is %d\n", fh2m_hal_get_s_max_width(), fh2m_hal_get_s_max_height());

#if (DRM_VERSION < KERNEL_VERSION(6, 1, 0))
	drm_dev->mode_config.fb_base = 0;
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	drm_dev->mode_config.async_page_flip = s_async_flip_enable;
#else
	drm_dev->mode_config.async_page_flip = false; // 4.14 support async
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 18, 0))
	drm_dev->mode_config.fb_modifiers_not_supported = !s_allow_fb_modifiers;
#else
	drm_dev->mode_config.allow_fb_modifiers = s_allow_fb_modifiers;
#endif

	inno_drm_info(drm_dev->dev, "async flip is %s\n", s_async_flip_enable ? "enabled" : "disabled");

	return innodpu_drm_create_common_properties(drm_dev, dev_priv);
}

int innodpu_modeset_late_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	inno_drm_info(drm_dev->dev, "mode config reset");
	drm_mode_config_reset(drm_dev);

	drm_kms_helper_poll_init(drm_dev);
	return 0;
}
