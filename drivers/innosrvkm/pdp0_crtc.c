/*************************************************************************/ /*!
@File			pdp0_crtc.c
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
#include "pdp0_crtc.h"
#include "pdp0_plane.h"
#include "innogpu_drm.h"

static bool dump_fb_bmp = false;
module_param(dump_fb_bmp, bool, 0600);
MODULE_PARM_DESC(dump_fb_bmp, "dump crtc framebuffer saves *.bmp(default: false)");

static int pdp0_for_vga_notifier_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	struct innodpu_pdp0_drm *pdp0_drm = notifier_to_pdp0_device(nb);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct innodpu_drm_private *dev_priv = NULL;
	struct drm_display_mode *mode = NULL;
	innodpu_pdp_vga_gem *pdp_vga_mem = NULL;
	bool opr = (bool)action;
	bool vga_buffer_set_success = false;
	u64 vga_buffer_addr = 0;

	dev_priv = innogpu_drm_to_display_private(pdp0_drm->drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(pdp0_drm->drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	if (!dev_priv->has_pdp_vga_mem) {
		return -EINVAL;
	}
	pdp_vga_mem = dev_priv->pdp_vga_gem;

	if (data == NULL) {
		return 0;
	}
	mode = (struct drm_display_mode *)data;

	if (opr == true && pdp0_drm->lg_layer_enabled == false) {
		vga_buffer_set_success = innodpu_pdp_vga_buffer_set(pdp_vga_mem,
			pdp0_drm->dev->parent, mode->hdisplay, mode->vdisplay);
		vga_buffer_addr = (u64)pdp_vga_mem->info.dev_paddr;

		if (vga_buffer_set_success && vga_buffer_addr) {
			pdp0_drm->lg_layer_enabled = true;
			hwdev->vga_point_enable(hwdev, vga_buffer_addr,
				mode->hdisplay, mode->vdisplay, 0, 0);
		}
	} else if(opr == false && pdp0_drm->lg_layer_enabled == true) {
		pdp0_drm->lg_layer_enabled = false;
		hwdev->vga_point_disable(hwdev);
	}

	return 0;
}

static void active_timer_start(struct innodpu_pdp0_drm *pdp0_drm)
{
	struct drm_crtc *crtc = &pdp0_drm->crtc;
	uint64_t vblank_interval_ns = 668000;

	if (crtc->mode.clock && crtc->mode.vdisplay && crtc->mode.vtotal) {
		vblank_interval_ns = 1000000000 / drm_mode_vrefresh(&crtc->mode);
		vblank_interval_ns = vblank_interval_ns *
			(crtc->mode.vtotal - crtc->mode.vdisplay) / crtc->mode.vtotal;
	}

	hrtimer_start(&pdp0_drm->timer_active, ns_to_ktime(vblank_interval_ns), HRTIMER_MODE_REL);

	return;
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
static enum drm_mode_status pdp0_crtc_mode_valid(struct drm_crtc *crtc,
												 const struct drm_display_mode *mode)
{
#if 0
	/* Do not allow doublescan modes from user space */
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s mode valid.\n", crtc->name);
		return MODE_NO_DBLESCAN;
	}
	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s mode no interlace.\n", crtc->name);
		return MODE_NO_INTERLACE;
	}
	if (mode->hdisplay % 16) {
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s:Hactive must be intergal multiple of 16\n", crtc->name);
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "Not surrport:Modeline " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
		return MODE_BAD_HVALUE;
	}
#endif
	return MODE_OK;
}
#endif

static int pdp0_first_connector_type(struct drm_crtc *crtc)
{
	struct drm_connector *connector = NULL;
	struct drm_connector_list_iter conn_iter;

	if (!crtc->state->connector_mask)
		return 0;

	drm_connector_list_iter_begin(crtc->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (crtc->state->connector_mask & inno_drm_connector_mask(connector)) {
			drm_connector_list_iter_end(&conn_iter);
			return connector->connector_type;
		}
	}
	drm_connector_list_iter_end(&conn_iter);
	return 0;
}

static void pdp0_crtc_atomic_begin_legacy(struct drm_crtc *crtc, struct drm_crtc_state *old_crtc_state)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	int    dpu_id = innodpu_get_dpuid_bycrtc(crtc);
	uint32_t src_h, src_w;
	uint32_t crtc_h, crtc_w;

	if (!hwdev) {
		return ;
	}

	if (dpu_id >= fh2m_hal_get_dev_nums(crtc->dev->dev, DEV_DB9000)) {
		if (hwdev->atomic_update_combine)
			hwdev->atomic_update_combine(crtc);
		if (hwdev->atomic_update_gamma)
			hwdev->atomic_update_gamma(crtc, old_crtc_state);
		//hwdev->pdp0_atomic_update_coloradj(crtc, old_crtc_state);
		if (hwdev->atomic_se_config)
			hwdev->atomic_se_config(crtc, old_crtc_state);
	}

	crtc_w = old_crtc_state->adjusted_mode.hdisplay;
	crtc_h = old_crtc_state->adjusted_mode.vdisplay;
	src_w = old_crtc_state->mode.hdisplay;
	src_h = old_crtc_state->mode.vdisplay;

	hwdev->x_scaler = 0;
	hwdev->y_scaler = 0;
	if ((src_w > 0) && (src_h > 0)) {
		if ((crtc_w != src_w) || (crtc_h != src_h)) {
			hwdev->x_scaler = crtc_w * 10000 / src_w;
			hwdev->y_scaler = crtc_h * 10000 / src_h;
		}
	}
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
static void pdp0_crtc_atomic_begin(struct drm_crtc *crtc, struct drm_atomic_state *atomic_state)
{
	struct drm_crtc_state *old_crtc_state = drm_atomic_get_crtc_state(atomic_state, crtc);

	pdp0_crtc_atomic_begin_legacy(crtc, old_crtc_state);
}
#endif

static void pdp0_crtc_atomic_enable_legacy(struct drm_crtc *crtc)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct videomode vm;
	uint64_t vblank_ns = 0;
	int conn_type = 0;

	if (!crtc->state) {
		fh2m_innodpu_err(crtc->dev->dev, "%s crtc state is NULL, not used bist mode", crtc->name);
		return;
	}

	if (hwdev->setvga) {
		conn_type = pdp0_first_connector_type(crtc);
		if (conn_type <=0) {
			fh2m_innodpu_err(crtc->dev->dev, "%s conn type %d not support\n", crtc->name, conn_type);
			return;
		}
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s conn type %d\n", crtc->name, conn_type);
		if (conn_type == DRM_MODE_CONNECTOR_VGA)
			hwdev->setvga_patch = true;
	}

	if (s_vga_auto_adapt) {
		/* make sure pdp belong vga, and pdp belong wrap0 */
		if ((pdp0_first_connector_type(crtc) == DRM_MODE_CONNECTOR_VGA) &&
			(hwdev->dpu_id <= 1) && (atomic_read(&pdp0_drm->vga_nb_registered) == 0)) {
			vga_notifier_register(&pdp0_drm->vga_adapt_nb);
			atomic_set(&pdp0_drm->vga_nb_registered, 1);
		}
	}

	drm_display_mode_to_videomode(&crtc->state->adjusted_mode, &vm);

	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s(combine-%d,dual-link-%d) enable mode:\n\t" DRM_MODE_FMT "\n",
		crtc->name, pdp0_drm->hwdev->combi, pdp0_drm->hwdev->dual_link, DRM_MODE_ARG(&crtc->state->adjusted_mode));

	if (pdp0_drm->hwdev->combi) {
		pdp0_drm->hwdev->enable_combine(pdp0_drm->hwdev, &vm);
	}
	if (pdp0_drm->hwdev->dual_link) {
		pdp0_drm->hwdev->enable_dual_link(pdp0_drm->hwdev);
	}

	hwdev->modeset(hwdev, &vm, false);

	if (hwdev->cursor_enable) {
		if (hwdev->cursor_is_disable(hwdev)) {
			hwdev->cursor_resume(hwdev);
			fh2m_innodpu_info(hwdev->dev, DPU_UT_CURSOR,"pdp-%d crtc resume cursor\n", hwdev->dpu_id);
		}
	}

	hwdev->leave_config_mode(hwdev);
	if (pdp0_drm->hwdev->combi) {
		hwdev->enter_config_mode(hwdev);
		hwdev->leave_config_mode(hwdev);
	}

	if (hwdev->is_nulldisp) {
		if (crtc->mode.clock) {
			vblank_ns = 1000000000 / drm_mode_vrefresh(&crtc->mode);
		} else {
			vblank_ns = 16700000;
		}
		hrtimer_start(&pdp0_drm->timer_hr, ns_to_ktime(vblank_ns), HRTIMER_MODE_REL);

		if (s_dpu_support_plane_fd) {
			active_timer_start(pdp0_drm);
		}
	}

	if(hwdev->enable_irq)
		hwdev->enable_irq(hwdev, PDP0_FD_BLOCK, hwdev->map.fd_irq_map.vsync_irq);

	atomic_set(pdp0_drm->hwdev->vblank_enable, 1);

	drm_crtc_vblank_on(crtc);
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
static void pdp0_crtc_atomic_enable(struct drm_crtc *crtc, struct drm_atomic_state *atomic_state)
{
	pdp0_crtc_atomic_enable_legacy(crtc);
}
#elif (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
static void pdp0_crtc_atomic_enable(struct drm_crtc *crtc, struct drm_crtc_state *old_state)
{
	pdp0_crtc_atomic_enable_legacy(crtc);
}
#endif

static void pdp0_crtc_atomic_disable_legacy(struct drm_crtc *crtc, struct drm_crtc_state *old_state)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	bool flag = false;
	hwdev->setvga_patch = false;

	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s disable\n", crtc->name);

	if(hwdev->disable_irq)
		hwdev->disable_irq(hwdev, PDP0_FD_BLOCK, hwdev->map.fd_irq_map.vsync_irq);

	atomic_set(pdp0_drm->hwdev->vblank_enable, 0);
	drm_crtc_vblank_off(crtc);

	if (s_vga_auto_adapt) {
		/* make sure pdp belong vga, and pdp belong wrap0 */
		if (atomic_read(&pdp0_drm->vga_nb_registered) == 1) {
			vga_notifier_unregister(&pdp0_drm->vga_adapt_nb);
			atomic_set(&pdp0_drm->vga_nb_registered, 0);
		}
	}

	if (hwdev->is_nulldisp) {
		hrtimer_cancel(&pdp0_drm->timer_hr);
		if (s_dpu_support_plane_fd) {
			hrtimer_cancel(&pdp0_drm->timer_active);
		}
		return;
	}
	hwdev->x_scaler = 0;
	hwdev->y_scaler = 0;

	if (hwdev->features & INNO_PDP_COMBINE) {
		if (!hwdev->in_config_mode(hwdev, 0)) {
			if (hwdev->combi == true)
				hwdev->reset(hwdev, 2);
			else
				hwdev->reset(hwdev, 0);
			flag = true;
		} else {
			fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s disable not reset！！！\n", crtc->name);
		}

		if (((hwdev->dual_link))&&(!hwdev->in_config_mode(hwdev, 1))) {
			hwdev->reset(hwdev, 1);
			flag |= true;
		} else {
			fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s disable not reset！！！\n", crtc->name);
		}

		if (flag) {
			hwdev->hardware_init(hwdev);
		}

		if (pdp0_drm->hwdev->dual_link) {
			pdp0_drm->hwdev->disable_dual_link(pdp0_drm->hwdev);
		}

		pdp0_drm->hwdev->disable_combine(pdp0_drm->hwdev);
	} else {
		if (!hwdev->in_config_mode(hwdev, 0))
			hwdev->enter_config_mode(hwdev);
	}

	hwdev->combi = false;
	hwdev->dual_link = false;
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
static void pdp0_crtc_atomic_disable(struct drm_crtc *crtc, struct drm_atomic_state *atomic_state)
{
	struct drm_crtc_state *old_crtc_state = drm_atomic_get_crtc_state(atomic_state, crtc);

	pdp0_crtc_atomic_disable_legacy(crtc, old_crtc_state);
}
#endif

static const struct gamma_curve_segment {
	u16 start;
	u16 end;
} segments[PDP0_COEFFTAB_NUM_COEFFS] = {
	/* sector 0 */
	/* sector 0 */
	{
	0, 0}, {
	1, 1}, {
	2, 2}, {
	3, 3},						// 1
	{
	4, 4}, {
	5, 5}, {
	6, 6}, {
	7, 7}, {
	8, 8}, {
	9, 9}, {
	10, 10}, {
	11, 11}, {
	12, 12}, {
	13, 13}, {
	14, 14}, {
	15, 15},
		/* sector 1 */
	{
	16, 19}, {
	20, 23}, {
	24, 27}, {
	28, 31},					// 4
		/* sector 2 */
	{
	32, 39}, {
	40, 47}, {
	48, 55}, {
	56, 63},					// 8
		/* sector 3 */
	{
	64, 79}, {
	80, 95}, {
	96, 111}, {
	112, 127},					// 16
		/* sector 4 */
	{
	128, 159}, {
	160, 191}, {
	192, 223}, {
	224, 255},					// 32
		/* sector 5 */
	{
	256, 319}, {
	320, 383}, {
	384, 447}, {
	448, 511},					// 64
		/* sector 6 */
	{
	512, 639}, {
	640, 767}, {
	768, 895}, {
	896, 1023},					// 128
	{
	1024, 1151}, {
	1152, 1279}, {
	1280, 1407}, {
	1408, 1535}, {
	1536, 1663}, {
	1664, 1791}, {
	1792, 1919}, {
	1920, 2047}, {
	2048, 2175}, {
	2176, 2303}, {
	2304, 2431}, {
	2432, 2559}, {
	2560, 2687}, {
	2688, 2815}, {
	2816, 2943}, {
	2944, 3071}, {
	3072, 3199}, {
	3200, 3327}, {
	3328, 3455}, {
	3456, 3583}, {
	3584, 3711}, {
	3712, 3839}, {
	3840, 3967}, {
3968, 4095},};

#define DE_COEFTAB_DATA(a, b) ((((a) & 0xfff) << 16) | (((b) & 0xfff)))

struct s_tmp_gamma {
	int y[256];
	int x[256];
	int out[4096];
	int coeffs[256][3];
};

static void pdp0_generate_gamma_table(struct drm_property_blob *lut_blob,
									  u32 coeffs_out[PDP0_COEFFTAB_NUM_COEFFS])
{
	int i;
	int j = 0, a = 0, b = 0, c = 0;
	int delta;
	int times = 4096 / 256;

	struct s_tmp_gamma *p_gamma = kzalloc(sizeof(*p_gamma), fh2m_hal_get_inno_gfp_kernel());
	struct drm_color_lut *lut = (struct drm_color_lut *)lut_blob->data;

	if (!p_gamma)
		return;

	for (i = 0; i < 256; ++i) {
		p_gamma->y[i] = ((lut[i].green) >> 8) * 16;
		p_gamma->x[i] = (i + 1) * 16;
	}

	for (i = 0; i < 256 - 1; i++) {
		if (p_gamma->y[i] == p_gamma->y[i + 1]) {
			if (p_gamma->y[i] == 0)
				p_gamma->y[i + 1] += times / 2;
			if (p_gamma->y[i] != 0)
				p_gamma->y[i] -= times / 2;
		}
	}

	for (i = 0; i < 256 - 1; i++) {
		delta = p_gamma->x[i + 1] - p_gamma->x[i];
		if (delta == 0) {
			a = 0;
		} else {
			a = ((p_gamma->y[i + 1] - p_gamma->y[i]) * 256) / delta;
		}
		b = p_gamma->y[i];
		c = p_gamma->x[i];
		p_gamma->coeffs[i][0] = a;
		p_gamma->coeffs[i][1] = b;
		p_gamma->coeffs[i][2] = c;
	}

	for (i = 0; i < 4096; i++) {
		for (j = 0; j < 256 - 1; j++) {
			if (i == 4095)
				p_gamma->out[i] = p_gamma->out[i - 1] + times / 2;

			if (i >= p_gamma->x[j] && i < p_gamma->x[j + 1]) {
				p_gamma->out[i] =
					(p_gamma->coeffs[j][0] / 256) * (i - p_gamma->coeffs[j][2]) +
					p_gamma->coeffs[j][1];
				break;
			}
		}
	}

	for (i = 0; i < PDP0_COEFFTAB_NUM_COEFFS; ++i) {
		u32 a, b, delta_in, out_start, out_end, x_start, x_end;
		x_start = segments[i].start;
		x_end = segments[i].end;
		delta_in = segments[i].end - segments[i].start;
		/* DP has 12-bit internal precision for its LUTs. */
		out_start = (p_gamma->out[x_start] + (1 << 3)) >> 4;
		out_end = (p_gamma->out[x_end] + (1 << 3)) >> 4;
		a = (delta_in == 0) ? 0 : ((out_end - out_start) * 256) / delta_in;
		b = out_start;
		a = a * 16;
		b = b * 16;
		coeffs_out[i] = DE_COEFTAB_DATA(a, b);
	}
	kfree(p_gamma);
}

static void pdp0_generate_coloradj_table(struct drm_property_blob *lut_blob,
									u32 coeffs_out[PDP0_COLORADJ_NUM_COEFFS])
{
	u32 gamma_r = 0 , gamma_b = 0, gamma_g = 0, tmp= 10000;
	struct drm_color_lut *lut = (struct drm_color_lut *)lut_blob->data;

	memset(coeffs_out, 0, 12);

	gamma_r = ((lut[255].red) >> 8);
	gamma_g = ((lut[255].green) >> 8);
	gamma_b = ((lut[255].blue) >> 8);
	coeffs_out[0] = (((gamma_r*tmp)/255)*(1<<12))/tmp;  //0x50
	coeffs_out[4] = (((gamma_g*tmp)/255)*(1<<12))/tmp;  //0x60
	coeffs_out[8] = (((gamma_b*tmp)/255)*(1<<12))/tmp;  //0x70

}

/*
 * Check if there is a new gamma LUT and if it is of an acceptable size. Also,
 * reject any LUTs that use distinct red, green, and blue curves.
 */
static int pdp0_crtc_atomic_check_gamma(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct pdp0_crtc_state *mc = to_pdp0_crtc_state(state);
	struct drm_color_lut *lut;
	size_t lut_size;
	int i;

	if (!state->color_mgmt_changed || !state->gamma_lut) {
		return 0;
	}

	if (crtc->state->gamma_lut && (crtc->state->gamma_lut->base.id == state->gamma_lut->base.id)) {
		fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "gamma_lut not change\n");
		return 0;
	}

	if (state->gamma_lut->length % sizeof(struct drm_color_lut)) {
		fh2m_innodpu_err(crtc->dev->dev, "gamma lut length error\n");
		return -EINVAL;
	}

	lut_size = state->gamma_lut->length / sizeof(struct drm_color_lut);
	if (lut_size != PDP0_GAMMA_LUT_SIZE) {
		fh2m_innodpu_err(crtc->dev->dev, "gamma lut length error\n");
		return -EINVAL;
	}

	lut = (struct drm_color_lut *)state->gamma_lut->data;

#if 1							// closes #665
	mc->coladj_en = false;

	for (i = 0; i < lut_size; ++i) {
		if ((!((lut[i].red == lut[i].green) && (lut[i].red == lut[i].blue))) || s_coladj_force) {
			//fh2m_innodpu_err(crtc->dev->dev, "gamma_lut 2 coloradj enable\n");
			mc->coladj_en = true;
		//	return -EINVAL;
		}
	}
#endif

	if (!state->mode_changed) {
		int ret;
		//这个打开会进cfg mode 导致调节gamma的时候黑屏
		// state->mode_changed = true;
		/*
		 * Kerneldoc for drm_atomic_helper_check_modeset mandates that
		 * it be invoked when the driver sets ->mode_changed. Since
		 * changing the gamma LUT doesn't depend on any external
		 * resources, it is safe to call it only once.
		 */
		ret = drm_atomic_helper_check_modeset(crtc->dev, state->state);
		if (ret) {
			fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "mode_changed\n");
			return ret;
		}
	}
	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "start to generate gamma table\n");
	if (mc->coladj_en) {
		pdp0_generate_coloradj_table(state->gamma_lut, mc->coloradj_coeffs);
	}else {
		pdp0_generate_gamma_table(state->gamma_lut, mc->gamma_coeffs);
	}
	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "end to generate gamma table\n");
	return 0;
}

/*
 * Check if there is a new CTM and if it contains valid input. Valid here means
 * that the number is inside the representable range for a Q3.12 number,
 * excluding truncating the fractional part of the input data.
 *
 * The COLORADJ registers can be changed atomically.
 */
static int  __maybe_unused pdp0_crtc_atomic_check_ctm(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct pdp0_crtc_state *mc = to_pdp0_crtc_state(state);
	struct drm_color_ctm *ctm;
	int i;

	if (!state->color_mgmt_changed) {
		return 0;
	}

	if (!state->ctm) {
		return 0;
	}

	if (crtc->state->ctm && (crtc->state->ctm->base.id == state->ctm->base.id)) {
		return 0;
	}

	/*
	 * The size of the ctm is checked in
	 * drm_atomic_replace_property_blob_from_id.
	 */
	ctm = (struct drm_color_ctm *)state->ctm->data;
	for (i = 0; i < INNO_ARRAY_SIZE(ctm->matrix); ++i) {
		/* Convert from S31.32 to Q3.12. */
		s64 val = ctm->matrix[i];
		u32 mag = ((((u64) val) & ~BIT_ULL(63)) >> 20) & GENMASK_ULL(14, 0);

		/*
		 * Convert to 2s complement and check the destination's top bit
		 * for overflow. NB: Can't check before converting or it'd
		 * incorrectly reject the case:
		 * sign == 1
		 * mag == 0x2000
		 */
		if (val & BIT_ULL(63))
			mag = ~mag + 1;
		if (! !(val & BIT_ULL(63)) != ! !(mag & INNO_BIT(14))) {
			fh2m_innodpu_err(crtc->dev->dev, "ctm val error\n");
			return -EINVAL;
		}
		mc->coloradj_coeffs[i] = mag;
	}

	return 0;
}

static int pdp0_crtc_atomic_check_scaling(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct pdp0_crtc_state *cs = to_pdp0_crtc_state(state);
	struct pdp0_se_config *s = &cs->scaler_config;
	struct drm_plane *plane = NULL;
	struct videomode vm;
	const struct drm_plane_state *pstate = NULL;
	u32 h_upscale_factor = 0;	/* U16.16 */
	u32 v_upscale_factor = 0;	/* U16.16 */
	u8 scaling = cs->scaled_planes_mask;
	int ret;

	if (!scaling) {
		s->scale_enable = false;
		goto mclk_calc;
	}

	/* The scaling engine can only handle one plane at a time. */
	if (scaling & (scaling - 1)) {
		fh2m_innodpu_err(crtc->dev->dev, "scaling %u error\n", scaling);
		return -EINVAL;
	}

	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		struct pdp0_plane *mp = to_pdp0_plane(plane);
		u32 phase;

		if (!(mp->layer->id & scaling))
			continue;

		if ((hwdev->features & INNO_PDP_COMBINE) &&
			(state->adjusted_mode.hdisplay > INNODPU_COMBINE_WIDTH)) {
			s->input_w = pstate->src_w >> 17;
			s->input_h = pstate->src_h >> 16;
			s->output_w = pstate->crtc_w >> 1;
			s->output_h = pstate->crtc_h;
			h_upscale_factor = div_u64((u64) (pstate->crtc_w>>1) << 32, (pstate->src_w>>1));
		} else {
			s->input_w = pstate->src_w >> 16;
			s->input_h = pstate->src_h >> 16;
			s->output_w = pstate->crtc_w;
			s->output_h = pstate->crtc_h;
			h_upscale_factor = div_u64((u64) pstate->crtc_w << 32, pstate->src_w);
		}

		v_upscale_factor = div_u64((u64) pstate->crtc_h << 32, pstate->src_h);
		s->enhancer_enable = ((h_upscale_factor >> 16) >= 2 || (v_upscale_factor >> 16) >= 2);

#define FS_N_PHASE 4
#define FS_SHIFT_N_PHASE 12
		/* Calculate initial_phase and delta_phase for horizontal. */
		phase = s->input_w;
		s->h_init_phase = ((phase << FS_N_PHASE) / s->output_w + 1) / 2;

		phase = s->input_w;
		phase <<= (FS_SHIFT_N_PHASE + FS_N_PHASE);
		s->h_delta_phase = phase / s->output_w;

		/* Same for vertical. */
		phase = s->input_h;
		s->v_init_phase = ((phase << FS_N_PHASE) / s->output_h + 1) / 2;

		phase = s->input_h;
		phase <<= (FS_SHIFT_N_PHASE + FS_N_PHASE);
		s->v_delta_phase = phase / s->output_h;
#undef FS_N_PHASE
#undef FS_SHIFT_N_PHASE
		s->plane_src_id = mp->layer->id;
	}

	s->scale_enable = true;
	s->hcoeff = pdp0_se_select_coeffs(h_upscale_factor);
	s->vcoeff = pdp0_se_select_coeffs(v_upscale_factor);
	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU,
			"h_upscale_factor=%#x,v_upscale_factor=%#x,se_config=%#x,src=%dx%d,dst=%dx%d\n",
			h_upscale_factor >> 16, v_upscale_factor >> 16, s->enhancer_enable, s->input_w,
			s->input_h, s->output_w, s->output_h);
	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "hcoeff=%#x,vcoeff=%#x\n", s->hcoeff, s->vcoeff);
mclk_calc:
	drm_display_mode_to_videomode(&state->adjusted_mode, &vm);
	ret = hwdev->fs_calc_mclk(hwdev, s, &vm);
	if (ret < 0) {
		fh2m_innodpu_err(crtc->dev->dev, "!! hardware calc mclk error\n");
		return -EINVAL;
	}
	return 0;

}

#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
static int pdp0_crtc_atomic_check(struct drm_crtc *crtc, struct drm_atomic_state *atomic_state)
#else
static int pdp0_crtc_atomic_check(struct drm_crtc *crtc, struct drm_crtc_state *state)
#endif
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct drm_plane *plane = NULL;
	const struct drm_plane_state *pstate = NULL;
	u32 rot_mem_free, rot_mem_usable;
	int rotated_planes = 0;
	int ret;

#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
	struct drm_crtc_state *state = drm_atomic_get_new_crtc_state(atomic_state, crtc);
#endif

	/*
	 * check if there is enough rotation memory available for planes
	 * that need 90 and 270 rotation. Each plane has set its required
	 * memory size in the ->plane_check() callback, here we only make
	 * sure that the sums are less that the total usable memory.
	 *
	 * The rotation memory allocation algorithm (for each plane):
	 *  a. If no more rotated planes exist, all remaining rotate
	 *     memory in the bank is available for use by the plane.
	 *  b. If other rotated planes exist, and plane's layer ID is
	 *     PDP0_OW1, it can use all the memory from first bank if
	 *     secondary rotation memory bank is available, otherwise it can
	 *     use up to half the bank's memory.
	 *  c. If other rotated planes exist, and plane's layer ID is not
	 *     PDP0_OW1, it can use half of the available memory
	 *
	 * Note: this algorithm assumes that the order in which the planes are
	 * checked always has PDP0_OW1 plane first in the list if it is
	 * rotated. Because that is how we create the planes in the first
	 * place, under current DRM version things work, but if ever the order
	 * in which drm_atomic_crtc_state_for_each_plane() iterates over planes
	 * changes, we need to pre-sort the planes before validation.
	 */

	/* first count the number of rotated planes */
	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		if (pstate->rotation & PDP0_ROTATED_MASK)
			rotated_planes++;
	}

	rot_mem_free = hwdev->rotation_memory[0];
	/*
	 * if we have more than 1 plane using rotation memory, use the second
	 * block of rotation memory as well
	 */
	if (rotated_planes > 1)
		rot_mem_free += hwdev->rotation_memory[1];

	/* now validate the rotation memory requirements */
	drm_atomic_crtc_state_for_each_plane_state(plane, pstate, state) {
		struct pdp0_plane *mp = to_pdp0_plane(plane);
		struct pdp0_plane_state *ms = to_pdp0_plane_state(pstate);

		if (pstate->rotation & PDP0_ROTATED_MASK) {
			/* process current plane */
			rotated_planes--;

			if (!rotated_planes) {
				/* no more rotated planes, we can use what's left */
				rot_mem_usable = rot_mem_free;
			} else {
				if ((mp->layer->id != PDP0_OW1) || (hwdev->rotation_memory[1] == 0))
					rot_mem_usable = rot_mem_free / 2;
				else
					rot_mem_usable = hwdev->rotation_memory[0];
			}

			rot_mem_free -= rot_mem_usable;

			if (ms->rotmem_size > rot_mem_usable) {
				fh2m_innodpu_err(crtc->dev->dev, "%s ms->rotmem_size \n", crtc->name);
				return -EINVAL;
			}
		}
	}
	ret = pdp0_crtc_atomic_check_gamma(crtc, state);
	ret = ret ? ret : pdp0_crtc_atomic_check_ctm(crtc, state);
	ret = ret ? ret : pdp0_crtc_atomic_check_scaling(crtc, state);

	return ret;
}

static const struct drm_crtc_helper_funcs pdp0_crtc_helper_funcs = {
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	.mode_valid = pdp0_crtc_mode_valid,
#endif
#if (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
	.atomic_enable = pdp0_crtc_atomic_enable,
#else
	.enable = pdp0_crtc_atomic_enable_legacy,
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
	.atomic_begin = pdp0_crtc_atomic_begin,
	.atomic_disable = pdp0_crtc_atomic_disable,
#else
	.atomic_begin = pdp0_crtc_atomic_begin_legacy,
	.atomic_disable = pdp0_crtc_atomic_disable_legacy,
#endif
	.atomic_check = pdp0_crtc_atomic_check,
};

// called by drm_atomic_set_property to duplicate state
static struct drm_crtc_state *pdp0_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct pdp0_crtc_state *state, *old_state;

	if (WARN_ON(!crtc->state))
		return NULL;


	old_state = to_pdp0_crtc_state(crtc->state);
	state = kmalloc(sizeof(*state), fh2m_hal_get_inno_gfp_kernel());
	if (!state) {
		fh2m_innodpu_err(crtc->dev->dev, "%s crtc state duplicate failed:short of mem", crtc->name);
		return NULL;
	}

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);
	memcpy(state->gamma_coeffs, old_state->gamma_coeffs, sizeof(state->gamma_coeffs));
	memcpy(state->coloradj_coeffs, old_state->coloradj_coeffs, sizeof(state->coloradj_coeffs));
	memcpy(&state->scaler_config, &old_state->scaler_config, sizeof(state->scaler_config));
	memcpy(&state->priv_config, &old_state->priv_config, sizeof(state->priv_config));
	memcpy(&state->coladj_en, &old_state->coladj_en, sizeof(state->coladj_en));
	state->scaled_planes_mask = 0;

	return &state->base;
}

// called by drm_mode_config_cleanup and pdp0_crtc_reset to clean states
static void pdp0_crtc_destroy_state(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct pdp0_crtc_state *pdp0_state = NULL;

	if (state) {
		pdp0_state = to_pdp0_crtc_state(state);
		__drm_atomic_helper_crtc_destroy_state(state);
		kfree(pdp0_state);
	}
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
static int
pdp0_crtc_async_page_flip(struct drm_crtc *crtc,
			   struct drm_framebuffer *fb,
			   struct drm_pending_vblank_event *event,
			   uint32_t flags)
{
	struct drm_device *dev = crtc->dev;
	struct drm_plane *plane = crtc->primary;
	struct pdp_crtc_async_flip_state *flip_state;
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;

	flip_state = kzalloc(sizeof(*flip_state), GFP_KERNEL);
	if (!flip_state) {
		return -ENOMEM;
	}

	drm_framebuffer_get(fb);
	flip_state->fb = fb;
	flip_state->crtc = crtc;
	flip_state->event = event;
	flip_state->old_fb = plane->state->fb;
	if (flip_state->old_fb)
		drm_framebuffer_get(flip_state->old_fb);

	WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	drm_atomic_set_fb_for_plane(plane->state, fb);

	if (hwdev->fd_async_update) {
		hwdev->fd_async_update(plane);
	}

	/*
	 * may lead to tearing; TBD:set the bootom half-step
	 */
	fh2m_inno_usleep_range(1000, 1000);

	if (flip_state->event) {
		unsigned long flags;
		spin_lock_irqsave(&dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, flip_state->event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	drm_crtc_vblank_put(crtc);
	drm_framebuffer_put(flip_state->fb);

	if (flip_state->old_fb) {
		drm_framebuffer_put(flip_state->old_fb);
	}

	kfree(flip_state);
	return 0;
}

static int
pdp0_crtc_async_check(struct drm_crtc *crtc)
{
	struct drm_plane *plane = crtc->primary;
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct drm_plane_state *state = plane->state;
#if 0
	if (!(s_async_flip_enable)) {
		fh2m_innodpu_err(crtc->dev->dev, "%s s_async_flip_enable = %d\n", crtc->name, s_async_flip_enable);
		return -EINVAL;
	}
#endif
	if (!(hwdev->features & INNO_PDP_ASYNC)) {
		fh2m_innodpu_err(crtc->dev->dev, "%s chip does not support async\n", crtc->name);
		return -EINVAL;
	}

	if (((state->src_w >> 16) != state->crtc_w) || ((state->src_h >> 16) != state->crtc_h)) {
		fh2m_innodpu_err(crtc->dev->dev, "%s async does not support scaler\n", crtc->name);
		return -EINVAL;
	}
	if (hwdev->combi && (hwdev->features & INNO_PDP_COMBINE)) {
		fh2m_innodpu_err(crtc->dev->dev, "%s async does not support combine \n", crtc->name);
		return -EINVAL;
	}
	return 0;
}

static int pdp0_crtc_page_flip(struct drm_crtc *crtc,
		  struct drm_framebuffer *fb,
		  struct drm_pending_vblank_event *event,
		  uint32_t flags,
		  struct drm_modeset_acquire_ctx *ctx)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct drm_plane *plane = crtc->primary;
	int ret = 0;

	if ((flags & DRM_MODE_PAGE_FLIP_ASYNC) && (!pdp0_crtc_async_check(crtc))) {
		ret = pdp0_crtc_async_page_flip(crtc, fb, event, flags);
		if (hwdev->fd_async_enable) {
			hwdev->fd_async_enable(plane);
		}
	} else {
		//TODO: if aync and vsync used alternately in once crtc_enable. there may be bug
		ret = drm_atomic_helper_page_flip(crtc, fb, event, flags, ctx);

		if ((hwdev->features & INNO_PDP_ASYNC) && hwdev->fd_async_disable) {
			hwdev->fd_async_disable(plane);
		}
	}
	return ret;
}
#endif


// called by drm_mode_config_reset to reset planes
// create the first state
static void pdp0_crtc_reset(struct drm_crtc *crtc)
{
	struct pdp0_crtc_state *state = kzalloc(sizeof(*state), fh2m_hal_get_inno_gfp_kernel());

#ifdef CONFIG_KALLSYMS
	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "%s crtc reset by %pf, old state %pK",
			crtc->name, __builtin_return_address(0), crtc->state);
#endif
	if (crtc->state)
		pdp0_crtc_destroy_state(crtc, crtc->state);

	if (state) {
		crtc->state = &state->base;
		crtc->state->crtc = crtc;
		state->priv_config.bist_value = 0;
		state->priv_config.is_yuv_fmt = 0;
		state->priv_config.wb_start_flag = 0;
		state->priv_config.pvric_value = 0;
		state->priv_config.pvric_type_value = 0;
		state->priv_config.pvric_compress_value = 0;
		state->priv_config.pvric_decomp_addr = 0;
		state->priv_config.wb_save_frames = 0;
		state->priv_config.pvric_display_id = 0;
		state->priv_config.pvric_decomp_type = 0;
		state->priv_config.is_uv_revs = 0;
		state->priv_config.comp_tile_4x16 = 0;
		state->priv_config.set_wm_fd = 0;
	}
}

static enum hrtimer_restart active_timer_func(struct hrtimer *hrt)
{
	uint64_t vblank_ns = 0;
	struct innodpu_pdp0_drm *pdp0_drm = active_timer_to_pdp0_device(hrt);
	struct drm_crtc *crtc = &pdp0_drm->crtc;

	atomic64_inc(&pdp0_drm->active_count);
	if (pdp0_drm->active_wq != NULL) {
		fh2m_inno_wake_up_interruptible(pdp0_drm->active_wq);
	}

	if (crtc->mode.clock) {
		vblank_ns = 1000000000 / drm_mode_vrefresh(&pdp0_drm->crtc.mode);
	} else {
		vblank_ns = 16700000;
	}
	hrtimer_forward_now(hrt, ns_to_ktime(vblank_ns));

	return HRTIMER_RESTART;
}

static enum hrtimer_restart htimer_func(struct hrtimer *hrt)
{
	uint64_t vblank_ns = 0;
	bool irq_enabled;
	struct innodpu_pdp0_drm *pdp0_drm = hrtimer_to_pdp0_device(hrt);

	innogpu_drm_irq_enabled(pdp0_drm->hwdev->drm_dev, &irq_enabled);

	//inno_error("[%s] hrtimer irq\n", pdp0_drm->crtc.name);
	atomic_set(pdp0_drm->hwdev->config_valid, 1);
	fh2m_inno_wake_up(pdp0_drm->hwdev->wq);

	if (pdp0_drm->crtc.mode.clock) {
		vblank_ns = 1000000000 / drm_mode_vrefresh(&pdp0_drm->crtc.mode);
	} else {
		vblank_ns = 16700000;
	}

	if (irq_enabled)
		fh2m_inno_drm_crtc_handle_vblank(&pdp0_drm->crtc);

	hrtimer_forward_now(hrt, ns_to_ktime(vblank_ns));

	return HRTIMER_RESTART;
}

int pdp0_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	uint64_t vblank_ns = 0;

	if (hwdev->is_nulldisp) {
		if (crtc->mode.clock) {
			vblank_ns = 1000000000 / drm_mode_vrefresh(&crtc->mode);
		} else {
			vblank_ns = 16700000;
		}
		hrtimer_start(&pdp0_drm->timer_hr, ns_to_ktime(vblank_ns), HRTIMER_MODE_REL);

		if (s_dpu_support_plane_fd) {
			/* restart active timer to make sure the gap of vblank and active */
			hrtimer_cancel(&pdp0_drm->timer_active);
			active_timer_start(pdp0_drm);
		}
	} else {
		atomic_set(pdp0_drm->hwdev->vblank_enable, 1);
	}

	return 0;
}

void pdp0_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;

	if (hwdev->is_nulldisp) {
		hrtimer_cancel(&pdp0_drm->timer_hr);
	} else {
		atomic_set(pdp0_drm->hwdev->vblank_enable, 0);
	}
}

static int pdp0_crtc_atomic_set_property(struct drm_crtc *crtc,
										 struct drm_crtc_state *crtc_state,
										 struct drm_property *property, uint64_t val)
{
	int ret = 0;
	struct drm_device *drm_dev = crtc->dev;
	struct innodpu_drm_private *dev_priv;
	struct pdp0_crtc_state *pdp0_state = to_pdp0_crtc_state(crtc_state);

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	// BY_TBD： 因为这里类型复杂，不是很好加打印信息
#define SET_PROPERTY(_tname, NAME, type) do { \
		if (dev_priv->crtc_prop[INNODPU_CRTC_PROP_##NAME] == property) { \
			pdp0_state->priv_config._tname = (type)val;	\
			fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%s crtc set property %s %llu.\n", crtc->name, #_tname, val); \
			goto done; \
		} \
	} while(0)
	SET_PROPERTY(bist_value, BIST, bool);
	SET_PROPERTY(is_yuv_fmt, ISYUV, bool);
	SET_PROPERTY(wb_start_flag, WB_START, uint64_t);
	SET_PROPERTY(pvric_value, PVR_ENABLE, bool);
	SET_PROPERTY(pvric_type_value, PVR_TYPE, bool);
	SET_PROPERTY(wb_save_frames, WB_SAVE, uint64_t);
	SET_PROPERTY(pvric_display_id, DISPLAY_ID, int);
	SET_PROPERTY(is_uv_revs, UVREVS, bool);
	SET_PROPERTY(comp_tile_4x16, COMP_TILE_4x16, bool);
	SET_PROPERTY(set_wm_fd, SET_WM_FD, int);	// INNODPU_CRTC_PROP_SET_WM_FD
	SET_PROPERTY(pvric_decomp_type, DECOMP_TYPE, int);
	SET_PROPERTY(pvric_decomp_addr, DECOMP_ADDR, uint32_t);

#undef SET_PROPERTY

	fh2m_innodpu_err(drm_dev->dev, "%s crtc set property failed,Invaild plane_state\n", crtc->name);
	return -EINVAL;

  done:

	return ret;
}

static int pdp0_crtc_atomic_get_property(struct drm_crtc *crtc,
										 const struct drm_crtc_state *crtc_state,
										 struct drm_property *property, uint64_t * val)
{
	int ret = 0;
	struct drm_device *drm_dev = crtc->dev;
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);
	struct pdp0_crtc_state *pdp0_state = to_pdp0_crtc_state(crtc_state);

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

	// BY_TBD： 因为这里类型复杂，不是很好加打印信息
#define GET_PROPERTY(_tname, NAME, type) do { \
		if (dev_priv->crtc_prop[INNODPU_CRTC_PROP_##NAME] == property) { \
			*val = (uint64_t)(pdp0_state->priv_config._tname);	\
			goto done; \
		} \
	} while(0)

	GET_PROPERTY(bist_value, BIST, bool);
	GET_PROPERTY(is_yuv_fmt, ISYUV, bool);
	GET_PROPERTY(wb_start_flag, WB_START, uint64_t);
	GET_PROPERTY(pvric_value, PVR_ENABLE, bool);
	GET_PROPERTY(pvric_type_value, PVR_TYPE, bool);
	GET_PROPERTY(wb_save_frames, WB_SAVE, uint64_t);
	GET_PROPERTY(pvric_display_id, DISPLAY_ID, int);
	GET_PROPERTY(is_uv_revs, UVREVS, bool);
	GET_PROPERTY(comp_tile_4x16, COMP_TILE_4x16, bool);
	GET_PROPERTY(set_wm_fd, SET_WM_FD, int);	// INNODPU_CRTC_PROP_SET_WM_FD
	GET_PROPERTY(pvric_decomp_type, DECOMP_TYPE, int);
	GET_PROPERTY(pvric_decomp_addr, DECOMP_ADDR, int);

	fh2m_innodpu_err(drm_dev->dev, "%s crtc set property failed,Invaild plane_state\n", crtc->name);
	ret = -EINVAL;
#undef GET_PROPERTY
  done:
	return ret;
}

static int __maybe_unused for_each_gem_object(int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	u64 *dev_ptr = data;

	if ((innodpu_obj->dev_paddr <= dev_ptr[0]) &&
		((innodpu_obj->dev_paddr + innodpu_obj->base.size) >= dev_ptr[0])) {
		dev_ptr[1] = (u64)innodpu_obj;
	}

	return 0;
}

#define BMP_HEADER_SIZE 54
struct __attribute__((packed)) bmp_head_info {
	u16 file_type;
	u32 file_size;
	u16 reserved1;
	u16 reserved2;
	u32 pixel_data_offset;
	u32 dib_header_size;
	int width;
	int height;
	u16 color_planes;
	u16 bits_per_pixel;
	u32 compression;
	u32 image_data_size;
	u32 horizontal_resolution;
	int vertical_resolution;
	u32 colors_used;
	u32 import_colors;
};

static void generate_bmp_header(void **map, int width, int height, int byte_num)
{
	struct bmp_head_info bmp_header;
	if (!*map) {
		return;
	}

	memset(&bmp_header, 0, sizeof(bmp_header));

	bmp_header.file_type = 0x4D42;
	bmp_header.file_size = width * height * byte_num + BMP_HEADER_SIZE;
	bmp_header.pixel_data_offset = BMP_HEADER_SIZE;
	bmp_header.dib_header_size = 40;
	bmp_header.width = width;
	bmp_header.height = -height;
	bmp_header.color_planes = 1;
	bmp_header.bits_per_pixel = 32;
	bmp_header.image_data_size = bmp_header.file_size - BMP_HEADER_SIZE;

	memcpy(*map, &bmp_header, BMP_HEADER_SIZE);
}

static int pdp0_snapshot_map(void **map, u64 *map_size,
		int (*pdp0_info)(struct innodpu_pdp0_hw_device *hw_dev,
			u64 buf[]), struct innodpu_pdp0_drm *pdp0_drm)
{
	u64 base_info[6] = {0};
	u64 dev_paddr = 0;
	u64 cpu_paddr = 0;
	u32 h_display = 0;
	u32 v_display = 0;
	u32 stride = 0;
	u8  byte_num = 4;
	u32 row = 0;
	void *vram  = NULL;
	u64 buf_size = 0;
	u32 bmp_header_size = 0;

	struct innodpu_pdp0_hw_device *hw_dev = pdp0_drm->hwdev;

	struct drm_device *dev = pdp0_drm->drm_dev;
	innodpu_mem_manager *mem_manager = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	bool hw_cur = false;

	if (pdp0_info(hw_dev, base_info)) {
		fh2m_innodpu_info(hw_dev->dev, DPU_UT_DPU, "layer disabled.\n");
		return -1;
	}

	dev_paddr  = base_info[0];
	cpu_paddr  = base_info[1];
	h_display = INNO_LOWER_32_BITS(base_info[2]);
	v_display = INNO_LOWER_32_BITS(base_info[3]);
	stride	  = INNO_LOWER_32_BITS(base_info[4]);

	if (stride == 0) {
		hw_cur = true;
		stride = 256;
		fh2m_innodpu_info(hw_dev->dev, DPU_UT_DPU, "use hw cursor\n");
	}

	if (hw_cur)
		hw_dev->combi = false;
	if (hw_dev->combi) {
		h_display = h_display * 2;
	}

	buf_size = stride * v_display;
	*map_size = h_display * v_display * byte_num;

	if (0 == buf_size) {
		fh2m_innodpu_info(pdp0_drm->dev, DPU_UT_DPU,"buffer size is 0\n");
		return -EFAULT;
	}

	base_info[1] = 0;
	idr_for_each(&dev->object_name_idr, for_each_gem_object, base_info);
	if (base_info[1] == 0) {
			return -EFAULT;
	}
	innodpu_obj = (innodpu_gem_object *)base_info[1];
	mem_manager = innodpu_obj->mem_manager;
	fh2m_innodpu_info(hw_dev->dev, DPU_UT_DPU, "cpu_paddr:%#.16llx. dev_paddr:%#.16llx.\n"
			" h_display:%d. v_display:%d. stride:%d. buf_size:%lld visible-%d\n",
			cpu_paddr, dev_paddr, h_display, v_display, stride, buf_size, mem_manager->visible);

	vram = fh2m_inno_vmalloc(buf_size);
	if (IS_ERR_OR_NULL(vram)) {
		fh2m_innodpu_err(pdp0_drm->dev, "no memory.\n");
		return -ENOMEM;
	}

	if (mem_manager->visible) {
		void *src = NULL, *dst = NULL;
		int size = buf_size;

		src = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdp0_drm->dev->parent, cpu_paddr);
		dst = (void*)vram;
		innodpu_dma_memcpy3(pdp0_drm->dev->parent, &src, &dst, &size, 1, GDDR2SYS);
	} else {
		int size = buf_size;
		void *axi_vram_src = NULL;
		void *dst = NULL;

		axi_vram_src = (void *)dev_paddr;
		dst = vram;
		innodpu_dma_memcpy_for_smallbar_sg(pdp0_drm->dev->parent, &axi_vram_src, &dst, &size, 1, GDDR2SYS);
	}

	if (dump_fb_bmp) {
		bmp_header_size = BMP_HEADER_SIZE;
	} else {
		bmp_header_size = 0;
	}

	*map = fh2m_inno_vmalloc(*map_size + bmp_header_size);
	if (IS_ERR_OR_NULL(*map)) {
		fh2m_innodpu_err(hw_dev->dev, "no memory.\n");
		fh2m_inno_vfree(vram);
		return -ENOMEM;
	}

	if (dump_fb_bmp) {
		generate_bmp_header(map, h_display, v_display, byte_num);
	}

	for (row = 0; row < v_display; row++) {
		memcpy(*map + row * h_display * byte_num + bmp_header_size,
			vram + row * stride,
			h_display * byte_num);
	}

	fh2m_inno_vfree(vram);

	return 0;
}

static int pdp0_base_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;
	file->private_data = crtc;

	return 0;
}

/* get pdp image ioctl */
#define CAPTURE_GET_IMAGE _IOWR('I', 0, struct user_capture_info)
#define CAPTURE_GET_INFO _IOWR('I', 1, struct user_rect_info)

struct user_capture_info {
	unsigned int split;
	unsigned int part;
	unsigned int width;
	unsigned int height;
	char __user *buf;
};
struct user_rect_info {
	unsigned int width;
	unsigned int height;
};

static long pdp0_base_get_size(struct file *file, unsigned long arg)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_crtc *crtc = NULL;
	struct innodpu_pdp0_hw_device *hw_dev = NULL;
	struct user_rect_info info = {0};
	u64 base_info[6] = {0};

	crtc = file->private_data;
	pdp0_drm = crtc_to_pdp0_device(crtc);
	hw_dev = pdp0_drm->hwdev;

	if ((!hw_dev->base_info) || hw_dev->base_info(hw_dev, base_info)) {
		fh2m_innodpu_err(hw_dev->dev, "capture layer disabled.\n");
		return -1;
	}
	info.width = INNO_LOWER_32_BITS(base_info[2]);
	info.width = hw_dev->combi ? info.width * 2 : info.width;
	info.height = INNO_LOWER_32_BITS(base_info[3]);

	if (copy_to_user((void *)arg, &info, sizeof(struct user_rect_info))) {
		fh2m_innodpu_err(hw_dev->dev, "caputer: update user wxh info err\n");
		return -1;
	}

	return 0;
}

static long pdp0_base_split_image(struct file *file, unsigned long arg)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct user_capture_info info = {0};
	struct drm_crtc *crtc = NULL;
	struct innodpu_pdp0_hw_device *hw_dev = NULL;
	innodpu_mem_manager *mem_manager = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	char *copy_buf = NULL, *temp_buf = NULL, *user_buf = NULL;

	u64 base_info[6] = {0};
	u64 dev_paddr, cpu_paddr;
	u32 width, height, stride, copy_size;
	int i = 0, row_len = 0, copy_ret = 0;
	unsigned int split_h, split_y;

	crtc = file->private_data;
	pdp0_drm = crtc_to_pdp0_device(crtc);
	hw_dev = pdp0_drm->hwdev;

	/* get and check user info here */
	copy_ret = fh2m_inno_copy_from_user(&info, (void *)arg, sizeof(struct user_capture_info));
	if (copy_ret) {
		fh2m_innodpu_err(hw_dev->dev, "capture: get user info err\n");
		return -1;
	}
	if ((info.part == 0) || (info.split == 0) || (info.split < info.part)) {
		fh2m_innodpu_err(hw_dev->dev, "split info err split[%d] part[%d]\n", info.split, info.part);
		return -1;
	}
	if (info.buf == NULL) {
		fh2m_innodpu_err(hw_dev->dev, "user image buffer invalid\n");
		return -1;
	}

	/* get pdp current info by chip callback */
	if ((!hw_dev->base_info) || hw_dev->base_info(hw_dev, base_info)) {
		fh2m_innodpu_err(hw_dev->dev, "capture layer disabled.\n");
		return -1;
	}
	dev_paddr = base_info[0];
	cpu_paddr = base_info[1];
	width = INNO_LOWER_32_BITS(base_info[2]);
	width = hw_dev->combi ? width * 2 : width;
	height = INNO_LOWER_32_BITS(base_info[3]);
	stride  = INNO_LOWER_32_BITS(base_info[4]);

	/* get current inno gem obj */
	base_info[1] = 0;
	idr_for_each(&pdp0_drm->drm_dev->object_name_idr, for_each_gem_object, base_info);
	if (base_info[1] == 0) {
			return -EFAULT;
	}
	innodpu_obj = (innodpu_gem_object *)base_info[1];
	mem_manager = innodpu_obj->mem_manager;

	/* calculate crop start addr */
	split_y = (height / info.split) * (info.part -1);
	split_h = (height / info.split);
	dev_paddr += split_y * stride;
	cpu_paddr += split_y * stride;

	/* malloc temp buffer */
	copy_size = split_h * stride;
	copy_buf = fh2m_inno_vmalloc(copy_size);
	if (IS_ERR_OR_NULL(copy_buf)) {
		fh2m_innodpu_err(pdp0_drm->dev, "no memory.\n");
		return -ENOMEM;
	}

	/* copy image data from gram to ram */
	if (mem_manager->visible) {
		void *src = NULL, *dst = NULL;
		int size = copy_size;

		src = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdp0_drm->dev->parent, cpu_paddr);
		dst = (void*)copy_buf;
		innodpu_dma_memcpy3(pdp0_drm->dev->parent, &src, &dst, &size, 1, GDDR2SYS);
	} else {
		void *axi_vram_src = NULL;
		void *dst = NULL;
		int size = copy_size;

		axi_vram_src = (void *)dev_paddr;
		dst = copy_buf;
		innodpu_dma_memcpy_for_smallbar_sg(pdp0_drm->dev->parent, &axi_vram_src, &dst, &size, 1, GDDR2SYS);
	}

	/* copy data from kernel to userspace */
	row_len = width * 4;
	temp_buf = copy_buf;
	user_buf = info.buf;
	for (i = 0; i < split_h; i++) {
		copy_ret = copy_to_user(user_buf , temp_buf, row_len);
		if (copy_ret) {
			fh2m_innodpu_err(hw_dev->dev, "capture: copy_to_user len err\n");
		}
		temp_buf += stride;
		user_buf += row_len;
	}

	/* update userspace size info */
	info.width = width;
	info.height = split_h;
	copy_ret = copy_to_user((void *)arg, &info, sizeof(struct user_capture_info));
	if (copy_ret) {
		fh2m_innodpu_err(hw_dev->dev, "capture: update user info err\n");
		return -1;
	}

	/* free temp buffer */
	fh2m_inno_vfree(copy_buf);

	return 0;
}

static long pdp0_base_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	/* ioctl info check here, maybe need add more check */
	if (arg == 0x0) {
		return -1;
	}

	switch (cmd) {
	case CAPTURE_GET_IMAGE:
		ret = pdp0_base_split_image(file, arg);
		break;
	case CAPTURE_GET_INFO:
		ret = pdp0_base_get_size(file, arg);
		break;
	}

	return ret;
}

static ssize_t pdp0_base_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_crtc *crtc = file->private_data;
	static void *map = NULL;
	static u64 map_size = 0;
	static u64 read_count = 0;
	ssize_t ret = 0;
	loff_t position = *ppos;

	if (position < 0 || size == 0)
		return -EINVAL;

	pdp0_drm = crtc_to_pdp0_device(crtc);

	if (!position) {
		ret = pdp0_snapshot_map(&map, &map_size,
					pdp0_drm->hwdev->base_info, pdp0_drm);
		if (ret) {
			return ret;
		}
	}

	if (size > map_size - read_count)
		size = map_size - read_count;
	if (size) {
		if (copy_to_user(buf, map + read_count, size)) {
			ret = -EFAULT;
		}
	}

	read_count += size;
	ret = size;
	*ppos = position + size;

	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "read count:%lld\n", read_count);

	if (!size) {
		read_count = 0;
		map_size = 0;
		fh2m_inno_vfree(map);
		ret = 0;
	}

	return ret;
}

static int pdp0_cur_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;
	file->private_data = crtc;

	return 0;
}

static ssize_t pdp0_cur_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_crtc *crtc = file->private_data;
	static void *map = NULL;
	static u64 map_size = 0;
	static u64 read_count = 0;
	ssize_t ret = 0;
	loff_t position = *ppos;

	if (position < 0 || size == 0)
		return -EINVAL;

	pdp0_drm = crtc_to_pdp0_device(crtc);

	if (!position) {
		ret = pdp0_snapshot_map(&map, &map_size,
					pdp0_drm->hwdev->cur_info, pdp0_drm);
		if (ret) {
			return ret;
		}
	}

	if (size > map_size - read_count)
		size = map_size - read_count;
	if (size) {
		if (copy_to_user(buf, map + read_count, size)) {
			ret = -EFAULT;
		}
	}

	read_count += size;
	ret = size;
	*ppos = position + size;

	fh2m_innodpu_info(crtc->dev->dev, DPU_UT_DPU, "read count:%lld\n", read_count);

	if (!size) {
		read_count = 0;
		map_size = 0;
		fh2m_inno_vfree(map);
		ret = 0;
	}

	return ret;
}

static int pdp0_reg_info(struct seq_file *m, void *data)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_crtc *crtc = m->private;

	pdp0_drm = crtc_to_pdp0_device(crtc);

	if (pdp0_drm->hwdev->reg_dump) {
		pdp0_drm->hwdev->reg_dump(m, data);
	}

	return 0;
}

static int pdp0_crtc_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *dev = inode->i_private;

	return single_open(file, pdp0_reg_info, dev);
}

static int pdp0_bisttest_show_ops(struct seq_file *m, void *data)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct drm_crtc *crtc = NULL;

	if (!m)
		return -EINVAL;
	crtc = m->private;

	if (!crtc)
		return -EINVAL;
	pdp0_drm = crtc_to_pdp0_device(crtc);

	if (!pdp0_drm)
		return -EINVAL;

	if (pdp0_drm->hwdev->bisttest_show) {
		pdp0_drm->hwdev->bisttest_show(m, crtc);
	}

	return 0;
}

static int pdp0_bisttest_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *dev = NULL;

	if (!inode || !inode->i_private || !file)
		return -EINVAL;
	dev = inode->i_private;

	return single_open(file, pdp0_bisttest_show_ops, dev);
}

static ssize_t pdp0_bisttest_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct seq_file *m = NULL;
	struct drm_crtc *crtc = NULL;
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	int ret = 0;

	if (!file)
		return -EINVAL;
	m = file->private_data;

	if (!m)
		return -EINVAL;
	crtc = m->private;

	if (!crtc)
		return -EINVAL;
	pdp0_drm = crtc_to_pdp0_device(crtc);

	if (!pdp0_drm)
		return -EINVAL;

	if (pdp0_drm->hwdev->bisttest_write)
		ret = pdp0_drm->hwdev->bisttest_write(m, crtc, buf, size, ppos);

	return ret;
}

static const struct file_operations pdp0_crtc_fops = {
	.owner = THIS_MODULE,
	.open = pdp0_crtc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations pdp_base_img_fops = {
	.owner = THIS_MODULE,
	.open = pdp0_base_open,
	.read = pdp0_base_read,
	.unlocked_ioctl = pdp0_base_ioctl,
	.llseek = default_llseek,
};

static const struct file_operations pdp_cur_img_fops = {
	.owner = THIS_MODULE,
	.open = pdp0_cur_open,
	.read = pdp0_cur_read,
	.llseek = default_llseek,
};

static const struct file_operations pdp_bisttest_fops = {
	.owner = THIS_MODULE,
	.open = pdp0_bisttest_open,
	.read = seq_read,
	.write = pdp0_bisttest_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pdp0_crtc_late_register(struct drm_crtc *crtc)
{
	struct drm_minor *minor = crtc->dev->primary;
	struct dentry *ent = NULL;
	struct dentry *root = NULL;
	u8 i = 0;

	const char *debug_name[] ={ "reg_info",
								"base_img",
								"cur_img",
								"bisttest"};
	const struct file_operations *debug_fops[] = {
								&pdp0_crtc_fops,
								&pdp_base_img_fops,
								&pdp_cur_img_fops,
								&pdp_bisttest_fops};

#if defined(CONFIG_DEBUG_FS)
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	root = crtc->debugfs_entry;
#else
	root = debugfs_create_dir(crtc->name, minor->debugfs_root);
	if (!root)
		return -ENOMEM;
#endif
#endif
	if (root) {
		for (i = 0; i < INNO_ARRAY_SIZE(debug_name); i++) {
			ent = debugfs_create_file(debug_name[i], S_IRUGO | S_IWUSR,
						root, crtc, debug_fops[i]);
			if (!ent) {
				fh2m_innodpu_err(crtc->dev->dev, "create debug node /sys/kernel/"
						"debug/dri/%d/crtc-%d/%s Error.\n",
						minor->index, crtc->index, debug_name[i]);
			}
		}
	}

	return 0;
}

static void pdp0_crtc_early_unregister(struct drm_crtc *crtc)
{

}

static int inno_dpu_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
								uint32_t handle, uint32_t width, uint32_t height,
								int32_t hot_x, int32_t hot_y)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct innodpu_pdp0_hw_device *hwdev = NULL;

	pdp0_drm = crtc_to_pdp0_device(crtc);
	hwdev = pdp0_drm->hwdev;


	if (hwdev->cursor_set2) {
		hwdev->cursor_set2(crtc, file_priv, handle,
				width, height, hot_x, hot_y);
	}

	return 0;
}

static int inno_dpu_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
							   uint32_t handle, uint32_t width, uint32_t height)
{
	return 0;
}

static int inno_dpu_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct innodpu_pdp0_drm *pdp0_drm = NULL;
	struct innodpu_pdp0_hw_device *hwdev = NULL;

	pdp0_drm = crtc_to_pdp0_device(crtc);
	hwdev = pdp0_drm->hwdev;

	if ((hwdev->x_scaler != 0) || (hwdev->y_scaler != 0)){
		fh2m_innodpu_info(hwdev->dev, DPU_UT_CURSOR, "pdp-%d before pos (%d,%d), scaler(%d, %d), hot(%d,%d)\n",
			hwdev->dpu_id, x,y, hwdev->x_scaler, hwdev->y_scaler, hwdev->hot_x, hwdev->hot_y);

		if ((x + hwdev->hot_x) < 0) {
			x = fh2m_inno_abs(x + hwdev->hot_x) * hwdev->x_scaler / 10000 + hwdev->hot_x;
			x = -x;
		} else {
			x = (x + hwdev->hot_x) * hwdev->x_scaler / 10000 - hwdev->hot_x;
		}

		if ((y + hwdev->hot_y) < 0) {
			y = fh2m_inno_abs(y + hwdev->hot_y) * hwdev->y_scaler / 10000 + hwdev->hot_y;
			y = -y;
		} else {
			y = (y + hwdev->hot_y) * hwdev->y_scaler / 10000 - hwdev->hot_y;
		}

		fh2m_innodpu_info(hwdev->dev, DPU_UT_CURSOR, "pdp-%d end pos (%d,%d)\n", hwdev->dpu_id, x,y);
		// x pos : (x + hot_x) * crtc_xscaler - hot_x;
		// y pos : (y + hot_y) * crtc_yscaler - hot_y;
	}

	if (hwdev->cursor_move) {
		hwdev->cursor_move(crtc, x, y);
	}

	return 0;
}

static const struct drm_crtc_funcs pdp0_crtc_funcs = {
	.cursor_set = inno_dpu_cursor_set,
	.cursor_set2 = inno_dpu_cursor_set2,
	.cursor_move = inno_dpu_cursor_move,
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	.page_flip = pdp0_crtc_page_flip,
#else
	.page_flip = drm_atomic_helper_page_flip,
#endif
	.reset = pdp0_crtc_reset,
	.atomic_duplicate_state = pdp0_crtc_duplicate_state,
	.atomic_destroy_state = pdp0_crtc_destroy_state,
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
	.enable_vblank = pdp0_crtc_enable_vblank,
	.disable_vblank = pdp0_crtc_disable_vblank,
#endif
	.atomic_set_property = pdp0_crtc_atomic_set_property,
	.atomic_get_property = pdp0_crtc_atomic_get_property,
#if (DRM_VERSION >= KERNEL_VERSION(5, 12, 0))
//	.gamma_set = drm_atomic_helper_legacy_gamma_set,
#else
	.gamma_set = drm_atomic_helper_legacy_gamma_set,
#endif
	.late_register = pdp0_crtc_late_register,
	.early_unregister = pdp0_crtc_early_unregister,
};

static void innodpu_pdp0_attach_crtc_property(struct innodpu_pdp0_drm *pdp0_drm)
{
	struct drm_device *drm_dev = pdp0_drm->drm_dev;
	struct drm_crtc *crtc = &pdp0_drm->crtc;
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	/* attach private property */
#define ATTACH_PROPERTY(prop, initval) \
		drm_object_attach_property(&crtc->base, prop, initval)
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_WB_START], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_WB_SAVE], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_PVR_ENABLE], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_PVR_TYPE], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_BIST], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_ISYUV], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_DISPLAY_ID], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_UVREVS], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_COMP_TILE_4x16], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_SET_WM_FD], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_DECOMP_TYPE], 0);
	ATTACH_PROPERTY(dev_priv->crtc_prop[INNODPU_CRTC_PROP_DECOMP_ADDR], 0);

#undef ATTACH_PROPERTY

	return;
}

int innodpu_pdp0_crtc_init(struct innodpu_pdp0_drm *pdp0_drm)
{
	int ret = 0;
	int i = 0;
	struct drm_device *drm_dev = pdp0_drm->drm_dev;
	struct drm_crtc *crtc = &pdp0_drm->crtc;
	struct drm_plane *primary = NULL;
	struct drm_plane *cursor = NULL;
	struct drm_plane *plane = NULL;

	ret = innodpu_pdp0_de_planes_init(pdp0_drm);
	if (ret < 0) {
		fh2m_innodpu_err(drm_dev->dev, "dpu%d failed init plane", pdp0_drm->dpu_id);
		return ret;
	}

	for (i = 0; i < PDP0_LAYERS; ++i) {
		plane = &pdp0_drm->plane[i]->base;
		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = plane;
		if (plane->type == DRM_PLANE_TYPE_CURSOR)
			cursor = plane;
	}
	if (!primary) {
		fh2m_innodpu_err(drm_dev->dev, "dpu%d no primary", pdp0_drm->dpu_id);
		ret = -EINVAL;
		goto err_no_primary;
	}
	ret = drm_crtc_init_with_planes(drm_dev, crtc, primary, NULL,
									&pdp0_crtc_funcs, "dpu%d", pdp0_drm->dpu_id);
	if (ret) {
		fh2m_innodpu_err(drm_dev->dev, "dpu%d failed init plane", pdp0_drm->dpu_id);
		goto err_init_crtc;
	}

	if (pdp0_drm->hwdev->is_nulldisp) {
		fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%s is nulldisplay!!!\n", crtc->name);
		hrtimer_init(&pdp0_drm->timer_hr, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		pdp0_drm->timer_hr.function = htimer_func;

		if (s_dpu_support_plane_fd) {
			hrtimer_init(&pdp0_drm->timer_active, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			pdp0_drm->timer_active.function = active_timer_func;
			pdp0_drm->active_wq = fh2m_inno_waitqueue_head_alloc();
			if (pdp0_drm->active_wq != NULL) {
				fh2m_inno_init_waitqueue_head(pdp0_drm->active_wq);
			}

			atomic64_set(&pdp0_drm->active_count, 0);
		}
	}

	pdp0_drm->vga_adapt_nb.notifier_call = pdp0_for_vga_notifier_cb;
	pdp0_drm->lg_layer_enabled = false;
	atomic_set(&pdp0_drm->vga_nb_registered, 0);

	fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "%s crtc_id:%d, primary:%d, dpu_id=%d\n",
			 crtc->name, crtc->base.id, primary->base.id, pdp0_drm->dpu_id);

	drm_crtc_helper_add(crtc, &pdp0_crtc_helper_funcs);
	drm_mode_crtc_set_gamma_size(crtc, PDP0_GAMMA_LUT_SIZE);
	/* No inverse-gamma: it is per-plane. */
	drm_crtc_enable_color_mgmt(crtc, 0, true, PDP0_GAMMA_LUT_SIZE);
	innodpu_pdp0_attach_crtc_property(pdp0_drm);
	return 0;

  err_init_crtc:
  err_no_primary:
	// innodpu_pdp0_de_planes_destroy(pdp0_drm);

	return ret;
}

void innodpu_pdp0_crtc_fini(struct innodpu_pdp0_drm *pdp0_drm)
{
	/*drm_crtc->destroy drm_plane->destroy interface cannot be called actively when the
	 * driver is uninstalled, but needs to be called to relase the resources by calling
	 * drm_mode_config_cleanup*/
	/*
	struct drm_crtc *crtc = &pdp0_drm->crtc;
	innodpu_pdp0_de_planes_destroy(pdp0_drm);
	if (crtc)
		drm_crtc_cleanup(crtc);
	*/

	if (pdp0_drm->hwdev->is_nulldisp) {
		hrtimer_cancel(&pdp0_drm->timer_hr);

		if (s_dpu_support_plane_fd) {
			hrtimer_cancel(&pdp0_drm->timer_active);
			if (pdp0_drm->active_wq != NULL) {
				fh2m_inno_waitqueue_head_free(pdp0_drm->active_wq);
				pdp0_drm->active_wq = NULL;
			}
		}
	}
}
