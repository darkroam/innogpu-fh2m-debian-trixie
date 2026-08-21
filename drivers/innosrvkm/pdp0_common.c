#include "inno_drm_version.h"
#include <linux/component.h>
#include <linux/platform_device.h>

#include "pdp0_drv.h"
#include "pdp0_crtc.h"
#include "pdp0_plane.h"
#include "pdp0_common.h"
#include "innodpu_drm_gem.h"

struct innodpu_pdp0_hw_device *pdp0_crtc_to_hwdev(inno_drm_crtc *crtc)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);

	if (WARN_ON(!pdp0_drm)) {
		return NULL;
	}
	return pdp0_drm->hwdev;
}

static __maybe_unused innodpu_gem_object *pdp0_get_innodpu_obj(void *obj)
{
	return to_innodpu_obj(obj);
}

u64 pdp0_get_fb_dev_paddr(struct innodpu_pdp0_hw_device *hwdev, void *inno_fb)
{
	void *get_obj = hwdev->get_gem(inno_fb);

	return fh2m_innodpu_gem_get_dev_paddr(get_obj);
}

u64 pdp0_get_fb_cpu_paddr(struct innodpu_pdp0_hw_device *hwdev, void *inno_fb)
{
	void *get_obj = hwdev->get_gem(inno_fb);

	return fh2m_innodpu_gem_get_cpu_paddr(get_obj);
}

unsigned long pdp0_gem_get_dev_paddr(inno_drm_gem_object * obj)
{
	return fh2m_innodpu_gem_get_dev_paddr(obj);
}

unsigned long pdp0_gem_get_cpu_paddr(inno_drm_gem_object * obj)
{
	return fh2m_innodpu_gem_get_cpu_paddr(obj);
}

struct pdp0_plane_config *pdp0_plane_state_get_config(inno_drm_plane *plane)
{
	struct drm_plane_state *plane_state = ((struct drm_plane *)plane)->state;
	struct pdp0_plane_state *state = to_pdp0_plane_state(plane_state);


	return &state->priv_config;
}

u8 *pdp0_plane_state_get_format(inno_drm_plane *plane)
{
	struct drm_plane_state *plane_state = ((struct drm_plane *)plane)->state;
	struct pdp0_plane_state *state = to_pdp0_plane_state(plane_state);


	return &state->format;
}

u8 *pdp0_plane_state_get_nplane(inno_drm_plane *plane)
{
	struct drm_plane_state *plane_state = ((struct drm_plane *)plane)->state;
	struct pdp0_plane_state *state = to_pdp0_plane_state(plane_state);


	return &state->n_planes;
}

const struct pdp0_layer *pdp0_plane_get_layer(inno_drm_plane *plane)
{
	struct pdp0_plane *pla = to_pdp0_plane(plane);

	return pla->layer;
}

struct innodpu_pdp0_hw_device *pdp0_plane_get_hwdev(inno_drm_plane *plane)
{
	struct pdp0_plane *pla = to_pdp0_plane(plane);

	return pla->hwdev;
}

u32 *pdp0_crtc_state_get_gamma_coeffs(inno_drm_crtc_state *crtc_state)
{
	struct pdp0_crtc_state *st = to_pdp0_crtc_state(crtc_state);

	return st->gamma_coeffs;
}

bool pdp0_crtc_state_get_coladj_en(inno_drm_crtc_state *crtc_state)
{
	struct pdp0_crtc_state *st = to_pdp0_crtc_state(crtc_state);

	return st->coladj_en;
}


u32 *pdp0_crtc_state_get_coloradj_coeffs(inno_drm_crtc_state *crtc_state)
{
	struct pdp0_crtc_state *st = to_pdp0_crtc_state(crtc_state);

	return st->coloradj_coeffs;
}

struct pdp0_se_config *pdp0_crtc_state_get_scaler_config(inno_drm_crtc_state *crtc_state)
{
	struct pdp0_crtc_state *st = to_pdp0_crtc_state(crtc_state);

	return &st->scaler_config;
}

struct pdp0_crtc_config *pdp0_crtc_state_get_priv_config(inno_drm_crtc_state *crtc_state)
{
	struct pdp0_crtc_state *st = to_pdp0_crtc_state(crtc_state);

	return &st->priv_config;
}

u16 pdp0_get_plane_layer_base(struct innodpu_pdp0_hw_device *hwdev, int num)
{
	return hwdev->plane[num]->layer->base;
}

void *pdp0_get_inno_framebuffer(void *fb)
{
	return to_inno_framebuffer(fb);
}

bool pdp0_is_alpha_support(uint32_t format)
{
	if((format == DRM_FORMAT_ARGB8888) || \
	   (format == DRM_FORMAT_ABGR8888) || \
	   (format == DRM_FORMAT_RGBA8888) || \
	   (format == DRM_FORMAT_BGRA8888)
	  ){
		return true;
	}else
		return false;
}


#if (DRM_VERSION > KERNEL_VERSION(4, 20, 0))
u32 pdp0_set_alpha_blend_mode(inno_drm_plane *inno_plane, inno_drm_framebuffer *inno_fb,
									 u32 layer_ctrl)
{
	struct drm_plane *plane = (struct drm_plane *)inno_plane;
	struct drm_framebuffer *fb = (struct drm_framebuffer *)inno_fb;
	struct drm_plane_state *plane_state = plane->state;

	u16 pixel_alpha = plane_state->pixel_blend_mode;
	u8 plane_alpha = plane_state->alpha >> 8;
	layer_ctrl &= ~(LAYER_COMP_MASK | LAYER_PMUL_ENABLE | LAYER_ALPHA_VALUE(0xff));

	if (plane_state->alpha == DRM_BLEND_ALPHA_OPAQUE) {
		if (pdp0_is_alpha_support(inno_drm_fb_format(fb))) {
			layer_ctrl |= LAYER_ALPHA_VALUE(0xff);
			layer_ctrl |= LAYER_COMP_PIXEL;
		} else {
			layer_ctrl |= LAYER_ALPHA_VALUE(0xff);
			layer_ctrl |= LAYER_COMP_LAYER;
		}
	} else if (plane_state->fb->format->has_alpha) {
		switch (pixel_alpha) {
		case DRM_MODE_BLEND_PIXEL_NONE:
			inno_error("DRM_MODE_BLEND_PIXEL_NONE\n");
			layer_ctrl |= LAYER_COMP_PIXEL;
			break;
		case DRM_MODE_BLEND_COVERAGE:
			inno_error("DRM_MODE_BLEND_COVERAGE\n");
			layer_ctrl |= LAYER_ALPHA_VALUE((plane_alpha - 1));
			layer_ctrl |= LAYER_COMP_LAYER;
			break;
		case DRM_MODE_BLEND_PREMULTI:
			inno_error("DRM_MODE_BLEND_PREMULTI\n");
			layer_ctrl |= LAYER_COMP_PIXEL | LAYER_PMUL_ENABLE;
			break;
		}
	}

	return layer_ctrl;
}
#else
u32 pdp0_set_alpha_blend_mode(inno_drm_plane *inno_plane, inno_drm_framebuffer *inno_fb,
									 u32 layer_ctrl)
{
	return layer_ctrl;
}
#endif

unsigned short inno_drm_display_mode_get_hdisplay(
		const inno_drm_display_mode *inno_dmode)
{
	struct drm_display_mode *dmode= (struct drm_display_mode *)inno_dmode;
	return dmode->hdisplay;
}

unsigned short inno_drm_display_mode_get_vdisplay(
		const inno_drm_display_mode *inno_dmode)
{
	struct drm_display_mode *dmode= (struct drm_display_mode *)inno_dmode;
	return dmode->vdisplay;
}

void inno_drm_display_mode_to_videomode(const inno_drm_display_mode *inno_dmode,
													inno_videomode *inno_vm)
{
	struct drm_display_mode *dmode= (struct drm_display_mode *)inno_dmode;
	struct videomode *vm = (struct videomode *)inno_vm;

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

u32 INNO_PADDING_ALIGN_SIZE_FUNC(void)
{
	return PADDING_ALIGN_SIZE;
}

u32 INNO_PDP0_CONFIG_VALID_DONE_FUNC(void)
{
	return PDP0_CONFIG_VALID_DONE;
}
