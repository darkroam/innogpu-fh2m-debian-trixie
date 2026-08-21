/*************************************************************************/ /*!
@File			innodpu_dpu.c
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
#include "innodpu_dpu.h"
#include "innodpu_common.h"
#include "innogpu.h"
#include "innogpu_drm.h"
#include "pdp0_drv.h"

bool s_pdp0_debug = false;
module_param(s_pdp0_debug, bool, 0600);
MODULE_PARM_DESC(s_pdp0_debug, "pdp0 debug(default: false)");

unsigned int s_wb_size = 0x200000;
module_param(s_wb_size, uint, 0444);
MODULE_PARM_DESC(s_wb_size, "wb size (default: 0x200000)");

int s_dp_vga = -1;
module_param(s_dp_vga, int, 0444);
MODULE_PARM_DESC(s_dp_vga, "dp to vga enable(-1 not support, 0-dp, 1-vga)");

unsigned int s_hdmi_dvi = 0x00;
module_param(s_hdmi_dvi, uint, 0600);
MODULE_PARM_DESC(s_hdmi_dvi, "hdmi to dvi enable(-1 not support, 0x01-hdmi0 to dvi, 0x02-hdmi1 to dvi)");

int s_hdmi_freq = 0;
module_param(s_hdmi_freq, int, 0644);
MODULE_PARM_DESC(s_hdmi_freq, "Only debug, to set hdmi freq target.");

int s_hdmi_ddcfreq = 0;
module_param(s_hdmi_ddcfreq, int, 0644);
MODULE_PARM_DESC(s_hdmi_ddcfreq, "set hdmi ddcfreq(kHz)");

int s_hdmi_anacfg[12] = {0};
module_param_array(s_hdmi_anacfg, int, NULL, 0644);
MODULE_PARM_DESC(s_hdmi_anacfg, "set hdmi anacfg, if (t && (!pclk || pclk == match_clock)) to take effect.\n"
		"format: t, 1b3, 1bb, 1bc, 1bd, 1b5, 1b6, 1b7, 1b8, 1bf, 1c0, pclk");



static int innodpu_bind(struct device *dev, struct device *master, void *data)
{
	int dpu_id = 0;
	plat_data_t *pdata;
	struct drm_device * drm_dev = data;
	struct innodpu_drm_private *dev_priv;
	unsigned dpu_nums = fh2m_hal_get_dev_nums(drm_dev->dev, DEV_DPU);
	unsigned db9000_nums = fh2m_hal_get_dev_nums(drm_dev->dev, DEV_DB9000);

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "Invaild dev_priv\n");
		return -EINVAL;
	}

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;

#if defined(__G3_NE__) || defined(__G3_PAL__)
	if (!(s_dpu_nums & BIT(dpu_id)))
		return 0;
#endif

	if (dpu_id > dpu_nums) {
		fh2m_innodpu_err(drm_dev->dev, "Invaild dpu_id:%d\n", dpu_id);
		return -EINVAL;
	}

	if (!(s_dpu_nums & INNO_BIT(dpu_id)) && !dev_priv->drm_nulldisplay)
		return 0;

	if (dpu_id < db9000_nums && !dev_priv->drm_nulldisplay) {
		fh2m_innodpu_warn(drm_dev->dev, "Not support db9000\n");
		return 0;
	} else {
		return innodpu_pdp0_bind(dev, master, data, dev_priv->drm_nulldisplay);
	}
}

static void innodpu_unbind(struct device *dev, struct device *master, void *data)
{
	int dpu_id = 0;
	plat_data_t *pdata;
	unsigned dpu_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DPU);
	unsigned db9000_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DB9000);
	struct drm_device * drm_dev = data;
	struct innodpu_drm_private *dev_priv;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "Invaild dev_priv\n");
		return;
	}

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;
	if (dpu_id > dpu_nums) {
		fh2m_innodpu_err(drm_dev->dev, "Invaild dpu_id:%d\n", dpu_id);
		return;
	}
#if defined(__G3_NE__) || defined(__G3_PAL__)

	if (!(s_dpu_nums & BIT(dpu_id)))
		return;
#endif

	if (!(s_dpu_nums & INNO_BIT(dpu_id)) && !dev_priv->drm_nulldisplay)
		return;

	if (dpu_id < db9000_nums && !dev_priv->drm_nulldisplay) {
		fh2m_innodpu_warn(drm_dev->dev, "Not support db9000\n");
	} else {
		innodpu_pdp0_unbind(dev, master, data, dev_priv->drm_nulldisplay);
	}
}

static const struct component_ops innodpu_ops = {
	.bind = innodpu_bind,
	.unbind = innodpu_unbind,
};

static int innodpu_platform_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &innodpu_ops);
}

static int innodpu_platform_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &innodpu_ops);
	return 0;
}

static void innodpu_dpu_shutdown(struct platform_device *pdev)
{
	plat_data_t *pdata;
	int dpu_id = 0;
	struct device *dev = &pdev->dev;
	unsigned dpu_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DPU);
	unsigned db9000_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DB9000);
	unsigned pdp0_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_PDP0);

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;

	if (dpu_id > dpu_nums) {
		fh2m_innodpu_err(dev, "Invaild dpu_id:%d\n", dpu_id);
		return;
	}

	if (!(s_dpu_nums & INNO_BIT(dpu_id)))
		return;

	if (dpu_id < db9000_nums) {
		fh2m_innodpu_warn(dev, "Not support db9000\n");
		return;
	} else {
		if (dpu_id - db9000_nums < pdp0_nums)
			innodpu_pdp0_shutdown(dev, dpu_id);
		else
			fh2m_innodpu_warn(dev, "Not support dpu_id:%d\n", dpu_id);
	}

	return;

}

#ifdef CONFIG_PM_SLEEP
static int innodpu_dpu_suspend(struct device *dev)
{
	plat_data_t *pdata;
	int dpu_id = 0;
	unsigned dpu_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DPU);
	unsigned db9000_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DB9000);
	unsigned pdp0_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_PDP0);

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;

	if (dpu_id > dpu_nums) {
		fh2m_innodpu_err(dev, "Invaild dpu_id:%d\n", dpu_id);
		return -EINVAL;
	}

	if (!(s_dpu_nums & INNO_BIT(dpu_id)))
		return 0;

	if (dpu_id < db9000_nums) {
		fh2m_innodpu_warn(dev, "Not support db9000\n");
		return 0;
	} else {
		if (dpu_id - db9000_nums < pdp0_nums)
			return innodpu_pdp0_suspend(dev, dpu_id);
		else
			fh2m_innodpu_warn(dev, "Not support dpu_id:%d\n", dpu_id);
	}

	return -EINVAL;
}

static int innodpu_dpu_resume(struct device *dev)
{
	plat_data_t *pdata;
	int dpu_id = 0;

	unsigned dpu_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DPU);
	unsigned db9000_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_DB9000);
	unsigned pdp0_nums = fh2m_hal_get_dev_nums(dev->parent, DEV_PDP0);

	pdata = dev->platform_data;
	dpu_id = pdata->dev_idx;

	if (dpu_id > dpu_nums) {
		fh2m_innodpu_err(dev, "Invaild dpu_id:%d\n", dpu_id);
		return -EINVAL;
	}

	if (!(s_dpu_nums & INNO_BIT(dpu_id)))
		return 0;

	if (dpu_id < db9000_nums) {
		fh2m_innodpu_warn(dev, "Not support db9000\n");
		return 0;
	} else {
		if (dpu_id - db9000_nums < pdp0_nums)
			return innodpu_pdp0_resume(dev, dpu_id);
		else
			fh2m_innodpu_warn(dev, "Not support dpu_id:%d\n", dpu_id);
	}

	return -EINVAL;

}
#endif

static const struct dev_pm_ops innodpu_dpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(innodpu_dpu_suspend, innodpu_dpu_resume)
};

struct platform_driver g_innogpu_dpu_driver = {
	.probe = innodpu_platform_probe,
	.remove = innodpu_platform_remove,
	.shutdown = innodpu_dpu_shutdown,
	.driver = {
		.name = INNO_DPU_DEVICE_NAME,
		.pm = &innodpu_dpu_pm_ops,
		},
};
