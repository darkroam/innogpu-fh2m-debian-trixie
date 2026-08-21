/*************************************************************************/ /*!
@File			innodpu_vkms.c
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
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/component.h>

#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>

#if (DRM_VERSION >= KERNEL_VERSION(5, 1, 0))
#include <drm/drm_probe_helper.h>
#endif

#include "innodpu_connector.h"
#include "innodpu_common.h"
#include "innogpu.h"

/* This header must always be included last */
#include "kernel_compatibility.h"

#define ctx_from_connector(c)	container_of(c, struct inno_vkms_context,connector)

struct inno_vkms_context {
	struct drm_encoder encoder;
	struct platform_device *pdev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct edid *raw_edid;
	unsigned int connected;
	struct mutex lock;
};

static inline struct inno_vkms_context *encoder_to_vidi(struct drm_encoder *e)
{
	return container_of(e, struct inno_vkms_context, encoder);
}

//探测 connector 的物理连接状态，由于是虚拟的 connector，必须默认是已连接状态
static enum drm_connector_status inno_vkms_detect(struct drm_connector *connector, bool force)
{
	//只有是已连接状态，才会调用.fill_modes
	return connector_status_connected;
}

static void inno_vkms_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs s_inno_vkms_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = inno_vkms_detect,
	.destroy = inno_vkms_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_display_mode s_edid_cea_modes_tmp[] = {
// 3 - 720x480@60Hz
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
			  798, 858, 0, 480, 489, 495, 525, 0,
			  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
// 4 - 1280x720@60Hz 16:9
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
			  1430, 1650, 0, 720, 725, 730, 750, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
// 16 - 1920x1080@60Hz
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
// 97 - 3840x2160@60Hz 16:9
	{DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
			  4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
// 196 - 7680x4320@30Hz 16:9
	{DRM_MODE("7680x4320", DRM_MODE_TYPE_DRIVER, 1188000, 7680, 8232,
			  8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
};

#ifdef SUPPORT_8K
static int inno_vkms_add_extra_modes(struct drm_connector *connector, int hdisplay, int vdisplay)
{
	struct drm_display_mode *mode;
	int num_modes;
	int i;

	for (i = 0, num_modes = 0; i < INNO_ARRAY_SIZE(s_edid_cea_modes_tmp); i++) {
		const struct drm_display_mode *ptr = &s_edid_cea_modes_tmp[i];
		if (hdisplay && vdisplay) {
			if ((ptr->hdisplay > hdisplay) || (ptr->vdisplay > vdisplay)) {
				continue;
			}
		}
		if (drm_mode_vrefresh(ptr) > 61) {
			continue;
		}

		mode = drm_mode_duplicate(connector->dev, ptr);
		fh2m_innodpu_info(connector->kdev, DPU_UT_VKMS, "Video Timing Settings:" DRM_MODE_FMT "\n",
					  DRM_MODE_ARG(mode));
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}

	return num_modes;
}
#endif

static const struct drm_display_mode s_edid_required_modes[] = {
// 196 - 7680x4320@30Hz 16:9
	{DRM_MODE("7680x4320", DRM_MODE_TYPE_DRIVER, 1188000, 7680, 8232,
			  8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 1920x1080@30Hz 16:9 */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
			  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},

	/* 1920x1080@60Hz 16:9 */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},

	/* 1920x1080@75Hz 16:9 */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 174500, 1920, 1968,
			  2000, 2080, 0, 1080, 1083, 1088, 1119, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},

	/* 2560x1440@30Hz 16:9 */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 119000, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1461, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1440@60Hz 16:9 */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 241500, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1440@75Hz 16:9 */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 304250, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1492, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 3840x2160@30Hz 16:9 */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 3840x2160@60Hz 16:9 */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
};

static int inno_vkms_add_modes(struct drm_connector *connector, const struct drm_display_mode *ptr_mode,
	int mode_num, int hdisplay, int vdisplay)
{
	struct drm_display_mode *mode;
	int num_modes, i;

	for (i = 0, num_modes = 0; i < mode_num; i++) {
		const struct drm_display_mode *ptr = (ptr_mode + i);
		if (hdisplay && vdisplay) {
			if ((ptr->hdisplay > hdisplay) || (ptr->vdisplay > vdisplay)) {
				continue;
			}
		}
		if (drm_mode_vrefresh(ptr) > 76) {
			continue;
		}

		mode = drm_mode_duplicate(connector->dev, ptr);
		fh2m_innodpu_info(connector->kdev, DPU_UT_VKMS, "Video Timing Settings:" DRM_MODE_FMT "\n",
					  DRM_MODE_ARG(mode));
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}

	return num_modes;
}

/**
* inno_vkms_get_modes - 获取虚拟 connector 支持哪些分辨率
* @connector: drm_connector obj
* Returns:
* >0--支持的分辨率个数, <0 -- 对应错误码.
*/
static int inno_vkms_get_modes(struct drm_connector *connector)
{
	int num_modes = 0;

	//根据传入的 DPU 支持的最大分辨率，从内核全局数组中，筛选出符合要求的分辨率
	num_modes =
		drm_add_modes_noedid(connector, fh2m_hal_get_s_vkms_width(), fh2m_hal_get_s_vkms_height());

	num_modes += inno_vkms_add_modes(connector, s_edid_required_modes,
		INNO_ARRAY_SIZE(s_edid_required_modes), fh2m_hal_get_s_vkms_width(), fh2m_hal_get_s_vkms_height());

	//drop repeat mode
	num_modes -= innodpu_modes_drop_repeat(connector);

	drm_mode_sort(&connector->probed_modes);
	fh2m_innodpu_info(connector->kdev, DPU_UT_VKMS, "[CONNECTOR:%d:%s] found %d modes\n", connector->base.id,
				 connector->name, num_modes);

	return num_modes;

}

static const struct drm_connector_helper_funcs s_inno_vkms_connector_helper_funcs = {
	.get_modes = inno_vkms_get_modes,
};

static int inno_vkms_create_connector(struct drm_encoder *encoder)
{
	struct inno_vkms_context *ctx = encoder_to_vidi(encoder);
	struct drm_connector *connector = &ctx->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	//初始化虚拟的 connector
	ret =
		drm_connector_init(ctx->drm_dev, connector, &s_inno_vkms_connector_funcs,
						   DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret) {
		DRM_ERROR("Failed to initialize connector with drm\n");
		return ret;
	}
	//绑定 Connecter 的 helper_funcs 函数
	drm_connector_helper_add(connector, &s_inno_vkms_connector_helper_funcs);

	//将 Encoder 和 Connector 绑定
	drm_connector_attach_encoder(connector, encoder);

	return 0;
}

static void inno_vkms_mode_set(struct drm_encoder *encoder, struct drm_display_mode *mode,
							   struct drm_display_mode *adjusted_mode)
{
}

static void inno_vkms_encoder_enable(struct drm_encoder *encoder)
{
}

static void inno_vkms_encoder_disable(struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs s_inno_vkms_encoder_helper_funcs = {
	.mode_set = inno_vkms_mode_set,
	.enable = inno_vkms_encoder_enable,
	.disable = inno_vkms_encoder_disable,
};

static const struct drm_encoder_funcs s_inno_vkms_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

 /**
 * inno_vkms_bind - 初始化虚拟的 Encoder 和 Connector 设备
 * @dev: VKMS 平台设备的父设备
 * @master:master 设备
 * @data: component_bind_all传过来的参数
 * Returns:
 * 0--成功, <0 -- 对应错误码.
 */
static int inno_vkms_bind(struct device *dev, struct device *master, void *data)
{
	struct inno_vkms_context *ctx = dev_get_drvdata(dev);
	struct drm_device *ddev = (struct drm_device *)data;
	struct drm_encoder *encoder = &ctx->encoder;
	int ret;

#if !defined(__G3_PAL__) && !defined(__G3_PAL_2SPU__)
	plat_data_t *pdata = dev_get_platdata(dev);
#endif

	ctx->drm_dev = ddev;

	//初始化虚拟的 Encoder
	drm_encoder_init(ddev, encoder, &s_inno_vkms_encoder_funcs, DRM_MODE_ENCODER_VIRTUAL, NULL);

	//绑定 Encoder 的 helper_funcs 函数
	drm_encoder_helper_add(encoder, &s_inno_vkms_encoder_helper_funcs);
#if defined(__G3_PAL__) || defined(__G3_PAL_2SPU__)
	encoder->possible_crtcs = 0xff;
#else
	encoder->possible_crtcs = INNO_BIT(pdata->dev_idx % fh2m_hal_get_nulldisplay_drm_pipe_num());
#endif

	//初始化虚拟的 connector
	ret = inno_vkms_create_connector(encoder);
	if (ret) {
		DRM_ERROR("failed to create connector ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return ret;
	}
	fh2m_innodpu_info(dev, DPU_UT_VKMS, "vkms:attach [ENCODER:%d] to [CONNECTOR:%d]\n",
				  ctx->encoder.base.id, ctx->connector.base.id);

	return 0;
}

/**
* inno_vkms_unbind - 释放虚拟的 Encoder 和 Connector 设备
* @dev: VKMS 平台设备的父设备
* @master:master 设备
* @data: component_bind_all传过来的参数
* Returns:
* void
*/
static void inno_vkms_unbind(struct device *dev, struct device *master, void *data)
{
	return;
}

static const struct component_ops s_inno_vkms_component_ops = {
	.bind = inno_vkms_bind,
	.unbind = inno_vkms_unbind,
};

static int inno_vkms_probe(struct platform_device *pdev)
{
	struct inno_vkms_context *ctx;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), fh2m_hal_get_inno_gfp_kernel());
	if (!ctx)
		return -ENOMEM;

	ctx->pdev = pdev;
	mutex_init(&ctx->lock);
	fh2m_inno_platform_set_drvdata(pdev, ctx);

	ret = component_add(&pdev->dev, &s_inno_vkms_component_ops);
	if (ret) {
		inno_error("\n");
	}
	return ret;
}

static int inno_vkms_remove(struct platform_device *pdev)
{
	struct inno_vkms_context *ctx;

	ctx = fh2m_inno_platform_get_drvdata(pdev);
	if (ctx) {
		devm_kfree(&pdev->dev, ctx);
	}

	fh2m_inno_platform_set_drvdata(pdev, NULL);
	component_del(&pdev->dev, &s_inno_vkms_component_ops);
	return 0;
}

static struct platform_device_id s_inno_vkms_platform_device_id_table[] = {
	{.name = INNO_VKMS_DEVICE_NAME,.driver_data = 0},
	{},
};

MODULE_DEVICE_TABLE(platform, s_inno_vkms_platform_device_id_table);

static int innodpu_vkms_suspend(struct device *dev)
{
	fh2m_innodpu_info(dev, DPU_UT_VKMS, "suspend: %s: %d, %s\n", __FILE__, __LINE__, __func__);

	return 0;
}

static int innodpu_vkms_resume(struct device *dev)
{
	fh2m_innodpu_info(dev, DPU_UT_VKMS, "resume: %s: %d, %s\n", __FILE__, __LINE__, __func__);

	return 0;
}

static void innodpu_vkmd_shutdown(struct platform_device *pdev)
{
	innodpu_vkms_suspend(&pdev->dev);
}

static const struct dev_pm_ops innodpu_vkms_pm_ops = {
	.suspend = innodpu_vkms_suspend,
	.resume = innodpu_vkms_resume,
};

struct platform_driver g_innogpu_vkms_driver = {
	.probe = inno_vkms_probe,
	.remove = inno_vkms_remove,
	.shutdown = innodpu_vkmd_shutdown,
	.driver = {
			   .name = INNO_VKMS_DEVICE_NAME,
			   .owner = THIS_MODULE,
			   .pm = &innodpu_vkms_pm_ops,
			   },
	.id_table = s_inno_vkms_platform_device_id_table,
};
