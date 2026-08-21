/*************************************************************************/ /*!
@File			pdp0_drv.c
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
#include "inno_drm_version.h"
#include <linux/component.h>
#include <linux/platform_device.h>

#include "pdp0_drv.h"
#include "pdp0_crtc.h"
#include "pdp0_plane.h"
#include "innogpu_drm.h"
#include "g0_pdp0_hw.h"
#include "g0m_pdp0_hw.h"
#include "g1_pdp0_hw.h"
#include "g1p_pdp0_hw.h"
#include "g3_pdp0_hw.h"

static void *innodpu_pdp0_get_gem(void *inno_fb)
{
	return ((struct inno_framebuffer *)inno_fb)->obj[0];
}

static int innodpu_pdp0_init(struct innodpu_pdp0_drm *pdp0_drm)
{
	int ret = 0;
	struct drm_device *drm_dev = pdp0_drm->drm_dev;

	ret = innodpu_pdp0_crtc_init(pdp0_drm);
	if (ret) {
		fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d crtc init failed.\n", pdp0_drm->dpu_id);
		return ret;
	}
	return 0;
}

static void innodpu_pdp0_fini(struct innodpu_pdp0_drm *pdp_drm)
{
	innodpu_pdp0_crtc_fini(pdp_drm);

	return;
}

static void innodpu_pdp_irq_handle(void *data)
{
	struct innodpu_pdp0_hw_device *hwdev = data;
	if (hwdev->irq_handle)
		hwdev->irq_handle(hwdev);
}

static void innodpu_pdp_platform_init(struct innodpu_pdp0_drm *pdp0_drm, struct innodpu_pdp0_hw_device *hwdev, u32 dpu_id)
{
	switch(pdp0_drm->plat) {
		case CHIP_G1_SOC:
		case CHIP_G1_NE:
		case CHIP_G1_PAL:
			g1_pdp0_hw_init(hwdev, dpu_id);
			break;
		case CHIP_G0_SOC:
		case CHIP_G0_NE:
		case CHIP_G0_PAL:
			g0_pdp0_hw_init(hwdev, dpu_id);
			break;
		case CHIP_G1P_SOC:
		case CHIP_G1P_NE:
		case CHIP_G1P_PAL:
			g1p_pdp0_hw_init(hwdev, dpu_id);
			break;
		case CHIP_G0M_SOC:
		case CHIP_G0M_NE:
		case CHIP_G0M_PAL:
			g0m_pdp0_hw_init(hwdev, dpu_id);
			break;
		case CHIP_G3_SOC:
		case CHIP_G3_NE:
		case CHIP_G3_PAL:
			g3_pdp0_hw_init(hwdev, dpu_id);
			break;
		default:
			hwdev->bus_align = 32;
			hwdev->setqos = false;
			break;
	}

	if (hwdev->is_nulldisp) {
		/* force hwdev valid size[2, 4096] when pdp as nulldisp device */
		hwdev->max_width = 4096;
		hwdev->max_height = 4096;
		hwdev->min_width = 2;
		hwdev->min_height = 2;
	}

}

static bool innodpu_pdp_filter(struct device *dev)
{
	plat_data_t *pdata = dev->platform_data;
	chip_type_e plat = fh2m_hal_get_chiptype(dev->parent);
	bool ret = false;

	switch (plat) {
	case CHIP_G0M_SOC:
	case CHIP_G0M_NE:
	case CHIP_G0M_PAL:
		ret = g0m_pdp_filter(pdata->dev_idx);
	break;
	case CHIP_G3_SOC:
	case CHIP_G3_NE:
	case CHIP_G3_PAL:
		ret = g3_pdp_filter(pdata->dev_idx);
	break;
	default:
		ret = false;
	break;
	}

	return ret;
}

static void pdp0_crtc_qos_enable(struct drm_crtc *crtc)
{
	struct innodpu_pdp0_drm *pdp0_drm = crtc_to_pdp0_device(crtc);
	struct innodpu_pdp0_hw_device *hwdev = pdp0_drm->hwdev;
	struct videomode vm;
	struct drm_display_mode default_modes = {
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC)
	};

	drm_display_mode_to_videomode(&default_modes, &vm);

	switch(pdp0_drm->plat) {
	case CHIP_G0_SOC:
		fh2m_hal_set_pll(pdp0_drm->dev->parent, PLL_LVDS, 148500);
		fh2m_hal_pdp_video_set(pdp0_drm->dev->parent, pdp0_drm->dpu_id, REG_M_LVDS);
		hwdev->modeset(hwdev, &vm, false);
		hwdev->leave_config_mode(hwdev);
		hwdev->enter_config_mode(hwdev);
		break;
	case CHIP_G0M_SOC:
#if 0
		fh2m_hal_set_pll(pdp0_drm->dev->parent, PLL_VGA, 148500);
		fh2m_hal_pdp_video_set(pdp0_drm->dev->parent, pdp0_drm->dpu_id, REG_M_VGA);
		hwdev->modeset(hwdev, &vm, false);
		hwdev->leave_config_mode(hwdev);
		hwdev->enter_config_mode(hwdev);
#endif
		break;
	default:
		break;
	}
}


int innodpu_pdp0_bind(struct device *dev, struct device *master,
		void *data, bool null_display)
{
	int ret = -EINVAL;
	u32 dpu_id;

	u32 rsize = 64;
	struct drm_device *drm_dev;
	struct innodpu_pdp0_drm *pdp0_drm;
	struct innodpu_pdp0_hw_device *hwdev;
	struct innodpu_drm_private *dev_priv;
	plat_data_t *pdata;

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;

	if (innodpu_pdp_filter(dev) && !null_display) {
		dpu_info(dev, "Inno dpu%d bind filter\n", dpu_id);
		return 0;
	}

	drm_dev = (struct drm_device *)data;
	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

	dpu_info(drm_dev->dev, "Inno dpu%d bind start %s\n", dpu_id,
			null_display ? "null_display" : "hwdev");

	if (dev_priv->innodpu_drm[dpu_id]) {
		fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d bind failed. "
				"Device is already registered.\n", dpu_id);
		return -EBUSY;
	}

	pdp0_drm = devm_kzalloc(dev, sizeof(*pdp0_drm), fh2m_hal_get_inno_gfp_kernel());
	if (IS_ERR(pdp0_drm)) {
		fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d  drm alloc failed. "
				"Short of memory.\n", dpu_id);
		return PTR_ERR(pdp0_drm);
	}
	dev_priv->innodpu_drm[dpu_id] = pdp0_drm;
	pdp0_drm->drm_dev = drm_dev;
	pdp0_drm->dpu_id = dpu_id;
	pdp0_drm->dev = get_device(dev);
	pdp0_drm->plat = fh2m_hal_get_chiptype(dev->parent);

	dev_set_drvdata(dev, pdp0_drm);

	hwdev = devm_kzalloc(dev, sizeof(*hwdev), fh2m_hal_get_inno_gfp_kernel());
	if (IS_ERR(hwdev)) {
		fh2m_innodpu_err(drm_dev->dev, "Inno dpu%d hwdev alloc failed. "
				"Short of memory.\n", dpu_id);
		goto err_alloc_hwdev;
	}
	memcpy(hwdev, &pdp0_hw_device, sizeof(*hwdev));

	if(hwdev->init_format_id)
		hwdev->init_format_id();

	hwdev->dpu_id = dpu_id;
	hwdev->dev = dev;
	hwdev->modules = dpu_id + REG_M_DPU0;
	hwdev->rotation_memory[0] = hwdev->rotation_memory[1] = rsize * SZ_1K;
	if (null_display) {
		hwdev->is_normal_mode = false;
		hwdev->is_nulldisp = true;
	} else {
		hwdev->is_normal_mode = false;
		hwdev->is_nulldisp = false;
	}
	innodpu_pdp_platform_init(pdp0_drm, hwdev, dpu_id);

	pdp0_drm->hwdev = hwdev;

	hwdev->coladj_en = false;
	hwdev->gamma_en = false;
	hwdev->pvric_en = false;

	atomic_set(&pdp0_drm->vblank_enable, 0);
	atomic_set(&pdp0_drm->config_valid, 0);
	init_waitqueue_head(&pdp0_drm->wq);

	hwdev->wq = &pdp0_drm->wq;
	hwdev->config_valid = &pdp0_drm->config_valid;
	hwdev->vblank_enable = &pdp0_drm->vblank_enable;
	hwdev->crtc = &pdp0_drm->crtc;
	hwdev->drm_dev = pdp0_drm->drm_dev;
	hwdev->plane = pdp0_drm->plane;
	hwdev->get_gem = innodpu_pdp0_get_gem;
	hwdev->pframe_size = dev_priv->frame_size;
	hwdev->pframe_width = dev_priv->frame_width;
	hwdev->pframe_height = dev_priv->frame_height;
	hwdev->pframe_y_width_align = dev_priv->frame_y_width_align;
	hwdev->pframe_y_size_align = dev_priv->frame_y_size_align;
	hwdev->pframe_uv_width_align = dev_priv->frame_uv_width_align;
	hwdev->pframe_uv_size_align = dev_priv->frame_uv_size_align;
	hwdev->ppitch = dev_priv->pitch;
	hwdev->pformat = dev_priv->format;

	ret = innodpu_pdp0_init(pdp0_drm);
	if (ret) {
		goto pdp0_init_fail;
	}

	if (null_display) {
		return 0;
	}

	if (hwdev->hardware_fini)
		hwdev->hardware_fini(hwdev);

	if (hwdev->hardware_init)
		hwdev->hardware_init(hwdev);

	if (hwdev->init_qos)
		pdp0_crtc_qos_enable(&pdp0_drm->crtc);

	ret = fh2m_hal_set_irq_handler(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0,
							  innodpu_pdp_irq_handle, hwdev);
	if (ret) {
		fh2m_innodpu_err(drm_dev->dev, "failed to set interrupt handler (err=%d)\n", ret);
		goto hal_irq_fail;
	}

	ret = fh2m_hal_dev_enable_irq(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0);
	if (ret) {
		fh2m_innodpu_err(drm_dev->dev, "failed to enable dpu%d interrupts (err=%d)\n", dpu_id, ret);
		goto hal_fail;
	}
	fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "Inno dpu%d bind end\n", dpu_id);

	return 0;

hal_fail:
	/* when it return error and hw has been enabled, to force disabled it.*/
	fh2m_hal_dev_disable_irq(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0);
	fh2m_hal_set_irq_handler(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0, NULL, NULL);
hal_irq_fail:
// pdp0_irq_init_fail:
	innodpu_pdp0_fini(pdp0_drm);
pdp0_init_fail:
	if (hwdev->hardware_fini)
		hwdev->hardware_fini(hwdev);
err_alloc_hwdev:
	if (dev_priv->innodpu_drm[dpu_id]) {
		if (hwdev) {
			devm_kfree(dev, hwdev);
		}
		devm_kfree(dev, dev_priv->innodpu_drm[dpu_id]);
		dev_priv->innodpu_drm[dpu_id] = NULL;
		put_device(dev);
	}

	return ret;
}


void innodpu_pdp0_unbind(struct device *dev, struct device *master,
		void *data, bool null_display)
{
	u32 dpu_id = 0;	// dpu_id [2,7]
	struct drm_device *drm_dev = (struct drm_device *)data;
	struct innodpu_pdp0_drm *pdp_drm = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;
	if (!pdp_drm)
		return;

	if (innodpu_pdp_filter(dev) && !null_display)
		return;
	dpu_id = pdp_drm->dpu_id;
	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "Inno dpu%d unbind start\n", dpu_id);
	if (!null_display) {
		fh2m_hal_dev_disable_irq(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0);
		fh2m_hal_set_irq_handler(drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0, NULL, NULL);
	}
	innodpu_pdp0_fini(pdp_drm);

	if (!null_display && pdp_drm->hwdev->hardware_fini) {
		pdp_drm->hwdev->hardware_fini(pdp_drm->hwdev);
	}

	devm_kfree(dev,pdp_drm->hwdev);
	devm_kfree(dev,pdp_drm);
	put_device(dev);
	dev_set_drvdata(dev, NULL);
	dev_priv->innodpu_drm[dpu_id] = NULL;
	fh2m_innodpu_info(drm_dev->dev, DPU_UT_DPU, "Inno dpu%d unbind end\n", dpu_id);

	return;
}

int innodpu_pdp0_suspend(struct device *dev, int dpu_id)
{
	struct innodpu_pdp0_drm *pdp_drm = dev_get_drvdata(dev);
	if (!pdp_drm)
		return 0;

	dpu_info(dev, "pdp-%d start suspend.\n", dpu_id);

	if (innodpu_pdp_filter(dev))
		return 0;

	if (pdp_drm->hwdev->is_nulldisp) {
		return 0;
	}

	fh2m_hal_dev_disable_irq(pdp_drm->drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0);

	// 1. 关闭当前中断
	if (pdp_drm->hwdev->hardware_fini)
		pdp_drm->hwdev->hardware_fini(pdp_drm->hwdev);

	return 0;
}

int innodpu_pdp0_shutdown(struct device *dev, int dpu_id)
{
	dpu_info(dev, "pdp-%d start shutdown.\n", dpu_id);

	if (innodpu_pdp_filter(dev))
		return 0;

	return innodpu_pdp0_suspend(dev, dpu_id);
}


int innodpu_pdp0_resume(struct device *dev, int dpu_id)
{
	int ret;
	struct innodpu_pdp0_drm *pdp0_drm = dev_get_drvdata(dev);
	struct innodpu_pdp0_hw_device *hwdev = NULL;

	if (!pdp0_drm)
		return 0;

	hwdev = pdp0_drm->hwdev;

	if (innodpu_pdp_filter(dev) && (!hwdev->is_nulldisp))
		return 0;

	dpu_info(dev, "pdp-%d start resume.\n", dpu_id);

	innodpu_pdp_platform_init(pdp0_drm, hwdev, dpu_id);
	if (hwdev->is_nulldisp) {
		return 0;
	}

	if (hwdev->hardware_fini)
		hwdev->hardware_fini(hwdev);

	if (hwdev->hardware_init)
		hwdev->hardware_init(hwdev);

	if (hwdev->init_qos)
		pdp0_crtc_qos_enable(&pdp0_drm->crtc);

	ret = fh2m_hal_set_irq_handler(pdp0_drm->drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0,
							  innodpu_pdp_irq_handle, hwdev);
	if (ret) {
		fh2m_innodpu_err(pdp0_drm->drm_dev->dev, "failed to set interrupt handler (err=%d)\n", ret);
	}

	ret = fh2m_hal_dev_enable_irq(pdp0_drm->drm_dev->dev, dpu_id + HAL_INTERRUPT_DPU_0);
	if (ret) {
		fh2m_innodpu_err(pdp0_drm->drm_dev->dev, "failed to enable dpu%d interrupts (err=%d)\n", dpu_id,
				ret);
	}

	return 0;
}
