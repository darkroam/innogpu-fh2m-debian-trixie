/*************************************************************************/ /*!
@File			pdp0_plane.c
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
#include "pdp0_drv.h"
#include "pdp0_hw.h"
#include "pdp0_plane.h"
#include "innodpu_drm_fb.h"
#include "innogpu_drm.h"

// called by drm_mode_config_cleanup to clean states
static void pdp0_de_plane_destroy(struct drm_plane *plane)
{

#ifdef CONFIG_KALLSYMS
	fh2m_innodpu_info(plane->dev->dev, DPU_UT_DPU,
		"%s plane destroy by %pf", plane->name, __builtin_return_address(0));
#endif
#if (DRM_VERSION < KERNEL_VERSION(4, 15, 0))
	drm_plane_helper_disable(plane);	// BY_TBD：为什么会调用这个
	drm_plane_cleanup(plane);
#else
	drm_plane_cleanup(plane);
#endif
}

// called by drm_mode_config_reset to reset planes
// create the first state
static void pdp0_plane_reset(struct drm_plane *plane)
{
	struct pdp0_plane_state *state;
#if defined(CONFIG_KALLSYMS)
	fh2m_innodpu_info(plane->dev->dev, DPU_UT_DPU,  "%s plane reset by %pf, old_state is %pK",
		plane->name, __builtin_return_address(0), plane->state);
#endif
	if (plane->state) {
		state = to_pdp0_plane_state(plane->state);
		__drm_atomic_helper_plane_destroy_state(&state->base);
		kfree(state);
	}
	plane->state = NULL;
	state = kzalloc(sizeof(*state), fh2m_hal_get_inno_gfp_kernel());
	if (state) {
		state->base.plane = plane;
		state->base.rotation = DRM_MODE_ROTATE_0;
#if (DRM_VERSION > KERNEL_VERSION(4, 20, 0))
		state->base.alpha = DRM_BLEND_ALPHA_OPAQUE;
#endif
		plane->state = &state->base;

		// BY_TBD:初始状态赋值，这里是否可以使用上plane->base object进行赋值？
		state->priv_config.color = 0;
		state->priv_config.pixel_alpha = 0;
		state->priv_config.pre_alpha = 0;
		state->priv_config.layer_alpha = 0;
		state->priv_config.wh_scaler = 0x01000100;
		state->priv_config.rotation = 0;
	}

}

// called by drm_atomic_set_property to duplicate state
static struct drm_plane_state *pdp0_duplicate_plane_state(struct drm_plane *plane)
{
	struct pdp0_plane_state *pdp0_state, *old_state;

	if (!plane->state)
		return NULL;

	pdp0_state = kmalloc(sizeof(*pdp0_state), fh2m_hal_get_inno_gfp_kernel());
	if (!pdp0_state) {
		fh2m_innodpu_err(plane->dev->dev, "%s alloc plane state failed,short of memory\n", plane->name);
		return NULL;
	}

	old_state = to_pdp0_plane_state(plane->state);
	// memcpy plane->state to state
	__drm_atomic_helper_plane_duplicate_state(plane, &pdp0_state->base);
	pdp0_state->rotmem_size = old_state->rotmem_size;
	pdp0_state->format = old_state->format;
	pdp0_state->n_planes = old_state->n_planes;
	memcpy(&pdp0_state->priv_config, &old_state->priv_config, sizeof(pdp0_state->priv_config));

	return &pdp0_state->base;
}

// called by drm_plane_helper_commit or drm_plane_cleanup
static void pdp0_destroy_plane_state(struct drm_plane *plane, struct drm_plane_state *state)
{
	struct pdp0_plane_state *pdp0_state = NULL;

	if (state) {
		pdp0_state = to_pdp0_plane_state(state);
		__drm_atomic_helper_plane_destroy_state(state);
		kfree(pdp0_state);
	}
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
// called by drm_mode_atomic_ioctl debug to print states
static void pdp0_plane_atomic_print_state(struct drm_printer *p,
										  const struct drm_plane_state *state)
{
	struct pdp0_plane_state *pdp0_state = to_pdp0_plane_state(state);

	drm_printf(p, "\trotmem_size=%u\n", pdp0_state->rotmem_size);
	drm_printf(p, "\tformat_id=%u\n", pdp0_state->format);
	drm_printf(p, "\tplanes=%u\n", pdp0_state->n_planes);
	drm_printf(p, "\tcolor=%llu\n", pdp0_state->priv_config.color);
	drm_printf(p, "\tpixel_alpha=%llu\n", pdp0_state->priv_config.pixel_alpha);
	drm_printf(p, "\tpre_alpha=%llu\n", pdp0_state->priv_config.pre_alpha);
	drm_printf(p, "\tlayer_alpha=%llu\n", pdp0_state->priv_config.layer_alpha);
	drm_printf(p, "\twh_scaler=%llu\n", pdp0_state->priv_config.wh_scaler);
	drm_printf(p, "\trotation=%u\n", pdp0_state->priv_config.rotation);
}
#endif

static int pdp0_plane_atomic_set_property(struct drm_plane *plane,
										  struct drm_plane_state *state,
										  struct drm_property *property, uint64_t val)
{
	int ret = 0;
	struct innodpu_drm_private *dev_priv;
	struct drm_device *drm_dev = plane->dev;
	struct pdp0_plane_state *pdp0_state = to_pdp0_plane_state(state);

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

#define SET_PROPERTY(_tname, NAME, type) do { \
		if (dev_priv->plane_prop[INNODPU_PLANE_PROP_##NAME] == property) { \
			fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%s plane set property %s %lld\n", plane->name, property->name, val); \
			pdp0_state->priv_config._tname = (type)val;	\
			goto done; \
		} \
	} while(0)

	SET_PROPERTY(color, COLOR, uint64_t);
	SET_PROPERTY(pixel_alpha, PIXEL_ALPHA, uint64_t);
	SET_PROPERTY(pre_alpha, PRE_ALPHA, uint64_t);
	SET_PROPERTY(layer_alpha, LAYER_ALPHA, uint64_t);
	SET_PROPERTY(wh_scaler, WH_SCALER, uint64_t);
	SET_PROPERTY(rotation, ROTATE, uint32_t);
	fh2m_innodpu_err(drm_dev->dev, "%s plane set property failed,Invaild plane_state\n", plane->name);
	ret = -EINVAL;
#undef SET_PROPERTY

  done:
	return ret;
}

static int pdp0_plane_atomic_get_property(struct drm_plane *plane,
										  const struct drm_plane_state *state,
										  struct drm_property *property, uint64_t * val)
{
	int ret = 0;
	struct innodpu_drm_private *dev_priv;
	struct drm_device *drm_dev = plane->dev;
	struct pdp0_plane_state *pdp0_state = to_pdp0_plane_state(state);

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

#define GET_PROPERTY(_tname, NAME, type) do { \
		if (dev_priv->plane_prop[INNODPU_PLANE_PROP_##NAME] == property) { \
			*val = pdp0_state->priv_config._tname;	\
			fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%s plane get property %s %lld\n", plane->name, property->name, *val); \
			goto done; \
		} \
	} while(0)

	GET_PROPERTY(color, COLOR, uint64_t);
	GET_PROPERTY(pixel_alpha, PIXEL_ALPHA, uint64_t);
	GET_PROPERTY(pre_alpha, PRE_ALPHA, uint64_t);
	GET_PROPERTY(layer_alpha, LAYER_ALPHA, uint64_t);
	GET_PROPERTY(wh_scaler, WH_SCALER, uint64_t);
	GET_PROPERTY(rotation, ROTATE, uint32_t);
	fh2m_innodpu_err(drm_dev->dev, "%s plane get property failed,Invaild plane_state\n", plane->name);
	ret = -EINVAL;
#undef GET_PROPERTY

  done:
	return ret;
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
static bool innodpu_mod_supported(uint32_t format, uint64_t modifier)
{
	bool flag = false;
	switch (format) {
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_ARGB2101010:
		case DRM_FORMAT_ABGR2101010:
			if (modifier == INNODPU_FORMAT_MOD_AB24 || modifier == DRM_FORMAT_MOD_LINEAR)
				flag = true;
			break;
		default:
			flag = false;
	}
	return flag;
}


static bool pdp0_primary_plane_format_mod_supported(struct drm_plane *plane, uint32_t format, uint64_t modifier)
{
	if (WARN_ON(modifier == DRM_FORMAT_MOD_INVALID)){
		pr_err("modifier is DRM_FORMAT_MOD_INVALID!\n");
		return false;
	}
	if(modifier == 0x0)
		return true;

	return innodpu_mod_supported(format, modifier);

}

#endif

static const struct drm_plane_funcs pdp0_de_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = pdp0_de_plane_destroy,
	.reset = pdp0_plane_reset,
	.atomic_duplicate_state = pdp0_duplicate_plane_state,
	.atomic_destroy_state = pdp0_destroy_plane_state,
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	.atomic_print_state = pdp0_plane_atomic_print_state,
#endif
	.atomic_set_property = pdp0_plane_atomic_set_property,
	.atomic_get_property = pdp0_plane_atomic_get_property,
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	.format_mod_supported = pdp0_primary_plane_format_mod_supported
#endif
};

static int pdp0_se_check_scaling(struct pdp0_plane *mp, struct drm_plane_state *state)
{
	int ret = 0;
	u32 src_w, src_h;
	struct drm_crtc_state *crtc_state =
			drm_atomic_get_existing_crtc_state(state->state, state->crtc);
	struct pdp0_crtc_state *mc = NULL;
	struct drm_plane *plane = &mp->base;
#if (DRM_VERSION < KERNEL_VERSION(4, 15, 0))
	struct drm_rect clip = { 0 };
#endif

	if (!crtc_state) {
		fh2m_innodpu_err(plane->dev->dev, "crtc_state is NULL");
		return -EINVAL;
	}
	mc = to_pdp0_crtc_state(crtc_state);

#if (DRM_VERSION < KERNEL_VERSION(4, 15, 0))
	clip.x2 = crtc_state->adjusted_mode.hdisplay;
	clip.y2 = crtc_state->adjusted_mode.vdisplay;
	ret = drm_plane_helper_check_state(state, &clip, 0, INT_MAX, true, true);
#else
	ret = drm_atomic_helper_check_plane_state(state, crtc_state, 0, INT_MAX, true, true);
#endif

	if (ret) {
		fh2m_innodpu_err(plane->dev->dev, "drm_atomic_helper_check_plane_state check failed-%d\n", ret);
		return -EINVAL;
	}
	if (state->rotation & PDP0_ROTATED_MASK) {
		src_w = state->src_h >> 16;
		src_h = state->src_w >> 16;
	} else {
		src_w = state->src_w >> 16;
		src_h = state->src_h >> 16;
	}

	state->crtc_w = crtc_state->adjusted_mode.hdisplay;
	state->crtc_h = crtc_state->adjusted_mode.vdisplay;

	if ((state->crtc_w == src_w) && (state->crtc_h == src_h)) {
		/* Scaling not necessary for this plane. */
		mc->scaled_planes_mask &= ~(mp->layer->id);
		return 0;
	}

	mc->scaled_planes_mask |= mp->layer->id;
	/* Defer scaling requirements calculation to the crtc check. */
	return 0;

}

static int pdp0_fd_plane_atomic_check_legacy(struct drm_plane *plane,
					struct drm_plane_state *new_plane_state)
{
	struct pdp0_plane *mp = to_pdp0_plane(plane);
	struct pdp0_plane_state *ms = to_pdp0_plane_state(new_plane_state);

	struct drm_framebuffer *fb = NULL;
	int i, ret;
	u32 format;

	// u32 src_w, src_h, dest_w, dest_h, val;
	u64 rotate_val = ms->priv_config.rotation;

	if (!new_plane_state->crtc || !new_plane_state->fb)
		return 0;

	fb = new_plane_state->fb;
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	format = fb->format->format;
#else
	format = fb->pixel_format;
#endif
	ms->format = pdp0_hw_get_format_id(&mp->hwdev->map, mp->layer->id, format);
	if (ms->format == PDP0_INVALID_FORMAT_ID) {
		fh2m_innodpu_err(plane->dev->dev, "format id invaild");
		return -EINVAL;
	}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	ms->n_planes = fb->format->num_planes;
#else
	ms->n_planes = 1;
#endif
	for (i = 0; i < ms->n_planes; i++) {
		if (!pdp0_hw_pitch_valid(mp->hwdev, fb->pitches[i])) {
			inno_error("Invalid pitch %u for plane %d\n", fb->pitches[i], i);
			return -EINVAL;
		}
	}

	if ((new_plane_state->crtc_w > mp->hwdev->max_width) || (new_plane_state->crtc_h > mp->hwdev->max_width) ||
		(new_plane_state->crtc_w < mp->hwdev->min_width) || (new_plane_state->crtc_h < mp->hwdev->min_width)) {
		fh2m_innodpu_err(plane->dev->dev, "dpu-%d, crtc_w_h %dx%d failed, max_width: %d, min_width: %d\n",
					mp->hwdev->dpu_id, new_plane_state->crtc_w, new_plane_state->crtc_h,
					mp->hwdev->max_width, mp->hwdev->min_width);
		return -EINVAL;
	}

	/*
	 * PDP0/650 video layers can accept 3 plane formats only if
	 * fb->pitches[1] == fb->pitches[2] since they don't have a
	 * third plane stride register.
	 */
	if (ms->n_planes == 3 && !(mp->hwdev->features & PDP0_DEVICE_LV_HAS_3_STRIDES) &&
		(new_plane_state->fb->pitches[1] != new_plane_state->fb->pitches[2])) {
		fh2m_innodpu_err(plane->dev->dev, "pitches failed\n");
		return -EINVAL;
	}

	ret = pdp0_se_check_scaling(mp, new_plane_state);
	if (ret) {
		fh2m_innodpu_err(plane->dev->dev, "se check scaling failed");
		return ret;
	}

	/* packed RGB888 / BGR888 can't be rotated or flipped */
	if (new_plane_state->rotation != DRM_MODE_ROTATE_0
		&& (format == DRM_FORMAT_RGB888 || format == DRM_FORMAT_BGR888)) {
		fh2m_innodpu_err(plane->dev->dev, "rotation failed\n");
		return -EINVAL;
	}

	ms->rotmem_size = 0;
	if ((new_plane_state->rotation & PDP0_ROTATED_MASK) || (rotate_val & PDP0_ROTATED_MASK)) {
		int val;

		val =
			mp->hwdev->rotmem_required(mp->hwdev, new_plane_state->crtc_h,
									   new_plane_state->crtc_w, format);
		if (val < 0) {
			fh2m_innodpu_err(plane->dev->dev, "rom required failed\n");
			return val;
		}
		ms->rotmem_size = val;
	}

	return 0;
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 13, 0))
static int pdp0_fd_plane_atomic_check(struct drm_plane *plane,
					struct drm_atomic_state *atomic_state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(atomic_state, plane);
	return pdp0_fd_plane_atomic_check_legacy(plane, new_plane_state);
}
#endif

static void pdp0_fd_plane_atomic_update_legacy(struct drm_plane *plane,
					struct drm_plane_state *old_state)
{
	struct pdp0_plane *pdp0_plane = to_pdp0_plane(plane);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_plane->hwdev;

	if (hwdev->fd_plane_update) {
		hwdev->fd_plane_update(plane, old_state);
	}
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 13, 0))
static void pdp0_fd_plane_atomic_update(struct drm_plane *plane,
					struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state, plane);

	pdp0_fd_plane_atomic_update_legacy(plane, old_state);
}
#endif

static void pdp0_fd_plane_atomic_disable_legacy(struct drm_plane *plane,
					struct drm_plane_state *state)
{
	struct pdp0_plane *pdp0_plane = to_pdp0_plane(plane);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_plane->hwdev;

	if (hwdev->fd_plane_disable) {
		hwdev->fd_plane_disable(plane, state);
	}
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 13, 0))
static void pdp0_fd_plane_atomic_disable(struct drm_plane *plane,
					struct drm_atomic_state *atomic_state)
{
	struct drm_plane_state *state = drm_atomic_get_old_plane_state(atomic_state, plane);

	pdp0_fd_plane_atomic_disable_legacy(plane, state);
}
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 13, 0))
#include <drm/drm_gem_atomic_helper.h>
static const struct drm_plane_helper_funcs pdp0_de_plane_helper_funcs = {
	.prepare_fb = drm_gem_plane_helper_prepare_fb,
	.atomic_check = pdp0_fd_plane_atomic_check,
	.atomic_update = pdp0_fd_plane_atomic_update,
	.atomic_disable = pdp0_fd_plane_atomic_disable,
};
#else
static const struct drm_plane_helper_funcs pdp0_de_plane_helper_funcs = {
#if (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
	.prepare_fb = drm_gem_fb_prepare_fb,
#endif
	.atomic_check = pdp0_fd_plane_atomic_check_legacy,
	.atomic_update = pdp0_fd_plane_atomic_update_legacy,
	.atomic_disable = pdp0_fd_plane_atomic_disable_legacy,
};
#endif

static void innodpu_plane_attach_properties(struct innodpu_drm_private *dev_priv,
											struct drm_plane *plane)
{
	/* attach private property */
#define ATTACH_PROPERTY(prop, initval) \
		drm_object_attach_property(&plane->base, prop, initval)

	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_COLOR], 0);
	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_PIXEL_ALPHA], 0);
	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_LAYER_ALPHA], 0);
	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_PRE_ALPHA], 0);
	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_WH_SCALER], 0x01000100);
	ATTACH_PROPERTY(dev_priv->plane_prop[INNODPU_PLANE_PROP_ROTATE], 0);

#undef ATTACH_PROPERTY
}



#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
static const uint64_t pdp_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	INNODPU_FORMAT_MOD_AB24,
	DRM_FORMAT_MOD_INVALID,
};

static const uint64_t default_modifiers[] = {
		DRM_FORMAT_MOD_LINEAR,
		DRM_FORMAT_MOD_INVALID
};
#endif

int innodpu_pdp0_de_planes_init(struct innodpu_pdp0_drm *pdp0_drm)
{
	int i = 0;
	int j = 0;
	int n = 0;
	int ret = 0;

	struct pdp0_plane *plane = NULL;
	struct drm_device *drm_dev = pdp0_drm->drm_dev;
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);
	unsigned long possible_crtcs = INNO_BIT(drm_dev->mode_config.num_crtc);
	const uint64_t *supported_modifiers = NULL;

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	unsigned long flags = DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_180 |
		DRM_MODE_ROTATE_270 | DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y;
#endif

#if (DRM_VERSION > KERNEL_VERSION(4, 20, 0))
	unsigned long blend_caps = INNO_BIT(DRM_MODE_BLEND_PIXEL_NONE) |
		INNO_BIT(DRM_MODE_BLEND_PREMULTI) | INNO_BIT(DRM_MODE_BLEND_COVERAGE);
#endif

	u32 *formats = NULL;
	const struct pdp0_hw_regmap *map = &pdp0_drm->hwdev->map;

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

	formats = kcalloc(map->n_pixel_formats, sizeof(*formats), fh2m_hal_get_inno_gfp_kernel());
	if (!formats) {
		fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d plane failed, Short of memory\n", pdp0_drm->dpu_id);
		return -ENOMEM;
	}

	for (i = 0; i < map->n_layers; i++) {
		u8 id = map->layers[i].id;

		plane = devm_kzalloc(pdp0_drm->dev, sizeof(*plane), fh2m_hal_get_inno_gfp_kernel());
		if (!plane) {
			fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d plane %d alloc failed\n", pdp0_drm->dpu_id, i);
			ret = -ENOMEM;
			goto err_init;
		}
		pdp0_drm->plane[i] = plane;
		plane->crtc = &pdp0_drm->crtc;
		plane->id = id;

		/* build the list of DRM supported formats based on the map */
		for (n = 0, j = 0; j < map->n_pixel_formats; j++) {
			if ((map->pixel_formats[j].layer & id) == id)
				formats[n++] = map->pixel_formats[j].format;
		}

		/* n is format_count */
		if (id == PDP0_OW1) {
			plane->plane_type = DRM_PLANE_TYPE_PRIMARY;

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
#if (DRM_VERSION >= KERNEL_VERSION(5, 18, 0))
			if((!drm_dev->mode_config.fb_modifiers_not_supported) && (pdp0_drm->hwdev->features & INNO_PDP_PVRIC))
#else
			if (drm_dev->mode_config.allow_fb_modifiers && (pdp0_drm->hwdev->features & INNO_PDP_PVRIC))
#endif
				supported_modifiers = pdp_modifiers;
			else
				supported_modifiers = default_modifiers;

#else
		supported_modifiers = NULL;
#endif
		}

		ret = drm_universal_plane_init(drm_dev, &plane->base, possible_crtcs,
									   &pdp0_de_plane_funcs, formats,
									   n,
									   supported_modifiers, plane->plane_type, "dpu_id:%d-ow:%d", pdp0_drm->dpu_id,
									   i);
		if (ret < 0) {
			fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d plane %d init failed\n", pdp0_drm->dpu_id, i);
			pdp0_drm->plane[i] = NULL;
			goto err_init;
		}
		drm_plane_helper_add(&plane->base, &pdp0_de_plane_helper_funcs);
		plane->hwdev = pdp0_drm->hwdev;
		plane->layer = &map->layers[i];
#ifdef CONFIG_KALLSYMS
		if (s_pdp0_debug) {
			fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%p plane %s  type(%d) create",
					 plane, plane->base.name, plane->plane_type);
		}
#endif

#if (DRM_VERSION > KERNEL_VERSION(4, 20, 0))
		drm_plane_create_alpha_property(&plane->base);
		drm_plane_create_blend_mode_property(&plane->base, blend_caps);
#endif
		innodpu_plane_attach_properties(dev_priv, &plane->base);

		fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "[%s]%s,id:%d\n",
				 plane->layer->name, plane->base.name, plane->base.base.id);
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
		drm_plane_create_rotation_property(&plane->base, DRM_MODE_ROTATE_0, flags);
#endif
	}
	kfree(formats);

	return 0;

  err_init:
	for (--i; i >= 0; --i) {
		// drm_plane_cleanup(&pdp0_drm->plane[i]->base);
		pdp0_de_plane_destroy(&plane->base);
	}
	kfree(plane);
	if (formats) {
		kfree(formats);
		formats = 0;
	}
	return ret;
}

void innodpu_pdp0_de_planes_destroy(struct innodpu_pdp0_drm *pdp0_drm)
{
	int i = 0;
	struct pdp0_plane *plane = NULL;

	for (i = 0; i < PDP0_LAYERS && pdp0_drm->plane[i]; ++i) {
		plane = pdp0_drm->plane[i];
		pdp0_de_plane_destroy(&plane->base);
	}
}
