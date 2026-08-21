#ifndef __PDP0_COMMON_H
#define __PDP0_COMMON_H

#include "inno_debug.h"
#include "inno_misc.h"
#include "inno_plat_dev.h"
#include "inno_task.h"
#include "inno_lock.h"
#include "inno_timer.h"
#include "inno_mm.h"
#include "inno_waitqueue.h"
#include "inno_drm.h"
#include "inno_drm_mode.h"
#include "inno_fs.h"
#include "inno_interrupt.h"


struct pdp0_plane_config {
	uint64_t color;
	uint64_t pixel_alpha;
	uint64_t pre_alpha;
	uint64_t layer_alpha;
	uint64_t wh_scaler;
	uint32_t rotation;
};

struct pdp0_layer {
	u8 name[16];
	u16 id;
	u16 base;
	u16 ptr;
	u16 stride_offset;
};

struct pdp0_se_config {
	u8 scale_enable:1;
	u8 enhancer_enable:1;
	u8 hcoeff:3;
	u8 vcoeff:3;
	u8 plane_src_id;
	u16 input_w, input_h;
	u16 output_w, output_h;
	u32 h_init_phase, h_delta_phase;
	u32 v_init_phase, v_delta_phase;
};

struct innodpu_pdp0_hw_device *pdp0_crtc_to_hwdev(inno_drm_crtc *crtc);
struct pdp0_plane_config *pdp0_plane_state_get_config(inno_drm_plane *plane);
u8 *pdp0_plane_state_get_format(inno_drm_plane *plane);
u8 *pdp0_plane_state_get_nplane(inno_drm_plane *plane);
const struct pdp0_layer *pdp0_plane_get_layer(inno_drm_plane *plane);
struct innodpu_pdp0_hw_device *pdp0_plane_get_hwdev(inno_drm_plane *plane);
u32 *pdp0_crtc_state_get_gamma_coeffs(inno_drm_crtc_state *crtc_state);
bool pdp0_crtc_state_get_coladj_en(inno_drm_crtc_state *crtc_state);
struct pdp0_se_config *pdp0_crtc_state_get_scaler_config(inno_drm_crtc_state *crtc_state);
struct pdp0_crtc_config *pdp0_crtc_state_get_priv_config(inno_drm_crtc_state *crtc_state);
u32 *pdp0_crtc_state_get_coloradj_coeffs(inno_drm_crtc_state *crtc_state);
u16 pdp0_get_plane_layer_base(struct innodpu_pdp0_hw_device *hwdev, int num);
void *pdp0_get_inno_framebuffer(void *fb);

unsigned short inno_drm_display_mode_get_vdisplay(
		const inno_drm_display_mode *inno_dmode);

unsigned short inno_drm_display_mode_get_hdisplay(
		const inno_drm_display_mode *inno_dmode);


extern void inno_drm_display_mode_to_videomode(const inno_drm_display_mode *inno_dmode,
															inno_videomode *inno_vm);

u32 pdp0_set_alpha_blend_mode(inno_drm_plane *inno_plane, inno_drm_framebuffer *inno_fb,
									 u32 layer_ctrl);
bool pdp0_is_alpha_support(uint32_t format);
unsigned long pdp0_gem_get_dev_paddr(inno_drm_gem_object *obj);
unsigned long pdp0_gem_get_cpu_paddr(inno_drm_gem_object *obj);
u64 pdp0_get_fb_dev_paddr(struct innodpu_pdp0_hw_device *hwdev, void *inno_fb);
u64 pdp0_get_fb_cpu_paddr(struct innodpu_pdp0_hw_device *hwdev, void *inno_fb);


u32 INNO_PADDING_ALIGN_SIZE_FUNC(void);
u32 INNO_PDP0_CONFIG_VALID_DONE_FUNC(void);
u32 INNO_PDP0_FS_BLOCK_FUNC(void);
u32 INNO_PDP0_FC_BLOCK_FUNC(void);
u32 INNO_PDP0_FD_BLOCK_FUNC(void);



#define INNO_PADDING_ALIGN_SIZE           INNO_PADDING_ALIGN_SIZE_FUNC()
#define INNO_PDP0_CONFIG_VALID_DONE       INNO_PDP0_CONFIG_VALID_DONE_FUNC()

#endif

