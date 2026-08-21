/*************************************************************************/ /*!
@File			innodpu_panel_backlight.c
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

#include <linux/backlight.h>
#include <drm/drm_connector.h>
#include "hal_interface.h"
#include "innodpu_common_drm_panel.h"
#include "innodpu_dp_common.h"
#include "innodpu_connector.h"
#include "innodpu_panel_pwr.h"
#include "innodpu_panel_backlight.h"
#include "inno_lock.h"

static bool s_bl_en_ctrl = false;
module_param(s_bl_en_ctrl, bool, 0644);
MODULE_PARM_DESC(s_bl_en_ctrl, "PWM & BL_EN CTRL enable(default: disabled)");

/* TODO:
 * Implement HDR, right now we just implement the bare minimum to bring us back into SDR mode so we
 * can make people's backlights work in the mean time
 */

/*
 * DP AUX registers for inno's proprietary HDR backlight interface. We define
 * them here since we'll likely be the only driver to ever use these.
 */
#define INNO_EDP_HDR_TCON_CAP0                                        0x340

#define INNO_EDP_HDR_TCON_CAP1                                        0x341
# define INNO_EDP_HDR_TCON_2084_DECODE_CAP                           INNO_BIT(0)
# define INNO_EDP_HDR_TCON_2020_GAMUT_CAP                            INNO_BIT(1)
# define INNO_EDP_HDR_TCON_TONE_MAPPING_CAP                          INNO_BIT(2)
# define INNO_EDP_HDR_TCON_SEGMENTED_BACKLIGHT_CAP                   INNO_BIT(3)
# define INNO_EDP_HDR_TCON_BRIGHTNESS_NITS_CAP                       INNO_BIT(4)
# define INNO_EDP_HDR_TCON_OPTIMIZATION_CAP                          INNO_BIT(5)
# define INNO_EDP_HDR_TCON_SDP_COLORIMETRY_CAP                       INNO_BIT(6)
# define INNO_EDP_HDR_TCON_SRGB_TO_PANEL_GAMUT_CONVERSION_CAP        INNO_BIT(7)

#define INNO_EDP_HDR_TCON_CAP2                                        0x342
# define INNO_EDP_SDR_TCON_BRIGHTNESS_AUX_CAP                        INNO_BIT(0)

#define INNO_EDP_HDR_TCON_CAP3                                        0x343

#define INNO_EDP_HDR_GETSET_CTRL_PARAMS                               0x344
# define INNO_EDP_HDR_TCON_2084_DECODE_ENABLE                        INNO_BIT(0)
# define INNO_EDP_HDR_TCON_2020_GAMUT_ENABLE                         INNO_BIT(1)
# define INNO_EDP_HDR_TCON_TONE_MAPPING_ENABLE                       INNO_BIT(2) /* Pre-TGL+ */
# define INNO_EDP_HDR_TCON_SEGMENTED_BACKLIGHT_ENABLE                INNO_BIT(3)
# define INNO_EDP_HDR_TCON_BRIGHTNESS_AUX_ENABLE                     INNO_BIT(4)
# define INNO_EDP_HDR_TCON_SRGB_TO_PANEL_GAMUT_ENABLE                INNO_BIT(5)
/* Bit 6 is reserved */
# define INNO_EDP_HDR_TCON_SDP_COLORIMETRY_ENABLE                    INNO_BIT(7)

#define INNO_EDP_HDR_CONTENT_LUMINANCE                                0x346 /* Pre-TGL+ */
#define INNO_EDP_HDR_PANEL_LUMINANCE_OVERRIDE                         0x34A
#define INNO_EDP_SDR_LUMINANCE_LEVEL                                  0x352
#define INNO_EDP_BRIGHTNESS_NITS_LSB                                  0x354
#define INNO_EDP_BRIGHTNESS_NITS_MSB                                  0x355
#define INNO_EDP_BRIGHTNESS_DELAY_FRAMES                              0x356
#define INNO_EDP_BRIGHTNESS_PER_FRAME_STEPS                           0x357

#define INNO_EDP_BRIGHTNESS_OPTIMIZATION_0                            0x358
# define INNO_EDP_TCON_USAGE_MASK                             GENMASK(0, 3)
# define INNO_EDP_TCON_USAGE_UNKNOWN                                    0x0
# define INNO_EDP_TCON_USAGE_DESKTOP                                    0x1
# define INNO_EDP_TCON_USAGE_FULL_SCREEN_MEDIA                          0x2
# define INNO_EDP_TCON_USAGE_FULL_SCREEN_GAMING                         0x3
# define INNO_EDP_TCON_POWER_MASK                                    INNO_BIT(4)
# define INNO_EDP_TCON_POWER_DC                                    (0 << 4)
# define INNO_EDP_TCON_POWER_AC                                    (1 << 4)
# define INNO_EDP_TCON_OPTIMIZATION_STRENGTH_MASK             GENMASK(5, 7)

#define INNO_EDP_BRIGHTNESS_OPTIMIZATION_1                            0x359

struct pwm_backlight {
	struct backlight_device *base;
	struct inno_panel *panel;
	struct drm_dp_aux *aux;
	enum reg_entity pwm_ctrl_reg;  /* pwm ctrl */
	enum reg_entity pwm_clock_reg; /* clock reg */
	enum reg_entity pwm_start_reg; /* start reg */
	enum reg_entity pwm_end_reg;   /* end reg */
	enum reg_module pwm_reg_type;  /* reg type */

	u32  pwm_clock;         /* pwm clock */
	u32  max_pwm;
	u32  min_pwm;
	inno_dev *parent;
	u16	 max_brightness;
	u16	 last_brightness;
	u16	 default_brightness;
	u16	 pwm_freq;
	u16	 vesa_max;

	bool enabled;
	bool sdr_uses_aux;
	bool vesa_uses_aux;
	bool vesa_lsb_reg_used;
	bool vesa_aux_set;
	bool is_R1;
};

static int inline bl_reg_write32(struct pwm_backlight *bl, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t val)
{
	if (!bl) {
		return -1;
	}

	if (reg_type == REG_M_PWM) {
		WARN_ON(!bl->parent);
		fh2m_hal_reg_write32(bl->parent, reg_type, entity, val);
	}

	return 0;
}

static int inline bl_reg_read32(struct pwm_backlight *bl, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t *val)
{
	if (!bl) {
		return -1;
	}

	if (reg_type == REG_M_PWM) {
		WARN_ON(!bl->parent);
		fh2m_hal_reg_read32(bl->parent, reg_type, entity, val);
	}

	return 0;
}

static int inline bl_reg_update_bits(struct pwm_backlight *bl, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t mask, uint32_t val)
{
	u32 value = 0;
	u32 tmp   = 0;

	if (!bl) {
		return -1;
	}

	bl_reg_read32(bl, reg_type, entity, &value);
	tmp = value & ~mask;
	tmp |= val & mask;
	bl_reg_write32(bl, reg_type, entity, tmp);

	return 0;
}

static u32 backlight_cal_duty_value_by_brightness(struct pwm_backlight *bl, u32 period, u16 brightness)
{
	u32 duty = 0;
	u32 step = 0;
	u32 total = 0;
	u32 min_count = 0;
	u32 max_count = 0;

	if (!bl) {
		return 0;
	}

	WARN_ON(!period);
	min_count = bl->min_pwm * period / 100;
	max_count = (100 - bl->max_pwm) * period / 100;
	total = period - max_count - min_count;
	step  = total / bl->max_brightness;
	duty  = brightness * step + min_count;
	if (brightness < 1) {
		duty = min_count;
	}
	if (brightness >= bl->max_brightness) {
		duty = bl->max_pwm * period / 100;
	}

	return duty;
}

static int backlight_aux_set_vesa_enable(struct pwm_backlight *bl, bool enable)
{
	int ret;
	u8 buf;

	if (!bl || !bl->aux) {
		return -EINVAL;
	}
	if (!bl->vesa_uses_aux) {
		return -EINVAL;
	}

	ret = drm_dp_dpcd_readb(bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER, &buf);
	if (ret != 1) {
		DRM_ERROR("%s: Failed to read eDP display control register: %d\n",
			bl->aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}
	if (enable) {
		buf |= DP_EDP_BACKLIGHT_ENABLE;
	} else {
		buf &= ~DP_EDP_BACKLIGHT_ENABLE;
	}

	ret = drm_dp_dpcd_writeb(bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER, buf);
	if (ret != 1) {
		DRM_ERROR("%s: Failed to write eDP display control register: %d\n",
			bl->aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

/**
 * backlight_aux_set_vesa_brightness() - Set the backlight level of an eDP panel via AUX
 * @aux: The DP AUX channel to use
 * @bl: Backlight capability info from drm_edp_backlight_init()
 * @level: The brightness level to set
 *
 * Sets the brightness level of an eDP panel's backlight. Note that the panel's backlight must
 * already have been enabled by the driver by calling drm_edp_backlight_enable().
 *
 * Returns: %0 on success, negative error code on failure
 */
static int backlight_aux_set_vesa_brightness(struct pwm_backlight *bl, u16 brightness)
{
	int ret;
	u8 buf[2] = { 0 };

	if (!bl || !bl->aux) {
		return -EINVAL;
	}

	if (!bl->vesa_uses_aux) {
		return -EINVAL;
	}

	/* The panel uses the PWM for controlling brightness levels */
	if (!bl->vesa_aux_set) {
		return -EINVAL;
	}

	if (bl->vesa_lsb_reg_used) {
		buf[0] = (brightness & 0xff00) >> 8;
		buf[1] = (brightness & 0x00ff);
	} else {
		buf[0] = brightness;
	}

	ret = drm_dp_dpcd_write(bl->aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		DRM_ERROR("%s: Failed to write aux backlight level: %d\n",
			bl->aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

/**
 * This function handles enabling DPCD backlight controls on a panel over DPCD, while additionally
 * restoring any important backlight state such as the given backlight level, the brightness byte
 * count, backlight frequency, etc.
 *
 * Note that certain panels do not support being enabled or disabled via DPCD, but instead require
 * that the driver handle enabling/disabling the panel through implementation-specific means using
 * the EDP_BL_PWR GPIO. For such panels, &drm_edp_backlight_info.aux_enable will be set to %false,
 * this function becomes a no-op, and the driver is expected to handle powering the panel on using
 * the EDP_BL_PWR GPIO.
 *
 * Returns: %0 on success, negative error code on failure.
 */
static int backlight_aux_set_vesa(struct pwm_backlight *bl, u16 brightness)
{
	int ret;
	u32 duty = 0;
	u32 period = 0;
	u8 dpcd_buf;

	if (!bl || !bl->aux) {
		return -EINVAL;
	}

	if (!bl->vesa_uses_aux) {
		return -EINVAL;
	}

	if (bl->vesa_aux_set) {
		dpcd_buf = DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
	} else {
		dpcd_buf = DP_EDP_BACKLIGHT_CONTROL_MODE_PWM;
		DRM_WARN("backlight driver uses pwm or aux ?");
	}

	ret = drm_dp_dpcd_writeb(bl->aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, dpcd_buf);
	if (ret != 1) {
		DRM_WARN("%s: Failed to write aux backlight mode: %d\n",
			    bl->aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	period = bl->vesa_max;
	duty = backlight_cal_duty_value_by_brightness(bl, period, brightness);
	ret = backlight_aux_set_vesa_brightness(bl, duty);
	if (ret < 0) {
		return ret;
	}
	ret = backlight_aux_set_vesa_enable(bl, true);
	if (ret < 0) {
		return ret;
	}

	DRM_DEBUG_KMS("[DPCD brightness controls vesa] duty = %d, duty cycle(%d%%) \n",
				  duty, duty * 100 / period);
	return 0;
}

static int backlight_aux_set_hdr_brightness(struct pwm_backlight *bl, u16 brightness)
{
	int ret;
	u8 buf[4] = { 0 };
	u16 cur_level = 0;
	int err_cnt = 50;

	if (!bl || !bl->aux) {
		return -EINVAL;
	}

	if (!bl->sdr_uses_aux) {
		return -EINVAL;
	}

retry:
	buf[0] = (brightness & 0x00ff);
	buf[1] = (brightness & 0xff00) >> 8;

	ret = drm_dp_dpcd_write(bl->aux, INNO_EDP_BRIGHTNESS_NITS_LSB, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		DRM_ERROR("%s: Failed to write aux backlight level: %d\n", bl->aux->name, ret);
		return ret;
	}

	ret = drm_dp_dpcd_read(bl->aux, INNO_EDP_BRIGHTNESS_NITS_LSB, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		DRM_ERROR("%s: Failed to write aux backlight level: %d\n", bl->aux->name, ret);
		return ret;
	}

	cur_level = (buf[1] & 0xff) << 8 | buf[0];
	if (err_cnt > 0) {
		if (cur_level != brightness) {
			fh2m_inno_udelay(1000);
			err_cnt--;
			goto retry;
		}
	} else {
		DRM_DEBUG_KMS("set hdr brightness failed(retry 50 times)");
	}
	return 0;
}

static int backlight_aux_set_hdr(struct pwm_backlight *bl, u16 brightness)
{
	int ret;
	u32 duty = 0;
	u32 period = 0;
	u8 old_ctrl, ctrl;

	if (!bl || !bl->aux) {
		return -EINVAL;
	}

	if (!bl->sdr_uses_aux) {
		return -EINVAL;
	}

	/* sanxing edp dpms set brightness failed, so msleep 30 */
	if (bl->is_R1) {
		fh2m_inno_udelay(30000);
	} else {
		msleep(30);
	}
	if (!bl->is_R1) {
		ret = drm_dp_dpcd_readb(bl->aux, INNO_EDP_HDR_GETSET_CTRL_PARAMS, &old_ctrl);
		if (ret != 1) {
			DRM_ERROR("Failed to read current backlight control mode: %d\n", ret);
			return ret;
		}
	}

	period = 512;

	duty = backlight_cal_duty_value_by_brightness(bl, period, brightness);
	if (duty == 0) {
		/* TODO hehui edp screen: Brightest at a brightness level of 0 */
		duty = 3;
	}
	backlight_aux_set_hdr_brightness(bl, duty);

	if (!bl->is_R1) {
		ctrl = old_ctrl;
		ctrl |= INNO_EDP_HDR_TCON_BRIGHTNESS_AUX_ENABLE;
		if (ctrl != old_ctrl &&
			drm_dp_dpcd_writeb(bl->aux, INNO_EDP_HDR_GETSET_CTRL_PARAMS, ctrl) != 1) {
			DRM_ERROR("Failed to configure DPCD brightness controls\n");
			return -EIO;
		}
	}

	DRM_DEBUG_KMS("[DPCD brightness controls hdr] duty = %d, duty cycle(%d%%) \n",
				  duty, duty * 100 / period);
	return 0;
}

static int backlight_pwm_set(struct pwm_backlight *bl, u16 brightness)
{
	u32 duty = 0;
	u32 period = 0;

	if (!bl) {
		return -EINVAL;
	}

	/* 1. disable pwm */
	bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_ctrl_reg, 0x0);

	/* 2.set pwm clock */
	if (bl->pwm_freq == 0) {
		bl->pwm_freq = 1;
	}

	period = bl->pwm_clock / bl->pwm_freq;
	bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_clock_reg, period);

	duty = backlight_cal_duty_value_by_brightness(bl, period, brightness);

	/* TODO 0 <= duty <= period */
	if (duty <= 0) {
		/* pwm == 0 need disable pwm */
		bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_ctrl_reg, 0x0);
		DRM_DEBUG_KMS("pwm disabled\n");
		return 0;
	}
	if (duty >= period) {
		/* 0x10 = 0 & 0x14 = 0 --> pwm == 1 */
		duty = 0;
	}

	/* 3. pwm start */
	bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_start_reg, 0x0);
	/* 4. pwm end */
	bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_end_reg, duty);
	/* 3.set pwm enable */
	bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_ctrl_reg, 0x1);

	DRM_DEBUG_KMS("pwm freq = %dHz, duty = %d, duty cycle(%d%%) \n",
					  bl->pwm_freq, duty == 0 ? period : duty,
					  (duty == 0 ? period : duty) * 100 / period);
	return 0;
}

static int panel_backlight_enable(struct pwm_backlight *bl, u16 brightness)
{
	if (!bl) {
		return -1;
	}

	if (s_bl_en_ctrl && bl->panel) {
		panel_set_bl_en_state(bl->panel, false);
	}

	if (bl->aux && (bl->sdr_uses_aux || bl->vesa_uses_aux)) {
		if (bl->sdr_uses_aux) {
			backlight_aux_set_hdr(bl, brightness);
		} else if (bl->vesa_uses_aux) {
			backlight_aux_set_vesa(bl, brightness);
		}
	} else {
		backlight_pwm_set(bl, brightness);
	}

	if (s_bl_en_ctrl && bl->panel) {
		panel_set_bl_en_state(bl->panel, true);
	}

	DRM_DEBUG_KMS("panel brightness = %d\n", brightness);

	return 0;
}

static int panel_backlight_disable(struct pwm_backlight *bl)
{
	if (!bl) {
		return -1;
	}

	if (bl->aux && (bl->sdr_uses_aux || bl->vesa_uses_aux)) {
		if (bl->sdr_uses_aux) {
			/* Nothing to do for AUX based backlight controls */
		} else if (bl->vesa_uses_aux) {
			backlight_aux_set_vesa_enable(bl, false);
		}
	} else {
		/* 2. disable pwm */
		bl_reg_write32(bl, bl->pwm_reg_type, bl->pwm_ctrl_reg, 0x0);
	}

	if (s_bl_en_ctrl && bl->panel) {
		panel_set_bl_en_state(bl->panel, false);
	}

	DRM_DEBUG_KMS("panel %s state is OFF\n",
				  (bl->sdr_uses_aux || bl->vesa_uses_aux) ? "aux" : "pwm");

	return 0;
}

static inline bool panel_backlight_is_blank(const struct backlight_device *bd)
{
	return bd->props.power != FB_BLANK_UNBLANK ||
	       bd->props.fb_blank != FB_BLANK_UNBLANK ||
	       bd->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK);
}

/**
 * backlight_get_brightness - Returns the current brightness value
 * @bd: the backlight device
 *
 * Returns the current brightness value, taking in consideration the current
 * state. If backlight_is_blank() returns true then return 0 as brightness
 * otherwise return the current brightness property value.
 *
 * Backlight drivers are expected to use this function in their update_status()
 * operation to get the brightness value.
 */
static int panel_backlight_get_brightness(struct backlight_device *bd)
{
	if (!bd) {
		return -EINVAL;
	}

	if (panel_backlight_is_blank(bd)) {
		return 0;
	} else {
		return bd->props.brightness;
	}
}

static int panel_backlight_update_status(struct backlight_device *bd)
{
	struct pwm_backlight *bl = bl_get_data(bd);
	u16 brightness = panel_backlight_get_brightness(bd);
	int ret = 0;

	if (!bl || !bd) {
		return -EINVAL;
	}

	if (brightness > bl->max_brightness) {
		brightness = bl->max_brightness;
		bd->props.brightness = brightness;
	}

	if (!panel_backlight_is_blank(bd)) {
			/* turn on pwm */
			ret = panel_backlight_enable(bl, brightness);
			bl->enabled = true;
	} else {
		if (bl->enabled) {
			/* turn off pwm */
			ret = panel_backlight_disable(bl);
			bl->enabled = false;
		}
	}

	bl->last_brightness = brightness;

	return ret;
}

static const struct backlight_ops panel_bl_ops = {
	.update_status  = panel_backlight_update_status,
	.get_brightness = panel_backlight_get_brightness,
};

static inline bool is_edp_backlight_supported(const u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE])
{
	return !!(edp_dpcd[1] & DP_EDP_TCON_BACKLIGHT_ADJUSTMENT_CAP);
}

static inline int backlight_aux_vsea_probe_max(struct pwm_backlight *bl)
{
	int ret;
	u8 pn;

	if (!bl->vesa_aux_set) {
		return 0;
	}

	ret = drm_dp_dpcd_readb(bl->aux, DP_EDP_PWMGEN_BIT_COUNT, &pn);
	if (ret != 1) {
		DRM_DEBUG_KMS("%s: Failed to read pwmgen bit count cap: %d\n",
			    bl->aux->name, ret);
		return -ENODEV;
	}

	pn &= DP_EDP_PWMGEN_BIT_COUNT_MASK;
	bl->vesa_max = (1 << pn) - 1;

	return 0;
}

static bool inno_dp_aux_supports_vesa_backlight(struct pwm_backlight *bl,
												enum connector_backlight_mode mode)
{
	u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE];
	int ret;

	if (!bl || !bl->aux) {
		return false;
	}

	if (mode == CONNECTOR_BACKLIGHT_AUX_VESA) {
		bl->vesa_aux_set = true;
		return true;
	} else {
		return false;
	}

	ret = drm_dp_dpcd_read(bl->aux, DP_EDP_DPCD_REV, edp_dpcd,
			       EDP_DISPLAY_CTL_CAP_SIZE);

	if (!is_edp_backlight_supported(edp_dpcd)) {
		DRM_DEBUG_KMS("DP AUX backlight is not supported\n");
		return false;
	}

	if (!(edp_dpcd[1] & DP_EDP_BACKLIGHT_AUX_ENABLE_CAP)) {
		return false;
	}
	if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_AUX_SET_CAP) {
		bl->vesa_aux_set = true;
	}
	if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_BYTE_COUNT) {
		bl->vesa_lsb_reg_used = true;
	}

	return true;
}

static bool is_inno_tcon_cap(const u8 tcon_cap[4])
{
	return tcon_cap[0] >= 1;
}

static bool inno_dp_aux_supports_hdr_backlight(struct drm_dp_aux *aux,
											   enum connector_backlight_mode mode)
{
	int ret;
	u8 tcon_cap[4];

	if (!aux) {
		return false;
	}

	if (mode == CONNECTOR_BACKLIGHT_AUX_HDR) {
		return true;
	} else {
		return false;
	}

	ret = drm_dp_dpcd_read(aux, INNO_EDP_HDR_TCON_CAP0, tcon_cap, sizeof(tcon_cap));

	if (!(tcon_cap[1] & INNO_EDP_HDR_TCON_BRIGHTNESS_NITS_CAP)) {
		return false;
	}

	DRM_DEBUG_KMS("Detected %s HDR backlight interface version %d\n",
		    is_inno_tcon_cap(tcon_cap) ? "inno" : "unsupported", tcon_cap[0]);

	if (!is_inno_tcon_cap(tcon_cap)) {
		return false;
	}

	return tcon_cap[2] & INNO_EDP_SDR_TCON_BRIGHTNESS_AUX_CAP;
}

static void backlight_get_custominfo_config(struct pwm_backlight *bl)
{
	struct hwinfo_item *hwinfo_item = NULL;
	int ret;

	if (!bl || !bl->parent) {
		return;
	}

	hwinfo_item = kzalloc(sizeof(*hwinfo_item), fh2m_hal_get_inno_gfp_kernel());
	if (!hwinfo_item) {
		DRM_WARN("hwinfo_item has no mem, will use default para");
		goto end;
	}
	if (!fh2m_hal_get_hwinfo_finished_status(bl->parent) &&
		!fh2m_hal_get_custominfo_finished_status(bl->parent)) {
		DRM_WARN("hwinfo or custominfo does not register, will use default para");
		goto end;
	}

	ret = fh2m_hal_hwinfo_get_item(bl->parent, SCR_PWM_FREQUENCY, hwinfo_item);
	if (ret) {
		if (hwinfo_item->def_val32 == -1) {
			DRM_WARN("get custom info def pwm_freq failed, will use driver default para");
			bl->pwm_freq = 1000;
		} else {
			DRM_WARN("get custom info pwm_freq failed, will use default para");
			bl->pwm_freq = hwinfo_item->def_val32 & 0xffff;
		}
	} else {
		if (hwinfo_item->hwinfo_type == HWINFO_TYPE_BYTE16) {
			bl->pwm_freq = hwinfo_item->val32 & 0xffff;
		} else {
			DRM_WARN("hwinfo_type dismatched");
			bl->pwm_freq = 1000;
		}
	}

	ret = fh2m_hal_hwinfo_get_item(bl->parent, SCR_BRIGHTNESS_LEVEL, hwinfo_item);
	if (ret) {
		if (hwinfo_item->def_val32 == -1) {
			DRM_WARN("get custom info def brightness level failed, will use driver default para");
			bl->max_brightness = 100;
		} else {
			DRM_WARN("get custom info brightness level failed, will use default para");
			bl->max_brightness = hwinfo_item->def_val32 & 0xff;
		}
	} else {
		if (hwinfo_item->hwinfo_type == HWINFO_TYPE_BYTE8) {
			bl->max_brightness = hwinfo_item->val32 & 0xff;
		} else {
			DRM_WARN("hwinfo_type dismatched");
			bl->max_brightness = 100;
		}
	}

	ret = fh2m_hal_hwinfo_get_item(bl->parent, SCR_DEF_BRIGHTNESS, hwinfo_item);
	if (ret) {
		if (hwinfo_item->def_val32 == -1) {
			DRM_WARN("get custom info def def_brightness failed, will use driver default para");
			bl->default_brightness = bl->max_brightness / 2;
		} else {
			DRM_WARN("get custom info def_brightness failed, will use default para");
			bl->default_brightness = hwinfo_item->def_val32 & 0xff;
		}
	} else {
		if (hwinfo_item->hwinfo_type == HWINFO_TYPE_BYTE8) {
			bl->default_brightness = hwinfo_item->val32 & 0xff;
		} else {
			DRM_WARN("hwinfo_type dismatched");
			bl->default_brightness = bl->max_brightness / 2;
		}
	}

	ret = fh2m_hal_hwinfo_get_item(bl->parent, SCR_MAX_DUTY_CYCLE, hwinfo_item);
	if (ret) {
		if (hwinfo_item->def_val32 == -1) {
			DRM_WARN("get custom info def max duty cycle failed, will use driver default para");
			bl->max_pwm = 90;
		} else {
			DRM_WARN("get custom info max duty cycle failed, will use default para");
			bl->max_pwm = hwinfo_item->def_val32 & 0xff;
		}
	} else {
		if (hwinfo_item->hwinfo_type == HWINFO_TYPE_BYTE8) {
			bl->max_pwm = hwinfo_item->val32 & 0xff;
		} else {
			DRM_WARN("hwinfo_type dismatched");
			bl->max_pwm = 90;
		}
	}

	ret = fh2m_hal_hwinfo_get_item(bl->parent, SCR_MIN_DUTY_CYCLE, hwinfo_item);
	if (ret) {
		if (hwinfo_item->def_val32 == -1) {
			DRM_WARN("get custom info def min duty cycle failed, will use driver default para");
			bl->min_pwm = 10;
		} else {
			DRM_WARN("get custom info min duty cycle failed, will use default para");
			bl->min_pwm = hwinfo_item->def_val32 & 0xff;
		}
	} else {
		if (hwinfo_item->hwinfo_type == HWINFO_TYPE_BYTE8) {
			bl->min_pwm = hwinfo_item->val32 & 0xff;
		} else {
			DRM_WARN("hwinfo_type dismatched");
			bl->min_pwm = 10;
		}
	}

end:
	/* fix some para */
	if (bl->pwm_freq == 0) {
		bl->pwm_freq = 1000;
	}
	if (bl->max_brightness <= 0 || bl->max_brightness > 255) {
		bl->max_brightness = 100;
	}
	if (bl->default_brightness <= 0 || bl->default_brightness > 255) {
		bl->default_brightness = bl->max_brightness / 2; /* default pwm 50% */
	}
	if (bl->min_pwm >= bl->max_pwm || bl-> min_pwm > 100 || bl->max_pwm > 100) {
		bl->min_pwm   = 10;
		bl->max_pwm   = 90;
	}
	if (bl->default_brightness == 0 && bl->min_pwm == 0) {
		/* output black? I do not think so. */
		bl->default_brightness = bl->max_brightness / 2; /* default pwm 50% */
	}
	if (hwinfo_item) {
		kfree(hwinfo_item);
		hwinfo_item = NULL;
	}

	DRM_DEBUG_KMS("max_brightness = %d, default_brightness = %d, min_pwm = %d, max_pwm = %d, pwm_freq = %d",
				  bl->max_brightness, bl->default_brightness,
				  bl->min_pwm, bl->max_pwm, bl->pwm_freq);
	return ;
}

int inno_panel_backlight_init(struct inno_panel *panel, inno_dev *pdev,
							  struct drm_dp_aux *aux, u8 backlight_mode)
{
	chip_type_e plat;
	struct pwm_backlight *bl;
	struct backlight_properties props = { 0 };

	if (!panel || !panel->dev)
		return -EINVAL;

	if (!pdev) {
		dev_warn(panel->dev, "pdev invalid, backlight has not used\n");
		return -EINVAL;
	}

	bl = devm_kzalloc(panel->dev, sizeof(*bl), fh2m_hal_get_inno_gfp_kernel());
	if (!bl) {
		return -ENOMEM;
	}

	bl->aux    = aux;
	bl->parent = pdev;

	{ // special handling for R1
		struct hw_board_info board = {"R1", "*"};
		bl->is_R1 = innodpu_is_odm_pcb_match(panel->dev, &board);
	}

	backlight_get_custominfo_config(bl);

	/* HDR AND VESA are two AUX channel adjustment backlight Technologies
	 * and we can choose one */
	if (inno_dp_aux_supports_hdr_backlight(bl->aux, backlight_mode)) {
		bl->sdr_uses_aux = true;
		DRM_DEBUG_KMS("DP AUX backlight is hdr mode");
		goto register_bl;
	}

	if (inno_dp_aux_supports_vesa_backlight(bl, backlight_mode)) {
		bl->vesa_uses_aux = true;
		backlight_aux_vsea_probe_max(bl);
		DRM_DEBUG_KMS("DP AUX backlight is vesa mode, probe max = %d", bl->vesa_max);
		goto register_bl;
	}

	if (backlight_mode == CONNECTOR_BACKLIGHT_PWM0) {
		plat = fh2m_hal_get_chiptype(pdev);
		switch(plat) {
		case CHIP_G1_SOC:
		case CHIP_G1P_SOC:
			dev_warn(panel->dev, "current only support g0m/g0c platform.\n");
			return -EINVAL;
		case CHIP_G0_SOC:
		case CHIP_G0M_SOC:
			/* default config */
			bl->pwm_ctrl_reg = REG_ENTITY0000;  /* pwm ctrl */
			bl->pwm_clock_reg = REG_ENTITY0001; /* clock reg */
			bl->pwm_start_reg = REG_ENTITY0002; /* start reg */
			bl->pwm_end_reg  = REG_ENTITY0003;   /* end reg */
			bl->pwm_reg_type = REG_M_PWM;  /* reg type */

			bl->pwm_clock = fh2m_hal_get_pll(pdev, PLL_CBUS);
			if (!bl->pwm_clock) {
				dev_warn(panel->dev, "used 128M default\n");
				bl->pwm_clock = 128;
			}
			bl->pwm_clock = bl->pwm_clock * 1000000;
			DRM_DEBUG_KMS("PWM CLOCK is %d", bl->pwm_clock);
			if (bl->pwm_freq > bl->pwm_clock) {
				bl->pwm_freq = 1000;
				DRM_WARN("fix up pwm freq default 1Khz, because pwm freq <= bl->pwm_clock");
			}
			break;
		default:
			dev_warn(panel->dev, "does not currently support platform.\n");
			return -EINVAL;
		}
	} else {
		dev_warn(panel->dev, "does not currently support backlightmode.\n");
		return -EINVAL;
	}

register_bl:
	props.type       = BACKLIGHT_RAW;
	props.brightness = bl->default_brightness;
	props.max_brightness = bl->max_brightness;
	bl->last_brightness  = props.brightness;
	bl->base = devm_backlight_device_register(panel->dev, "panel_backlight",
						  panel->dev, bl,
						  &panel_bl_ops, &props);
	if (IS_ERR(bl->base))
		return PTR_ERR(bl->base);

	panel->backlight = bl->base;
	bl->panel = panel;

	DRM_DEBUG_KMS("backlight used %s",
				  (bl->sdr_uses_aux || bl->vesa_uses_aux) ? "aux" : "pwm");

	return 0;
}
