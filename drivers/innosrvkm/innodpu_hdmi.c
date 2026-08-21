/*************************************************************************/ /*!
@File			innodpu_hdmi.c
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
#include <linux/platform_device.h>
#include <drm/drm_modes.h>
#include <drm/drm_edid.h>

#include "innodpu_hdmi.h"
#include "innodpu_hdmi_debugfs.h"
#include "innodpu_connector.h"

#ifdef CONFIG_DRM_INNO_AUDIO
#include "innoaudio_drv.h"
#endif

bool s_hdmi_mask_hpd_irq = false;
module_param(s_hdmi_mask_hpd_irq, bool, 0600);
MODULE_PARM_DESC(s_hdmi_mask_hpd_irq, "hdmi mask hpd irq (default: false)");

static int s_hdmi_hwi2c = false;
module_param(s_hdmi_hwi2c, int, 0600);
MODULE_PARM_DESC(s_hdmi_hwi2c, "hdmi hwi2c mode. (default: false)");

/*
 * 0x1 hw  controller
 * 0x2 hw  i2c
 * 0x4 bit i2c
 */
int s_g3_hdmi_hwi2c = 0x4;
module_param(s_g3_hdmi_hwi2c, int, 0600);
MODULE_PARM_DESC(s_g3_hdmi_hwi2c, "g3 hdmi hwi2c(default: gpio i2c)");

static bool s_hdmi_support_75hz = true;
module_param(s_hdmi_support_75hz, bool, 0600);
MODULE_PARM_DESC(s_hdmi_support_75hz, "hdmi support 75hz (default: true)");

bool s_g3_hdmi_hdcp14 = false;
module_param(s_g3_hdmi_hdcp14, bool, 0600);
MODULE_PARM_DESC(s_g3_hdmi_hdcp14, "hdmi20 hdcp14 (default: false)");

static bool inno_hdmi_encoder_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(encoder, struct hdmi_device_t, encoder);
	const struct drm_display_mode *rmode = NULL;

	if (is_native_mode_valid(&inno_hdmi->native_mode) && \
			is_virtual_mode(&inno_hdmi->native_mode, adjusted_mode)) {
		drm_mode_copy(adjusted_mode, &inno_hdmi->native_mode);
	}

	if (inno_hdmi->chip.replace_timing) {
		rmode = innodpu_modes_match_replace_table(adjusted_mode, NULL, NULL);
		if (!rmode)
			goto out;

		if (!is_special_mode(rmode)) {
			innodpu_modes_replace_timing(adjusted_mode, rmode);
		}

		fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI,"%s fixup: "DRM_MODE_FMT "\n",
			inno_hdmi->name, DRM_MODE_ARG(adjusted_mode));
	}

out:
	return true;
}

static void inno_hdmi_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(encoder, struct hdmi_device_t, encoder);
	int dpu_id = 0;

	if (!encoder || !adjusted_mode)
		return;

	dpu_id = innodpu_get_dpuid_bycrtc(encoder->crtc);

	if (inno_hdmi->chip.encoder_modeset) {
		hdmi_info(inno_hdmi->dev, "%s modeset: "DRM_MODE_FMT "\n",
			inno_hdmi->name, DRM_MODE_ARG(adjusted_mode));
		drm_mode_copy(inno_hdmi->chip.adjusted_mode, adjusted_mode);
		inno_hdmi->chip.encoder_modeset(&inno_hdmi->chip,dpu_id, inno_hdmi->chip.test_mode, adjusted_mode);
	}

	return;
}

static void inno_hdmi_audio_status_update(struct hdmi_device_t *inno_hdmi)
{
	if (!inno_hdmi)
		return;

#ifdef CONFIG_DRM_INNO_AUDIO
	if (inno_hdmi->ac) {
		int dpms, conn_st;
		dpms = inno_hdmi->connector.dpms;
		conn_st = hdmi_get_hpg_status(&inno_hdmi->chip);
		inno_hdmi->ac->has_audio = inno_hdmi->chip.sink_has_audio;
		inno_hdmi->ac->update_eld(inno_hdmi->ac, inno_hdmi->connector.eld, MAX_ELD_BYTES);
		inno_hdmi->ac->report_jack(inno_hdmi->ac, dpms, conn_st);
		hdmi_info(inno_hdmi->dev,
			"try report jack, conn_st:%d, has_audio:%d",
			conn_st, inno_hdmi->ac->has_audio);
	}
#endif
}

static void inno_hdmi_encoder_mode_disable(struct drm_encoder *encoder)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(encoder, struct hdmi_device_t, encoder);

	if (!encoder)
		return;

	inno_hdmi_audio_status_update(inno_hdmi);

	if (encoder->crtc) {
		hdmi_info(inno_hdmi->dev,
			"inno %s encoder mode disable\n",inno_hdmi->name );
	} else {
		hdmi_info(inno_hdmi->dev,
			"inno %s encoder mode disable, crtc is null\n",inno_hdmi->name);
	}

	if (inno_hdmi->chip.encoder_disable)
		inno_hdmi->chip.encoder_disable(&inno_hdmi->chip);

	atomic64_set(&inno_hdmi->modesetting, 0);
	fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI, "modesetting: %d\n",
		atomic64_read(&inno_hdmi->modesetting));
}

static void inno_hdmi_encoder_mode_enable(struct drm_encoder *encoder)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(encoder, struct hdmi_device_t, encoder);

	if (!encoder)
		return;

	if (encoder->crtc) {
		hdmi_info(inno_hdmi->dev,
			"inno %s encoder mode enable\n",inno_hdmi->name );
	} else {
		hdmi_info(inno_hdmi->dev,
			"inno %s encoder mode enable, crtc is null\n",inno_hdmi->name);
	}

	if (inno_hdmi->chip.encoder_enable)
		inno_hdmi->chip.encoder_enable(&inno_hdmi->chip, encoder->crtc);

	atomic64_set(&inno_hdmi->modesetting, 1);
	hdmi_info(inno_hdmi->dev, "modesetting: %d\n",
		atomic64_read(&inno_hdmi->modesetting));
	hdmi_info(inno_hdmi->dev, "enable ok\n");

	inno_hdmi_audio_status_update(inno_hdmi);
}

static int inno_hdmi_encoder_destroy(struct drm_encoder *encoder)
{
	BUG_ON(encoder);

	/*
	 * Called  drm_encoder_cleanup
	 */
	if (encoder->funcs && encoder->funcs->destroy)
		encoder->funcs->destroy(encoder);

	return 0;
}

static const struct drm_encoder_funcs s_inno_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs s_inno_hdmi_encoder_helper_funcs = {
	//.mode_valid = inno_hdmi_encoder_mode_valid,
	.mode_fixup = inno_hdmi_encoder_mode_fixup,
	.mode_set = inno_hdmi_encoder_mode_set,
	.disable = inno_hdmi_encoder_mode_disable,
	.enable = inno_hdmi_encoder_mode_enable,
};

static int inno_hdmi_encoder_create(struct drm_device *drm_dev,
		struct drm_encoder *encoder, unsigned int possible_crtc)
{
	int retcode = 0;

	drm_encoder_helper_add(encoder, &s_inno_hdmi_encoder_helper_funcs);
	retcode = drm_encoder_init(drm_dev, encoder,
		&s_inno_hdmi_encoder_funcs, DRM_MODE_ENCODER_TMDS, NULL);

	encoder->possible_crtcs = possible_crtc;

	return retcode;
}

static void inno_hdmi_connector_destroy_func(struct drm_connector *connector)
{
	if (!connector) {
		return;
	}

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static void inno_hdmi_avmute_for_LCH(struct hdmi_device_t *inno_hdmi)
{
	struct mfc_monitor_info monitor = MFC_MONITOR_INFO_ITEM("LCH", "*");

	if (innodpu_is_mfc_monitor_match((unsigned char *)inno_hdmi->chip.edid_buf, &monitor)) {
		inno_hdmi->chip.flags_avmute |=  (AVMUTE_WAIT_ONEFRAME | AVMUTE_KEEP_GCPPACKET);
	} else {
		inno_hdmi->chip.flags_avmute &= ~(AVMUTE_WAIT_ONEFRAME | AVMUTE_KEEP_GCPPACKET);
	}
}

static int inno_hdmi_get_edid(void *data, u8 * buf, u32 block, size_t len)
{
	struct hdmi_chip_t *chip = (struct hdmi_chip_t *)data;

	fh2m_inno_memcpy(buf, chip->edid_buf + EDID_LENGTH * block, len);

	return 0;
}

static int inno_hdmi_ddc_status_detect(struct drm_connector *connector)
{
	struct hdmi_device_t *inno_hdmi = container_of(connector, struct hdmi_device_t, connector);
	u8 out = 0x0;
	u8 buf[8];
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = DDC_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &out,
		},
		{
			.addr = DDC_ADDR,
			.flags = I2C_M_RD,
			.len = 8,
			.buf = buf,
		}
	};

	ret = i2c_transfer(&inno_hdmi->i2c->adapter, msgs, 2);
	if (ret != 2)
		/* Couldn't find an accessible DDC on this connector */
		return false;

	/* Probe also for valid EDID header
	 * EDID header starts with:
	 * 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00.
	 * Only the first 6 bytes must be valid as
	 * drm_edid_block_valid() can fix the last 2 bytes */
	if (fh2m_inno_drm_edid_header_is_valid(buf) < 6) {
		/* Couldn't find an accessible EDID on this
		 * connector */
		return false;
	}
	return true;
}

static int inno_hdmi_adapter_dongle(struct hdmi_device_t *inno_hdmi, struct edid *edid)
{

	if (!inno_hdmi || !edid) {
		return -1;
	}

	switch (inno_hdmi->connector_type) {
	case DRM_MODE_CONNECTOR_HDMIA:
		/*if it is not hdmi sink for hdmi source, to force set dvi mode.*/
		inno_hdmi->chip.output_mode->mode_connector_item = DRM_MODE_CONNECTOR_HDMIA;
		if (!drm_detect_hdmi_monitor(edid)) {
			inno_hdmi->chip.output_mode->mode_connector_item = DRM_MODE_CONNECTOR_DVII;
		}
		break;

	case DRM_MODE_CONNECTOR_DVII:
		/*if it is hdmi sink for dvi source, to force set hdmi mode.*/
		inno_hdmi->chip.output_mode->mode_connector_item = DRM_MODE_CONNECTOR_DVII;
		if (drm_detect_hdmi_monitor(edid)) {
			inno_hdmi->chip.output_mode->mode_connector_item = DRM_MODE_CONNECTOR_HDMIA;
		}
		break;

	default:
		break;
	}

	return 0;
}

static int inno_hdmi_get_modes_by_autoedid(struct drm_connector *connector)
{
	int nums = 0;
	struct edid *edid = NULL;
	struct hdmi_device_t *inno_hdmi = NULL;

	if (!connector)
		return nums;

	inno_hdmi = container_of(connector, struct hdmi_device_t, connector);

	if (inno_hdmi->chip.hdmi_edid_read) {
		if (inno_hdmi->chip.hdmi_edid_read(&inno_hdmi->chip) > 0)
			edid = drm_do_get_edid(connector, inno_hdmi_get_edid, &inno_hdmi->chip);
		else
			return nums;
	} else if (inno_hdmi->chip.connector_get_edid) {
		edid = inno_hdmi->chip.connector_get_edid(connector, &inno_hdmi->chip);
	}

	if (edid) {
		inno_hdmi->chip.max_pclk_rx = innodpu_conn_get_monitor_max_clk((u8 *)edid);
		if (inno_hdmi->chip.max_pclk_rx <= 0) {
			inno_hdmi->chip.max_pclk_rx = 605000;
			hdmi_info(inno_hdmi->dev,  "get hdmi max clock err, set it to 605MHz\n");
		}

		if (inno_hdmi->chip.replace_timing)
			innodpu_detailed_block_monitor_range_replace((u8 *)edid);

		hdmi_info(inno_hdmi->dev, "update_eld\n");
		nums = drm_add_edid_modes(connector, edid);

#if (DRM_VERSION <= KERNEL_VERSION(4, 16, 0))
		drm_edid_to_eld(connector, edid);
#endif
#if !defined(__G3_NE__)
		inno_hdmi_avmute_for_LCH(inno_hdmi);
		inno_hdmi->chip.sink_is_hdmi = drm_detect_hdmi_monitor(edid);
		inno_hdmi->chip.sink_has_audio = drm_detect_monitor_audio(edid);
		hdmi_info(inno_hdmi->dev, "sink is %s\n",
			inno_hdmi->chip.sink_is_hdmi?"hdmi":"dvi");

		inno_hdmi_adapter_dongle(inno_hdmi, edid);
#endif
		drm_connector_update_edid_property(connector, edid);

		kfree(edid);
	}

	return nums;
}

static int inno_hdmi_get_modes_by_strpush(struct drm_connector *connector)
{
	int ret = -1;
	int nums = 0;
	struct hdmi_device_t *inno_hdmi = NULL;
	unsigned char str_push_edid_buf[INNOHDMI_EDID_BUF_LEN] = {0};

	if (!connector)
		return nums;

	inno_hdmi = container_of(connector, struct hdmi_device_t, connector);

	ret = fh2m_hal_hdmi_edid_data(inno_hdmi->dev->parent, inno_hdmi->hdmi_id,
			str_push_edid_buf);
	if (ret < 0) {
		hdmi_info(inno_hdmi->dev,  \
				"fh2m_hal_hdmi_edid_data failed, return %d.\n", ret);
		return nums;
	}

	nums = innodpu_str_push_edid(str_push_edid_buf, connector);
	drm_mode_sort(&connector->probed_modes);

	return nums;
}

static int inno_hdmi_get_modes_by_userdefine(struct drm_connector *connector)
{
	int ret = -1;
	int nums = 0;
	struct edid *edid = NULL;
	struct hdmi_device_t *inno_hdmi = NULL;

	if (!connector)
		return nums;

	inno_hdmi = container_of(connector, struct hdmi_device_t, connector);

	ret = fh2m_hal_hdmi_edid_data(inno_hdmi->dev->parent, inno_hdmi->hdmi_id,
			inno_hdmi->chip.edid_buf);
	if (ret < 0) {
		hdmi_info(inno_hdmi->dev,  \
				"fh2m_hal_hdmi_edid_data failed, return %d.\n", ret);
		return nums;
	}

	edid = drm_do_get_edid(connector, inno_hdmi_get_edid, &inno_hdmi->chip);
	if (edid) {
		inno_hdmi->chip.max_pclk_rx = innodpu_conn_get_monitor_max_clk((u8 *)edid);
		if (inno_hdmi->chip.max_pclk_rx <= 0) {
			inno_hdmi->chip.max_pclk_rx = 605000;
			fh2m_innodpu_err(inno_hdmi->dev, "get hdmi max clock err, set it to 605MHz\n");
		}
		hdmi_info(inno_hdmi->dev, "update eld\n");
		nums = drm_add_edid_modes(connector, edid);

#if (DRM_VERSION <= KERNEL_VERSION(4, 16, 0))
		drm_edid_to_eld(connector, edid);
#endif

		inno_hdmi_avmute_for_LCH(inno_hdmi);
		inno_hdmi->chip.sink_is_hdmi = drm_detect_hdmi_monitor(edid);
		inno_hdmi->chip.sink_has_audio = drm_detect_monitor_audio(edid);
		hdmi_info(inno_hdmi->dev, "sink is %s\n",
			inno_hdmi->chip.sink_is_hdmi?"hdmi":"dvi");

		inno_hdmi_adapter_dongle(inno_hdmi, edid);

		drm_connector_update_edid_property(connector, edid);

		kfree(edid);
	}

	return nums;
}

static bool inno_hdmi_native_mode_filter(struct drm_connector *connector,
		struct drm_display_mode *mode, bool scaling_filter)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);
#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	struct drm_encoder *encoder = &inno_hdmi->encoder;
#endif
	bool combi_en = true;
	int ret = 0;

	if (!mode)
		return true;

	combi_en = inno_hdmi->chip.combi_en;
	/*
	 * if enable pdp combination, it can be scaled up to 4k
	 * if disable pdp combination, it can be scaled up to 2048x2048
	 */
	if (!combi_en) {
		if ((mode->hdisplay > SCALE_MAX_MODE) || (mode->vdisplay > SCALE_MAX_MODE))
			return true;
	} else {
		if (scaling_filter &&
			((mode->hdisplay > SCALE_MAX_MODE) || (mode->vdisplay > SCALE_MAX_MODE)))
			return true;
	}

	if (mode && inno_hdmi->chip.max_pclk_rx > 0 && \
		mode->clock > inno_hdmi->chip.max_pclk_rx)
		return true;

	if (connector->helper_private && connector->helper_private->mode_valid) {
		ret = connector->helper_private->mode_valid(&inno_hdmi->connector, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}

#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	if (encoder->helper_private && encoder->helper_private->mode_valid) {
		ret = encoder->helper_private->mode_valid(&inno_hdmi->encoder, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}
#endif
	return false;
}

/**
 * inno_hdmi_connector_helper_get_modes - Get hdmi supports mode;
 *
 * Returns:
 * Number of supported modes
 *
 * 1. read edid and get modes
 * 2. push string
 * 3. replace timing
 * 4. get native mode
 * 5. add common modes
 */
static int inno_hdmi_connector_helper_get_modes(
		struct drm_connector *connector)
{
	int nums = 0;
	struct drm_display_mode *native_mode;
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);

	switch (inno_hdmi->chip.hal_edid_mode) {
		case EDID_AUTO_READ:
			nums = inno_hdmi_get_modes_by_autoedid(connector);
			break;
		case EDID_STR_PUSH:
			nums = inno_hdmi_get_modes_by_autoedid(connector);
			if (nums <= 0) {
				hdmi_info(inno_hdmi->dev,  "zoom:can't read autoedid.\n");
			}
			nums += inno_hdmi_get_modes_by_strpush(connector);
			if (nums <= 0) {
				hdmi_info(inno_hdmi->dev,  "zoom:can't parse strpush.\n");
			}
			break;
		case EDID_USER_DEFINE:
			nums = inno_hdmi_get_modes_by_userdefine(connector);
			break;
		default:
			nums = 0;
			fh2m_innodpu_warn(inno_hdmi->dev, "unkown hal_edid_mode(%d)!\n", inno_hdmi->chip.hal_edid_mode);
			break;
	}

	if (!nums) {
		inno_hdmi->chip.modes = innodpu_add_modes_without_edid(connector, NULL);
	} else {
		inno_hdmi->chip.modes = nums;
	}

	// drop repeat mode
	inno_hdmi->chip.modes -= innodpu_modes_drop_repeat(&inno_hdmi->connector);

	//sort modes
	drm_mode_sort(&connector->probed_modes);

	// fixup 1366x768 and more modes for /dev/fb0
	innodpu_modes_fixup_preferred_nonaligned_modes(&inno_hdmi->connector);

	// get native mode
	native_mode = innodpu_get_native_mode(connector, inno_hdmi_native_mode_filter);

	// add common mode
	if (native_mode) {
		hdmi_info(inno_hdmi->dev,  "%s native mode: "DRM_MODE_FMT ", status:%d\n",
			inno_hdmi->name, DRM_MODE_ARG(native_mode), native_mode->status);
		fh2m_inno_memcpy(&inno_hdmi->native_mode, native_mode, sizeof(inno_hdmi->native_mode));
		drm_mode_set_crtcinfo(&inno_hdmi->native_mode, CRTC_INTERLACE_HALVE_V);
		inno_hdmi->native_mode.status = MODE_OK;
		inno_hdmi->chip.modes += innodpu_connector_add_common_modes(connector, &inno_hdmi->native_mode, false);
	} else {
		fh2m_inno_memset(&inno_hdmi->native_mode, 0, sizeof(inno_hdmi->native_mode));
	}

	return inno_hdmi->chip.modes;
}


/**
 * inno_hdmi_connector_detect_ctx - check hdmi hpd status
 *
 * Returns:
 * drm_connector_status. (connector_status_connected or connector_status_disconnected)
 */
static int inno_hdmi_connector_detect_ctx(struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx, bool force)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);
	int conn_status = connector_status_unknown;

#if defined(__G3_NE__)
	return inno_connector_status_connected;
#endif

	if (inno_hdmi->chip.connector_detect) {
		conn_status = inno_hdmi->chip.connector_detect(&inno_hdmi->chip);
	} else {
		fh2m_innodpu_warn(inno_hdmi->dev, "Not support %s chip detect handle !!!\n",
				inno_hdmi->name);
	}

	fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI,
		"%s: %s\n", inno_hdmi->name,
		fh2m_inno_drm_get_connector_status_name(conn_status));

	return conn_status;
}

static enum drm_connector_status inno_hdmi_detect(struct drm_connector *connector, bool force)
{
    return inno_hdmi_connector_detect_ctx(connector, NULL, force);
}

static enum drm_mode_status inno_hdmi_connector_helper_mode_valid(
		struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);
	int status = MODE_OK;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		return MODE_NO_DBLESCAN;
	}

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		return MODE_NO_INTERLACE;
	}

	if (!s_hdmi_support_75hz && drm_mode_vrefresh(mode) == 75) {
		return MODE_NOMODE;
	}

	if (mode->clock > 600000) {
		return MODE_CLOCK_HIGH;
	}

//#define HDMI_MODE_NEED_BYPASS
#ifdef  HDMI_MODE_NEED_BYPASS
	{
		struct mfc_monitor_info monitor = MFC_MONITOR_INFO_ITEM("LEN", "*");
		struct resolution_info  resolution[] = {
			RESOLUTION_INFO_ITEM(1920, 1080, 75, -1),
			RESOLUTION_INFO_ITEM(2560, 1440, 60, -1),
		};
		struct resolution_info  *match_resolution = NULL;

		if (innodpu_is_mfc_monitor_match((unsigned char *)inno_hdmi->chip.edid_buf, &monitor)) {
			match_resolution = innodpu_resolution_match(mode, resolution, INNO_ARRAY_SIZE(resolution));
			if (match_resolution) {
				hdmi_info(inno_hdmi->dev,  \
						"match resolution[%dx%d@%d %dKHz], drop it for monitor(LEN/*)\n",
						match_resolution->hdisplay, match_resolution->vdisplay, match_resolution->vrefresh);
				return MODE_NOMODE;
			}
		}
	}
#endif

	if (inno_hdmi->chip.connector_mode_valid) {
		status = inno_hdmi->chip.connector_mode_valid(&inno_hdmi->chip, mode);
		if (status != MODE_OK)
			return status;
	}

	return MODE_OK;
}

#if ((DRM_VERSION >= KERNEL_VERSION(4, 10, 0)))
static void inno_hdmi_connector_print_state(struct drm_printer *p,
					     const struct drm_connector_state *state)
{

}
#endif

static void inno_hdmi_hotplug_early_handle(struct hdmi_device_t *inno_hdmi)
{
	int hpd_status = 0;

	/* when hpd-pin is high, should to report hpd-in. */
	if (inno_hdmi->chip.hpd_status_detect) {
		hpd_status = inno_hdmi->chip.hpd_status_detect(&inno_hdmi->chip);
		if (hpd_status) {
			hdmi_info(inno_hdmi->dev, "hdmi connected\n");
			hdmi_set_hpg_status(&inno_hdmi->chip, 1);
			drm_helper_hpd_irq_event(inno_hdmi->drm_dev);
		} else {
			hdmi_info(inno_hdmi->dev, "hdmi disconnected\n");
			hdmi_set_hpg_status(&inno_hdmi->chip, 0);
		}
	}
}

static int inno_hdmi_connector_late_register(struct drm_connector *connector)
{
	struct hdmi_device_t * inno_hdmi = container_of(connector, struct hdmi_device_t, connector);
	int ret = 0;
	struct i2c_adapter *adapter = NULL;
	struct device *adapter_dev  = NULL;

	BUG_ON(!(connector->kdev));
#if !defined(__G3_NE__)
	if (inno_hdmi->i2c) {
		adapter = &inno_hdmi->i2c->adapter;
		adapter_dev  = &adapter->dev;

		ret = sysfs_create_link(&connector->kdev->kobj,
				&adapter_dev->kobj,
				adapter_dev->kobj.name);
		if (ret) {
			fh2m_innodpu_err(inno_hdmi->dev, "%s sysfs link create failed-%d\n",
				  adapter_dev->kobj.name, ret);
		}
	}

#if defined(CONFIG_DEBUG_FS)
	ret = inno_hdmi_custom_debugfs_create(connector->debugfs_entry, &inno_hdmi->chip);
	if (ret) {
		fh2m_innodpu_err(inno_hdmi->dev, "%s custom debugfs create failed-%d\n",
			  inno_hdmi->name, ret);
	}
#endif
#endif
	fh2m_hal_dev_enable_irq(inno_hdmi->dev->parent, inno_hdmi->chip.hal_module);
#if !defined(__G3_NE__)
	inno_hdmi_hotplug_early_handle(inno_hdmi);
#endif
	return 0;
}

static void inno_hdmi_connector_early_unregister(struct drm_connector *connector)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);
	struct i2c_adapter *adapter = NULL;
	struct device *adapter_dev  = NULL;

	BUG_ON(!(connector->kdev));

	fh2m_hal_dev_disable_irq(inno_hdmi->dev->parent, inno_hdmi->chip.hal_module);

#if !defined(__G3_NE__)
#if defined(CONFIG_DEBUG_FS)
	inno_hdmi_custom_debugfs_remove(connector->debugfs_entry, &inno_hdmi->chip);
#endif

	if (inno_hdmi->i2c) {
		adapter = &inno_hdmi->i2c->adapter;
		adapter_dev  = &adapter->dev;
		sysfs_remove_link(&connector->kdev->kobj, adapter_dev->kobj.name);
	}
#endif
}

static struct drm_connector_helper_funcs s_inno_hdmi_connector_helper_funcs = {
	.get_modes  = inno_hdmi_connector_helper_get_modes,
	.mode_valid = inno_hdmi_connector_helper_mode_valid,
#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	.detect_ctx = inno_hdmi_connector_detect_ctx,
#endif
};


static const struct drm_connector_funcs s_inno_hdmi_connector_funcs = {
#if ((DRM_VERSION <= KERNEL_VERSION(4, 13, 0)))
	.dpms = drm_atomic_helper_connector_dpms,
#else
	.dpms = drm_helper_connector_dpms,
#endif
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = inno_hdmi_connector_destroy_func,
#if ((DRM_VERSION >= KERNEL_VERSION(4, 10, 0)))
	.atomic_print_state = inno_hdmi_connector_print_state,
#endif
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = inno_hdmi_connector_late_register,
	.early_unregister = inno_hdmi_connector_early_unregister,
	.detect = inno_hdmi_detect,
};

static int inno_hdmi_connector_create(struct drm_device *drm_dev,
				struct drm_connector *connector)
{
	int retcode = 0;
	struct hdmi_device_t * hdmi_dev = container_of(connector, struct hdmi_device_t, connector);

	BUG_ON(!drm_dev);
	BUG_ON(!connector);

	drm_connector_helper_add(connector, &s_inno_hdmi_connector_helper_funcs);

	if (hdmi_dev->chip.output_mode) {
		hdmi_dev->connector_type = hdmi_dev->chip.output_mode->mode_connector_item;
	} else {
		hdmi_dev->connector_type = DRM_MODE_CONNECTOR_HDMIA;
	}

	retcode = drm_connector_init(drm_dev, connector, &s_inno_hdmi_connector_funcs,
			hdmi_dev->connector_type);

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;

	return retcode;
}

static int inno_hdmi_connector_destroy(struct drm_connector *connector)
{
	struct drm_mode_object *conn_obj = &connector->base;

	while (kref_read(&conn_obj->refcount) > 0)
		drm_connector_put(connector); // will be called drm_connector_free

	return 0;
}


static int inno_hdmi_connector_attach_encoder(
			struct hdmi_device_t *inno_hdmi)
{
	int retcode = 0;

	retcode = inno_hdmi_encoder_create(inno_hdmi->drm_dev, &inno_hdmi->encoder,
		inno_hdmi->chip.possible_crtc);
	if (retcode) {
		fh2m_innodpu_err(inno_hdmi->dev, "%s encoder create failed-%d\n",
				  inno_hdmi->name, retcode);
		return retcode;
	}

	retcode = inno_hdmi_connector_create(inno_hdmi->drm_dev, &inno_hdmi->connector);
	if (retcode) {
		fh2m_innodpu_err(inno_hdmi->dev, "%s connector create failed-%d\n",
			inno_hdmi->name, retcode);
		goto err_connector_create;
	}

	// only set connector->possible_encoder,
	retcode = drm_connector_attach_encoder(&inno_hdmi->connector, &inno_hdmi->encoder);
	if (retcode) {
		fh2m_innodpu_err(inno_hdmi->dev, "%s connector attach encoder failed-%d\n",
			inno_hdmi->name, retcode);
		goto error_attach_failed;
	}

	hdmi_info(inno_hdmi->dev,  "%s attach encoder-%d connector-%d\n",
		inno_hdmi->name, inno_hdmi->encoder.base.id, inno_hdmi->connector.base.id);

	return retcode;

error_attach_failed:
	inno_hdmi_connector_destroy(&inno_hdmi->connector);
err_connector_create:
	inno_hdmi_encoder_destroy(&inno_hdmi->encoder);

	return retcode;
}

static void inno_hdmi_cleanup_connector_encoder(
			struct hdmi_device_t *inno_hdmi)
{
	/*
	 * After the hdmi reference count is zeroed, drm_connector_free will be called
	 * encoder autorelease function without reference counting
	 */
	inno_hdmi_connector_destroy(&inno_hdmi->connector);
	inno_hdmi_encoder_destroy(&inno_hdmi->encoder);
}

static int inno_hdmi_edid_fixup(struct drm_connector *connector)
{
	int ret = -1;
	struct edid *edid = NULL;
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);

	inno_hdmi = container_of(connector, struct hdmi_device_t, connector);
	switch (inno_hdmi->chip.hal_edid_mode) {
	case EDID_AUTO_READ:
		if (inno_hdmi->chip.hdmi_edid_read) {
			if (inno_hdmi->chip.hdmi_edid_read(&inno_hdmi->chip) > 0)
				edid = drm_do_get_edid(connector, inno_hdmi_get_edid, &inno_hdmi->chip);
			else
				return -1;
		} else if (inno_hdmi->chip.connector_get_edid) {
			edid = inno_hdmi->chip.connector_get_edid(connector, &inno_hdmi->chip);
		}
		break;
	case EDID_USER_DEFINE:
		ret = fh2m_hal_hdmi_edid_data(inno_hdmi->dev->parent, inno_hdmi->hdmi_id,
				inno_hdmi->chip.edid_buf);
		if (ret < 0) {
			hdmi_info(inno_hdmi->dev,  \
					"fh2m_hal_hdmi_edid_data failed, return %d.\n", ret);
			return -1;
		}
		edid = drm_do_get_edid(connector, inno_hdmi_get_edid, &inno_hdmi->chip);
		break;
	default:
		return -1;
	}

	if (edid) {
#if (DRM_VERSION <= KERNEL_VERSION(4, 16, 0))
		drm_edid_to_eld(connector, edid);
#endif

		inno_hdmi_avmute_for_LCH(inno_hdmi);
		inno_hdmi->chip.sink_is_hdmi = drm_detect_hdmi_monitor(edid);
		inno_hdmi->chip.sink_has_audio = drm_detect_monitor_audio(edid);
		hdmi_info(inno_hdmi->dev, "sink is %s\n",
			inno_hdmi->chip.sink_is_hdmi?"hdmi":"dvi");

		inno_hdmi_adapter_dongle(inno_hdmi, edid);

		drm_connector_update_edid_property(connector, edid);

		kfree(edid);
		return 0;
	}

	return -1;
}

static void inno_hdmi_hotplug_resume_handle(struct hdmi_device_t *inno_hdmi)
{
	int hpd_status = 0;

	/* when hpd-pin is high/low, should to report hpd-in/hpd-out. */
	if (inno_hdmi->chip.hpd_status_detect) {
		hpd_status = inno_hdmi->chip.hpd_status_detect(&inno_hdmi->chip);
		if (hpd_status) {
			hdmi_info(inno_hdmi->dev, "hdmi connected\n");
			hdmi_set_hpg_status(&inno_hdmi->chip, 1);
			inno_hdmi_edid_fixup(&inno_hdmi->connector);
		} else {
			hdmi_info(inno_hdmi->dev, "hdmi disconnected\n");
			hdmi_set_hpg_status(&inno_hdmi->chip, 0);
		}
		drm_helper_hpd_irq_event(inno_hdmi->drm_dev);
	}
}

static void inno_hdmi_start_recover(struct drm_connector *connector)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);
	struct drm_encoder *encoder = &inno_hdmi->encoder;
	int clock = 0;

	fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI, "modesetting: %d\n",
		atomic64_read(&inno_hdmi->modesetting));
	if (atomic64_read(&inno_hdmi->modesetting)) {
		struct drm_display_mode *mode = NULL;

		mode = inno_hdmi->chip.adjusted_mode;
		if (IS_ERR_OR_NULL(mode)) {
			hdmi_info(inno_hdmi->dev,  "recover-mode is NULL\n");
			return;
		}

		clock = fh2m_inno_drm_disp_get_member(clock, mode);
		if (clock <= 0) {
			hdmi_info(inno_hdmi->dev,  "recover-mode clock is zero\n");
			return;
		}

		hdmi_info(inno_hdmi->dev,  "%s start recover-%dx%d@%d\n",
			inno_hdmi->name, mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode));

		inno_hdmi_edid_fixup(connector);
		inno_hdmi_encoder_mode_set(encoder, NULL, mode);
		inno_hdmi_encoder_mode_enable(encoder);
	}
}

static int inno_hdmi_hpdout_clean(struct drm_connector *connector)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(connector, struct hdmi_device_t, connector);

	fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI, "modesetting: %d\n",
		atomic64_read(&inno_hdmi->modesetting));

	hdmi_set_hpg_status(&inno_hdmi->chip, 0);

	inno_hdmi->chip.sink_is_hdmi   = false;
	inno_hdmi->chip.sink_has_audio = false;
	inno_hdmi->chip.total_block = 0;
	hdmi_info(inno_hdmi->dev,  "edid clear ok\n");

	inno_hdmi_audio_status_update(inno_hdmi);

	drm_connector_update_edid_property(connector, NULL);
	fh2m_inno_memset(inno_hdmi->chip.edid_buf, 0, INNOHDMI_EDID_BUF_LEN);

	drm_connector_update_edid_property(connector, NULL);

	inno_hdmi->chip.max_pclk_rx = 0;

	return 0;
}

/*
 * If the ddc detection is more than N times, the HPD signal is not elevated,
 * which means that the display should be pulled out.
 *
 */
#define INNOHDMI_HPDOUT_THRESHOLD (5U)
static void __attribute__ ((unused)) inno_hdmi_hotplug_work(struct work_struct *work)
{
	struct hdmi_device_t *inno_hdmi =
		container_of(work, struct hdmi_device_t, hotplug_work.work);

	int hpd_status, ddc_status;

	/* when hpd-pin is high, should to report hpd-in. */
	if (inno_hdmi->chip.hpd_status_detect) {
		hpd_status = inno_hdmi->chip.hpd_status_detect(&inno_hdmi->chip);
		if (hpd_status) {
			inno_hdmi->hpdout_cnt = 0;
			hdmi_set_hpg_status(&inno_hdmi->chip, 1);
			inno_hdmi_start_recover(&inno_hdmi->connector);
			drm_helper_hpd_irq_event(inno_hdmi->drm_dev);
			return;
		}
	}

	/* when hpd low happend, should to read ddc status,
	 * when ddc is okay, to ignore it.
	 * because it could be connected, the user to close hdmi signel out.
	 * when ddc is failed, to report hpd-out.
	 * */
	if (!is_flags_skip_ddc_detect(inno_hdmi->chip.output_mode)) {
		ddc_status = inno_hdmi_ddc_status_detect(&inno_hdmi->connector);
		hdmi_info(inno_hdmi->dev,  \
				"loop ddc_detect:%s, hpdout_cnt:%d.\n",\
				ddc_status ? "okay" : "failed", inno_hdmi->hpdout_cnt);
		if (ddc_status && inno_hdmi->hpdout_cnt < INNOHDMI_HPDOUT_THRESHOLD) {
			inno_hdmi->hpdout_cnt ++;
			cancel_delayed_work(&inno_hdmi->hotplug_work);
			queue_delayed_work(inno_hdmi->hpdwq, &inno_hdmi->hotplug_work, \
					msecs_to_jiffies(1000));
			return;
		} else {
			inno_hdmi->hpdout_cnt = 0;
			inno_hdmi_hpdout_clean(&inno_hdmi->connector);
			drm_helper_hpd_irq_event(inno_hdmi->drm_dev);
			return;
		}
	} else {
		//force to report hpd-out
		inno_hdmi_hpdout_clean(&inno_hdmi->connector);
		drm_helper_hpd_irq_event(inno_hdmi->drm_dev);
		return;
	}
}

static void inno_hdmi_irq_handle(void *data)
{
	unsigned int irq_event = 0;
	struct hdmi_device_t *inno_hdmi = (struct hdmi_device_t *)data;
	unsigned int delay_ms   = 200;
	int hpd_status = 0;

	BUG_ON(!inno_hdmi);

	if (inno_hdmi->chip.irq_handle) {
		irq_event = inno_hdmi->chip.irq_handle(&inno_hdmi->chip);
	} else {
		fh2m_innodpu_warn(inno_hdmi->dev, "Not support %s chip irq handle !!!\n",
				  inno_hdmi->name);
		return;
	}

#if !defined(__G3_NE__)
	if (irq_event & HDMI_IRQ_HPD_MASK) {
		hpd_status = inno_hdmi->chip.hpd_status_detect(&inno_hdmi->chip);

		if (!hpd_status) {
			/*
			 * when hpd is low, we need wait longer to skip abormal hpd signel.
			 * and if not specified, the default is 500ms.
			 * */
			if (is_flags_wait_longer_hpd(inno_hdmi->chip.output_mode)) {
				delay_ms = inno_hdmi->chip.output_mode->data;
			} else {
				delay_ms = 500;
			}
		} else {
			/*
			 * when hpd is high,
			 * longer delay to wait dvi sink ready, or will fail to read edid.
			 */
			if (inno_hdmi->chip.output_mode && is_output_type_dvi(inno_hdmi->chip.output_mode)) {
				delay_ms = 500;
			}
		}

		cancel_delayed_work(&inno_hdmi->hotplug_work);
		fh2m_innodpu_info(inno_hdmi->dev, DPU_UT_HDMI, "hotplug workqueue delay %d ms excute\n", delay_ms);
		queue_delayed_work(inno_hdmi->hpdwq, &inno_hdmi->hotplug_work, msecs_to_jiffies(delay_ms));
	}
#endif
	return;
}

static void __maybe_unused inno_hdmi_hw_irq_enable(struct hdmi_device_t *inno_hdmi, unsigned int flags)
{
	BUG_ON(!inno_hdmi);
	if (inno_hdmi->chip.irq_enable)
		return inno_hdmi->chip.irq_enable(&inno_hdmi->chip, flags);
}

static void __maybe_unused inno_hdmi_hw_irq_disable(struct hdmi_device_t *inno_hdmi, unsigned int flags)
{
	BUG_ON(!inno_hdmi);
	if (inno_hdmi->chip.irq_disable)
		return inno_hdmi->chip.irq_disable(&inno_hdmi->chip, flags);
}

static int innohdmi_get_clock(void *data)
{
	struct hdmi_device_i2c_t *i2c = (struct hdmi_device_i2c_t *)data;
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->getscl) {
		return chip->chipi2c->getscl(chip);
	}

	return -1;
}

static int innohdmi_get_data(void *data)
{
	struct hdmi_device_i2c_t *i2c = (struct hdmi_device_i2c_t *)data;
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->getsda) {
		return chip->chipi2c->getsda(chip);
	}

	return -1;
}

static void innohdmi_set_clock(void *data, int state)
{
	struct hdmi_device_i2c_t *i2c = (struct hdmi_device_i2c_t *)data;
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->setscl) {
		chip->chipi2c->setscl(chip, state);
	}

	return;
}

static void innohdmi_set_data(void *data, int state)
{
	struct hdmi_device_i2c_t *i2c = (struct hdmi_device_i2c_t *)data;
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->setsda) {
		chip->chipi2c->setsda(chip, state);
	}

	return;
}


#define I2C_RISEFALL_TIME 20
static int innohdmi_pre_xfer(struct i2c_adapter *adapter)
{
	int ret;
	struct hdmi_device_i2c_t *i2c = container_of(adapter,
					       struct hdmi_device_i2c_t,
					       adapter);
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->pre_xfer) {
		ret = chip->chipi2c->pre_xfer(chip);
		if (ret)
			return ret;
	}

	innohdmi_set_data(i2c, 1);
	innohdmi_set_clock(i2c, 1);

	fh2m_inno_udelay(I2C_RISEFALL_TIME);
	return 0;
}

static void innohdmi_post_xfer(struct i2c_adapter *adapter)
{
	struct hdmi_device_i2c_t *i2c = container_of(adapter,
					       struct hdmi_device_i2c_t,
					       adapter);
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;

	if (chip && chip->chipi2c && chip->chipi2c->post_xfer) {
		chip->chipi2c->post_xfer(chip);
	}

	innohdmi_set_data(i2c, 1);
	innohdmi_set_clock(i2c, 1);

	fh2m_inno_udelay(I2C_RISEFALL_TIME);
}

static int innohdmi_i2c_bit_xfer(struct i2c_adapter *adapter,
		struct i2c_msg *msgs, int num)
{
	extern const struct i2c_algorithm i2c_bit_algo;
	return i2c_bit_algo.master_xfer(adapter, msgs, num);
}

#define I2C_M_RD 0x0001
static int innohdmi_i2c_hw_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int num)
{
	struct hdmi_device_i2c_t *i2c = container_of(adapter,
					       struct hdmi_device_i2c_t,
					       adapter);
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&i2c->hdmi_dev->chip;
	int i, ret = -1;
	u16 addr, len;
	u8 *buf;

	if (chip && chip->chipi2c && chip->chipi2c->set_ddc_clk) {
		chip->chipi2c->set_ddc_clk(chip);
	}

	if (!chip || !chip->dev)
		return ret;

	for (i = 0; i < num; i++) {
		addr = (msgs[i].flags & I2C_M_RD) ? ((msgs[i].addr<<1) | 0x1) : (msgs[i].addr<<1);
		buf = msgs[i].buf;
		len = msgs[i].len;

		if (chip && chip->chipi2c && chip->chipi2c->hwi2c_transfer) {
			ret = chip->chipi2c->hwi2c_transfer(chip, addr, buf, len);
			if (ret < 0)
				break;
		}
	}

	if (!ret) {
		ret = num;
	}

	return ret;
}

static int innohdmi_i2c_xfer(struct i2c_adapter *adapter,
		struct i2c_msg *msgs, int num)
{
	int ret = -1;
	struct hdmi_device_i2c_t *i2c = container_of(adapter,
					       struct hdmi_device_i2c_t,
					       adapter);
	struct hdmi_device_t *hdmi_dev = i2c->hdmi_dev;
	struct hdmi_chip_t   *chip = (struct hdmi_chip_t *)&hdmi_dev->chip;
	int (*xfer)(struct i2c_adapter *, struct i2c_msg *, int) = NULL;
	chip_type_e plat;

	plat = fh2m_hal_get_chiptype(hdmi_dev->dev->parent);

	if (s_hdmi_hwi2c && (plat == CHIP_G0M_SOC)) {
		xfer = innohdmi_i2c_hw_xfer;
	} else {
		xfer = innohdmi_i2c_bit_xfer;
	}

	fh2m_inno_mutex_lock(chip->chipi2c->mutex);
	ret = xfer(adapter, msgs, num);
	fh2m_inno_mutex_unlock(chip->chipi2c->mutex);

	return ret;
}

static u32 innohdmi_i2c_functionality(struct i2c_adapter *adapter)
{
	/*
	struct hdmi_device_i2c_t *i2c = container_of(adapter,
					       struct hdmi_device_i2c_t,
					       adapter);

	return i2c->algorithm->functionality(adap, msgs, num);
	*/

	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm innohdmi_algorithm = {
	.master_xfer = innohdmi_i2c_xfer,
	.functionality = innohdmi_i2c_functionality,
};

static int innohdmi_i2c_init(struct hdmi_device_t *hdmi_dev)
{
	struct hdmi_device_i2c_t *i2c = NULL;
	struct i2c_adapter *adapter = NULL;
	struct i2c_algo_bit_data *bit_algo = NULL;

	i2c = kzalloc(sizeof(struct hdmi_device_i2c_t), fh2m_hal_get_inno_gfp_kernel());
	if (!i2c)
		return -ENOMEM;

	adapter = &i2c->adapter;
	bit_algo = &i2c->bit_algo;

	snprintf(adapter->name, sizeof(adapter->name), \
			"hdmi%d-i2c", hdmi_dev->hdmi_id);
	adapter->algo = &innohdmi_algorithm;
#if (DRM_VERSION < KERNEL_VERSION(6, 8, 0))
	adapter->class = I2C_CLASS_DDC;
#endif
	adapter->dev.parent = hdmi_dev->dev;
	adapter->nr = -1;
	adapter->owner = THIS_MODULE;

	bit_algo->getscl = innohdmi_get_clock;
	bit_algo->getsda = innohdmi_get_data;
	bit_algo->setscl = innohdmi_set_clock;
	bit_algo->setsda = innohdmi_set_data;
	bit_algo->pre_xfer = innohdmi_pre_xfer;
	bit_algo->post_xfer = innohdmi_post_xfer;
	bit_algo->udelay = I2C_RISEFALL_TIME;
	bit_algo->timeout = usecs_to_jiffies(2200);
	bit_algo->data = i2c;
	//bit_algo->can_do_atomic = true;
	adapter->retries = 3;
	adapter->algo_data = bit_algo;

	if (i2c_add_adapter(adapter)) {
		goto err;
	}

	//i2c->hw_ddc_ops  = ddc_ops;
	i2c->hdmi_dev = hdmi_dev;
	hdmi_dev->i2c = i2c;

	return 0;

err:
	kfree(i2c);
	return -EINVAL;
}


static int inno_hdmi_hw_init(struct hdmi_device_t *inno_hdmi)
{
	int retcode = 0;

	if (inno_hdmi->chip.hw_init)
		return inno_hdmi->chip.hw_init(&inno_hdmi->chip);

	return retcode;
}

static void inno_hdmi_hw_fini(struct hdmi_device_t *inno_hdmi)
{
	if (inno_hdmi->chip.hw_fini)
		inno_hdmi->chip.hw_fini(&inno_hdmi->chip);
}

static int inno_hdmi_chip_init(struct hdmi_device_t *inno_hdmi)
{
	chip_type_e plat;
	int retcode = 0;

	inno_hdmi->chip.hal_edid_mode = fh2m_hal_hdmi_edid_mode(inno_hdmi->dev->parent, inno_hdmi->hdmi_id);
	if (inno_hdmi->chip.hal_edid_mode < 0) {
		hdmi_info(inno_hdmi->dev, "hdmi-%d fh2m_hal_hdmi_edid_mode failed, return %d\n",
			inno_hdmi->hdmi_id, inno_hdmi->chip.hal_edid_mode);
		inno_hdmi->chip.hal_edid_mode = EDID_AUTO_READ;
	}

	hdmi_info(inno_hdmi->dev, "hdmi-%d use edid mode:%d\n",
			inno_hdmi->hdmi_id, inno_hdmi->chip.hal_edid_mode);

	inno_hdmi->chip.drm_dev = (void *)inno_hdmi->drm_dev;

	if (hdmi_ext_init(&inno_hdmi->chip))
		fh2m_innodpu_warn(inno_hdmi->dev, "hdmi ext init error\n");

	plat = fh2m_hal_get_chiptype(inno_hdmi->dev->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		hdmi_info(inno_hdmi->dev, "%s start init g1 soc.\n", inno_hdmi->name);
		retcode = g1_soc_hdmi_chip_init(&inno_hdmi->chip,
				              inno_hdmi->dev, inno_hdmi->hdmi_id);
		break;
	case CHIP_G0_SOC:
		hdmi_info(inno_hdmi->dev, "%s start init g0 soc.\n", inno_hdmi->name);
		retcode = g0_soc_hdmi_chip_init(&inno_hdmi->chip,
				              inno_hdmi->dev, inno_hdmi->hdmi_id);
		break;
	case CHIP_G1P_SOC:
		hdmi_info(inno_hdmi->dev, "%s start init g1p soc.\n", inno_hdmi->name);
		retcode = g1p_soc_hdmi_chip_init(&inno_hdmi->chip,
				              inno_hdmi->dev, inno_hdmi->hdmi_id);
		break;
	case CHIP_G0M_SOC:
		hdmi_info(inno_hdmi->dev, "%s start init g0m soc.\n", inno_hdmi->name);
		retcode = g0m_soc_hdmi_chip_init(&inno_hdmi->chip,
				              inno_hdmi->dev, inno_hdmi->hdmi_id);
		break;
	case CHIP_G3_NE:
		hdmi_info(inno_hdmi->dev, "%s start init g3 ne.\n", inno_hdmi->name);
		if (inno_hdmi->hdmi_id == 0)
			retcode = g3_ne_hdmi_chip_init(&inno_hdmi->chip,
				              inno_hdmi->dev, inno_hdmi->hdmi_id);
		break;
	default:
		fh2m_innodpu_err(inno_hdmi->dev, "%s does not currently support %d platform.\n",
			inno_hdmi->name, plat);
		retcode = -EINVAL;
		break;
	}
#if !defined(__G3_NE__)
	innohdmi_i2c_init(inno_hdmi);

	inno_hdmi->chip.output_mode = innodpu_get_connector_output_mode(inno_hdmi->dev, inno_hdmi->chip.reg_module);
	if (inno_hdmi->chip.output_mode) {
		if (is_output_type_dvi(inno_hdmi->chip.output_mode) ||
			is_output_type_vga(inno_hdmi->chip.output_mode)  ||
			is_output_type_lvds(inno_hdmi->chip.output_mode)) {
			inno_hdmi->chip.max_width = 2048;
			inno_hdmi->chip.max_height = 2048;
		}
		hdmi_info(inno_hdmi->dev, "mode_connector:%d convert:%s",
			inno_hdmi->chip.output_mode->mode_connector_item, inno_hdmi->chip.output_mode->convert_name);
	}

	{
		struct hw_board_info board[] = {
			HW_BOARD_INFO_ITEM("ZY", "YF27_1"), //VGA+HDMI2VGA+DP2LVDS
			HW_BOARD_INFO_ITEM("ZY", "YF27_2"), //VGA+HDMI2VGA+DP2VGA
		};
		if (innodpu_odm_pcb_match(inno_hdmi->dev->parent, board, INNO_ARRAY_SIZE(board))) {
			inno_hdmi->chip.max_width = 1920;
			inno_hdmi->chip.max_height = 1080;
		}
	}
#endif

	return retcode;
}

static void inno_hdmi_chip_fini(struct hdmi_device_t *inno_hdmi)
{
	chip_type_e plat;

	plat = fh2m_hal_get_chiptype(inno_hdmi->dev->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		g1_soc_hdmi_chip_fini(&inno_hdmi->chip);
		break;
	case CHIP_G0_SOC:
		g0_soc_hdmi_chip_fini(&inno_hdmi->chip);
		break;
	case CHIP_G1P_SOC:
		g1p_soc_hdmi_chip_fini(&inno_hdmi->chip);
		break;
	case CHIP_G0M_SOC:
		g0m_soc_hdmi_chip_fini(&inno_hdmi->chip);
		break;
	case CHIP_G3_NE:
		if (inno_hdmi->hdmi_id == 0)
			g3_ne_hdmi_chip_fini(&inno_hdmi->chip);
		break;
	default:
		fh2m_innodpu_err(inno_hdmi->dev, "%s does not currently support %d platform.\n",
			inno_hdmi->name, plat);
		break;
	}

	hdmi_ext_fini(&inno_hdmi->chip);
}

/**
 * inno_hdmi_bind - innosilicon hdmi-driver initialization function
 * @dev: hdmi_device_t_info allocated when inno_hdmi_device_t_register
 * parent(dev->parent) is &pci_dev.dev
 * @master: component master
 * @data: point of struct drm_device
 *
 * This function initializes the innosilicon hdmi device
 * 1. alloc and init hdmi handle
 * 2. hdmi hardware init
 * 3. hdmi ddcci supports
 * 4. creation and bonding of HDMI connectors and encoders
 * 5. setup hdmi irq
 *
 * Returns:
 * 0 if it is OK, errno otherwise.
 */

#ifdef CONFIG_DRM_INNO_AUDIO
static void inno_hdmi_audio_enable(struct audio_conn *ac)
{
	struct hdmi_device_t *inno_hdmi = NULL;

	if (!ac || !ac->priv) {
		return ;
	}

	inno_hdmi = (struct hdmi_device_t *)ac->priv;
	if (inno_hdmi && inno_hdmi->chip.hdmi_enable_audio) {
		inno_hdmi->chip.hdmi_enable_audio(&inno_hdmi->chip);
	}
}

static void inno_hdmi_audio_disable(struct audio_conn *ac)
{
	struct hdmi_device_t *inno_hdmi = NULL;

	if (!ac || !ac->priv) {
		return ;
	}

	inno_hdmi = (struct hdmi_device_t *)ac->priv;
	if (inno_hdmi && inno_hdmi->chip.hdmi_disable_audio) {
		inno_hdmi->chip.hdmi_disable_audio(&inno_hdmi->chip);
	}
}
#endif

static int inno_hdmi_bind(struct device *dev,
					  struct device *master, void *data)
{
	int retcode = 0;
	struct drm_device * drm_dev = data;
	struct hdmi_device_t *inno_hdmi = NULL;
	plat_data_t *pdata =  dev_get_platdata(dev);
#ifdef CONFIG_DRM_INNO_AUDIO
	struct audio_conn *pac;
#endif
	struct drm_display_mode *inno_mode = NULL;

	hdmi_info(dev, "start\n");
	BUG_ON(!dev);
	BUG_ON(!data);

#if !defined(__G3_NE__)
	if (!(s_hdmi_nums & (1<<pdata->dev_idx)))
		return 0;
#endif

#if !defined(__G3_NE__)
	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_HDMI0 + pdata->dev_idx)) {
		hdmi_info(dev,  "possible_crtc = 0, do not bind hdmi-%d\n", pdata->dev_idx);
		return retcode;
	}
#endif
	inno_hdmi = devm_kmalloc(dev, sizeof(*inno_hdmi), fh2m_hal_get_inno_gfp_kernel());
	if (!inno_hdmi) {
		fh2m_innodpu_err(dev, "Alloc hdmi-%d handle failed. Short of memory.\n",
			pdata->dev_idx);
		return -ENOMEM;
	}

	memset(inno_hdmi, 0, sizeof(*inno_hdmi));
	inno_hdmi->hdmi_id = pdata->dev_idx;
	inno_hdmi->dev = fh2m_inno_get_device(dev);
	inno_hdmi->drm_dev = drm_dev;
	atomic64_set(&inno_hdmi->modesetting, 0);

	inno_hdmi->name = fh2m_inno_kasprintf(fh2m_hal_get_inno_gfp_kernel(), "inno-hdmi-%d", inno_hdmi->hdmi_id);
	if (!inno_hdmi->name) {
		fh2m_innodpu_err(dev, "Alloc hdmi-%d name failed. Short of memory.\n",
			pdata->dev_idx);
		retcode = -ENOMEM;
		goto err_out_name;
	}

	dev_set_drvdata(dev, inno_hdmi);
	hdmi_info(dev,  "%s start bind.\n", inno_hdmi->name);

	retcode = inno_hdmi_chip_init(inno_hdmi);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s chip Init failed-%d.\n", inno_hdmi->name, retcode);
		goto err_chip_init;
	}

	retcode = inno_hdmi_hw_init(inno_hdmi);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s hw Init failed-%d.\n", inno_hdmi->name, retcode);
		goto err_hw_init;
	}

	inno_mode = devm_kmalloc(dev, sizeof(*inno_mode), fh2m_hal_get_inno_gfp_kernel());
	if (!inno_mode) {
		fh2m_innodpu_err(dev, "Alloc hdmi-%d mode failed. Short of memory.\n",
			pdata->dev_idx);
		return -ENOMEM;
	}
	memset(inno_mode, 0, sizeof(*inno_mode));
	inno_hdmi->chip.adjusted_mode = inno_mode;

	retcode = inno_hdmi_connector_attach_encoder(inno_hdmi);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s connector and encoder Init failed-%d.\n",
			inno_hdmi->name, retcode);
		goto err_attach_encoder;
	}

	retcode = fh2m_hal_set_irq_handler(dev->parent,
		inno_hdmi->chip.hal_module, inno_hdmi_irq_handle, inno_hdmi);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s irq Init failed-%d.\n", inno_hdmi->name, retcode);
		goto err_enable_irq;
	}

	inno_hdmi->hpdwq = create_singlethread_workqueue("hotplug");
	INIT_DELAYED_WORK(&inno_hdmi->hotplug_work, inno_hdmi_hotplug_work);

#ifdef CONFIG_DRM_INNO_AUDIO
	if (inno_hdmi->chip.output_mode &&
		(is_output_type_hdmi(inno_hdmi->chip.output_mode) || \
				is_output_type_dp(inno_hdmi->chip.output_mode))) {
		pac = kzalloc(sizeof(struct audio_conn), fh2m_hal_get_inno_gfp_kernel());
		if (!pac) {
			fh2m_innodpu_err(dev, "Alloc audio_conn failed.\n");
			goto err_enable_irq;
		}
		pac->conn_st = 0; //todo
		pac->has_audio = 0; //todo
		pac->dev = (void *)inno_hdmi->dev;
		pac->id = inno_hdmi->hdmi_id;
		if(is_output_type_dp(inno_hdmi->chip.output_mode)){
			pac->type = INNOAUDIO_CONNECTOR_TYPE_HDMI2DP;
		}else{
			pac->type = INNOAUDIO_CONNECTOR_TYPE_HDMI;
		}
		pac->priv = (void *)inno_hdmi;
		pac->enable  = inno_hdmi_audio_enable;
		pac->disable = inno_hdmi_audio_disable;

		if (fh2m_innoaudio_register_connector(pac)) {
			hdmi_info(dev,  "register audio_conn failed.\n");
			inno_hdmi->ac = NULL;
			kfree(pac);
		} else {
			hdmi_info(dev,  "register audio_conn ok.\n");
			inno_hdmi->ac = pac;
		}
	}
#endif

	hdmi_info(dev, "end\n");
	return retcode;

err_enable_irq:
	inno_hdmi_cleanup_connector_encoder(inno_hdmi);
err_attach_encoder:
	inno_hdmi_hw_fini(inno_hdmi);
err_hw_init:
	inno_hdmi_chip_fini(inno_hdmi);
err_chip_init:
	if (inno_hdmi->name)
		kfree(inno_hdmi->name);
err_out_name:
	put_device(dev);
	devm_kfree(dev, inno_hdmi);

	return retcode;
}

/**
* inno_hdmi_unbind - innosilicon hdmi-driver initialization function
* @dev: hdmi_device_t_info allocated when inno_hdmi_device_t_register
* parent(dev->parent) is &pci_dev.dev
* @master: component master
* @data: point of struct drm_device
*
* This function deinitializes the innosilicon hdmi device
*
* Once drm_dev_register is called, the connector and encoder
* resources are released by the drm_mode_config_cleanup function!!!
* So we don't need to worry about the release of the connector and encoder here.
*/
static void inno_hdmi_unbind(struct device *dev,
						  struct device *master, void *data)

{
	struct hdmi_device_t *inno_hdmi = NULL;
	plat_data_t *pdata =  dev_get_platdata(dev);

	hdmi_info(dev, "start\n");
	BUG_ON(!dev);
	BUG_ON(!data);

#if !defined(__G3_NE__)
	if (!(s_hdmi_nums & (1<<pdata->dev_idx)))
		return;
#endif

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_HDMI0 + pdata->dev_idx)) {
		hdmi_info(dev,  "possible_crtc = 0, do not unbind hdmi-%d\n", pdata->dev_idx);
		return;
	}

	inno_hdmi = dev_get_drvdata(dev);
	if (!inno_hdmi) {
		fh2m_innodpu_err(dev, "hdmi handle is NULL\n");
		return;
	}

	fh2m_hal_set_irq_handler(dev->parent,
		inno_hdmi->chip.hal_module, NULL, NULL);

#ifdef CONFIG_DRM_INNO_AUDIO
	if (inno_hdmi->ac) {
		fh2m_innoaudio_unregister_connector(inno_hdmi->ac);
		kfree(inno_hdmi->ac);
		inno_hdmi->ac = NULL;
	}
#endif

	cancel_delayed_work_sync(&inno_hdmi->hotplug_work);

	inno_hdmi_hw_fini(inno_hdmi);
	inno_hdmi_chip_fini(inno_hdmi);
	kfree(inno_hdmi->name);

	put_device(dev);
	devm_kfree(dev, inno_hdmi);
	hdmi_info(dev, "end\n");
}


static const struct component_ops s_inno_hdmi_ops = {
	.bind = inno_hdmi_bind,
	.unbind = inno_hdmi_unbind,
};

static int inno_hdmi_probe(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	return component_add(&pdev->dev, &s_inno_hdmi_ops);
}

static int inno_hdmi_remove(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	component_del(&pdev->dev, &s_inno_hdmi_ops);
	return 0;
}

static struct platform_device_id s_inno_hdmi_device_id_table[] = {
	{.name = INNO_HDMI_DEVICE_NAME, .driver_data = 0},
	{},
};
MODULE_DEVICE_TABLE(platform, s_inno_hdmi_device_id_table);

static int inno_hdmi_suspend(struct device *dev)
{
	struct hdmi_device_t *inno_hdmi = NULL;
	plat_data_t *pdata =  dev_get_platdata(dev);

	hdmi_info(dev, "start\n");
	BUG_ON(!dev);

#if !defined(__G3_NE__)
	if (!(s_hdmi_nums & (1<<pdata->dev_idx)))
		return 0;
#endif

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_HDMI0 + pdata->dev_idx)) {
		hdmi_info(dev,  "possible_crtc = 0, do not suspend hdmi-%d\n", pdata->dev_idx);
		return 0;
	}

	inno_hdmi = dev_get_drvdata(dev);
	if (!inno_hdmi) {
		fh2m_innodpu_err(dev, "hdmi handle is NULL\n");
		return 0;
	}

	fh2m_hal_dev_disable_irq(dev->parent, inno_hdmi->chip.hal_module);

	cancel_delayed_work_sync(&inno_hdmi->hotplug_work);
	inno_hdmi_hpdout_clean(&inno_hdmi->connector);

	hdmi_info(dev, "end\n");
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int inno_hdmi_resume(struct device *dev)
{
	struct hdmi_device_t *inno_hdmi = NULL;
	plat_data_t *pdata =  dev_get_platdata(dev);

	hdmi_info(dev, "start\n");
	BUG_ON(!dev);

#if !defined(__G3_NE__)
	if (!(s_hdmi_nums & (1<<pdata->dev_idx)))
		return 0;
#endif

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_HDMI0 + pdata->dev_idx)) {
		hdmi_info(dev,  "possible_crtc = 0, do not resume hdmi-%d\n", pdata->dev_idx);
		return 0;
	}

	inno_hdmi = dev_get_drvdata(dev);
	if (!inno_hdmi) {
		fh2m_innodpu_err(dev, "hdmi handle is NULL\n");
		return 0;
	}

	inno_hdmi_hw_init(inno_hdmi);

	fh2m_hal_dev_enable_irq(dev->parent, inno_hdmi->chip.hal_module);

	inno_hdmi_hotplug_resume_handle(inno_hdmi);
	hdmi_info(dev, "end\n");

	return 0;
}
#endif

static void inno_hdmi_shutdown(struct platform_device *pdev)
{
	struct hdmi_device_t *inno_hdmi = NULL;
	plat_data_t *pdata =  dev_get_platdata(&pdev->dev);

	if (!innodpu_detect_is_valid_output(&pdev->dev, CONNECTOR_M_HDMI0 + pdata->dev_idx)) {
		hdmi_info(&pdev->dev, "possible_crtc = 0, do not shutdown hdmi-%d\n", pdata->dev_idx);
		return;
	}

	inno_hdmi =  fh2m_inno_platform_get_drvdata(pdev);
	if (!inno_hdmi) {
		fh2m_innodpu_err(&pdev->dev, "hdmi handle is NULL\n");
		return;
	}

	inno_hdmi_encoder_mode_disable(&inno_hdmi->encoder);
	inno_hdmi_suspend(&pdev->dev);
}

static const struct dev_pm_ops inno_hdmi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(inno_hdmi_suspend, inno_hdmi_resume)
};

struct platform_driver g_innogpu_hdmi_driver = {
	.probe = inno_hdmi_probe,
	.remove = inno_hdmi_remove,
	.shutdown = inno_hdmi_shutdown,
	.driver = {
		.name = INNO_HDMI_DEVICE_NAME,
		.pm = &inno_hdmi_pm_ops,
	},
	.id_table = s_inno_hdmi_device_id_table,
};

