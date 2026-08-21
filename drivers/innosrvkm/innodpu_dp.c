#include "../innogpu/compat_kernel6.h"
/*************************************************************************/ /*!
@File			innodpu_dp.c
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
#include <linux/compiler.h>
#include "innodpu_dp.h"
#include "innodpu_connector.h"
#include "innodpu_dp_debugfs.h"
#ifdef CONFIG_DRM_INNO_AUDIO
#include "innoaudio_chip_common.h"
#include "innoaudio_drv.h"
#endif

static int s_dp_monitor = 0x26b;
MODULE_PARM_DESC(s_dp_monitor,
		"\t\tBit 0 (0x01)  will enable edid segment pointer\n"
		"\t\tBit 1 (0x02)  will enable Listen for link status\n"
		"\t\tBit 2 (0x04)  will enable tramsmission configuration to improve signal quality\n"
		"\t\tBit 3 (0x08)  will enable ctx detect retry to ensure connector status\n"
		"\t\tBit 4 (0x10)  will enable lane count adjust\n"
		"\t\tBit 5 (0x20)  will enable mvid software control\n"
		"\t\tBit 6 (0x40)  will enable replacement of non-standard timing\n"
		"\t\tBit 7 (0x80)  will enable display timing bit depth of the dp is obtained from the edid\n"
		"\t\tBit 8 (0x100) will enable support for yuv output\n"
		"\t\tBit 9 (0x200) will enable link power ctrl\n"
		"\t\tBit 10(0x400) will enable alternate_scrambler_reset\n"
		"\t\tBit 11(0x800) will enable audioinfoframe dynamic init\n");
module_param_named(s_dp_monitor, s_dp_monitor, uint, 0600);

static int s_dp_swing_param = 0;
MODULE_PARM_DESC(s_dp_swing_param,
		"\t\tBit 0 - 7 Analogue parameter isel\n"
		"\t\tBit 8 -15 Analogue parameter mainsel\n"
		"\t\tBit 16-23 Analogue parameter post pre-emphasis\n"
		"\t\tBit 24-31 Analogue parameter pre  pre-emphisis\n");
module_param_named(s_dp_swing_param, s_dp_swing_param, uint, 0600);

static int dp_mdelay = 20;
MODULE_PARM_DESC(dp_mdelay, "msleep\n");
module_param_named(dp_mdelay, dp_mdelay, uint, 0600);

bool inno_dp_monitor_en(enum dp_monitor_status info)
{
	if (s_dp_monitor & INNO_BIT(info))
		return true;

	return false;
}

static int inno_dp_hw_init(struct dp_device_t *inno_dp)
{
	if (inno_dp->chip.dp_hw_init)
		return inno_dp->chip.dp_hw_init(&inno_dp->chip);

	return 0;
}

static void inno_dp_hw_fini(struct dp_device_t *inno_dp)
{
	if (inno_dp->chip.dp_hw_fini)
		inno_dp->chip.dp_hw_fini(&inno_dp->chip);
}

static void inno_dp_clear_edid_info(struct dp_chip_t *chip)
{
	if (dp_get_edid(chip)) {
		dp_info(chip->dev, "clear edid\n");
		fh2m_inno_kfree(dp_get_edid(chip));
		dp_set_edid(chip, NULL);
		fh2m_inno_memset(chip->edid_buf, 0, sizeof(chip->edid_buf));
		chip->max_pclk_rx = 0;
	}
}

static void inno_dp_irq_handle(void *data)
{
	int irq_status = 0;
	unsigned int delay_ms = 0;
	struct dp_device_t *inno_dp = container_of(data, struct dp_device_t, chip);

	BUG_ON(!inno_dp);

	if (inno_dp->chip.dp_irq_handle)
		irq_status = inno_dp->chip.dp_irq_handle(data);

	/* irq_status == 0; Not hpd in; hpd out; hpd irq irq generate */
	if (irq_status == 0)
		return;

	/* No hot-pluging on forced connections */
	if (inno_dp->connector.force == DRM_FORCE_ON ||
		inno_dp->connector.force == DRM_FORCE_ON_DIGITAL)
		irq_status = INNODP_HPD_IN;

	/* if hpd work is in pending state and an hpd irq event is triggered
	 * the irq event will not be processed this time in order to avoid the
	 * last hpd in event not being reported.
	 */
	if (delayed_work_pending(&inno_dp->hpd_work)) {
		dp_info(inno_dp->dev, "[hpd work]work pending, clear hpd-irq status\n");
		irq_status &= ~INNODP_HPD_IRQ;
	}

	if (irq_status) {

		if (irq_status & INNODP_HPD_OUT)
			delay_ms = 500;

		dp_set_hpg_status(&inno_dp->chip, irq_status);
		cancel_delayed_work(&inno_dp->hpd_work);

		dp_info(inno_dp->dev, "QueueHpdWork[delay-%d ms]\n", delay_ms);
		queue_delayed_work(inno_dp->hpd_wq, &inno_dp->hpd_work, msecs_to_jiffies(delay_ms));
	}
}

static void __attribute__((unused)) inno_dp_hw_irq_enable(struct dp_device_t *inno_dp)
{
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "hw irq enable\n");

	dp_set_hpg_status(&inno_dp->chip, INNODP_HPD_OUT);
	if (inno_dp->chip.dp_irq_enable)
		inno_dp->chip.dp_irq_enable(&inno_dp->chip);
}

static void inno_dp_hw_irq_disable(struct dp_device_t *inno_dp)
{
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "hw irq disable\n");

	if (inno_dp->chip.dp_irq_disable)
		inno_dp->chip.dp_irq_disable(&inno_dp->chip);
}

static int inno_dp_aux_rw(struct dp_device_t *inno_dp, struct aux_cfg *aux_rx)
{
	int drm_power_state = -1;
	struct dp_chip_t *chip = &inno_dp->chip;

	if (!aux_rx || !chip->dp_aux_channel_run)
		return DP_AUX_NATIVE_REPLY_NACK;

	if (inno_dp->panel) {
		if (inno_dp->drm_dev)
			drm_power_state = inno_dp->drm_dev->switch_power_state;

		if (drm_power_state == DRM_SWITCH_POWER_OFF) {
			fh2m_innodpu_info(chip->dev, DPU_UT_DP, "suspend...,disconnected\n");
			return DP_AUX_NATIVE_REPLY_NACK;
		}
	}

	inno_panel_prepare(chip->panel);

	if (dp_get_hpg_status(chip) == INNODP_HPD_OUT) {
		fh2m_innodpu_info(chip->dev, DPU_UT_DP, "disconnected\n");
		return DP_AUX_NATIVE_REPLY_NACK;
	}

	return chip->dp_aux_channel_run(aux_rx, chip);
}

static void dp_set_poll_cnt(struct dp_device_t *dp, int val)
{
	atomic64_set(&dp->poll_cnt, val);
}

static long long dp_get_poll_cnt(struct dp_device_t *dp)
{
	return atomic64_read(&dp->poll_cnt);
}

static const int inno_bw_table[] = {
	INNODP_LINK_BW_1_62,
	INNODP_LINK_BW_2_7,
	INNODP_LINK_BW_5_4,
	INNODP_LINK_BW_8_1,
};

static int inno_dp_rate_index(const int *rates, int len, int rate)
{
	int i;

	for (i = 0; i < len; i++)
		if (rate == rates[i])
			return i;

	return -1;
}

int inno_dp_mvid_calc(struct dp_chip_t *chip)
{
	int mvid = 0, mod = 0;
	struct dp_device_t *inno_dp = NULL;
	u64 temp1 = 0, f_ls_clk = 0, f_strm_clk = 0;

	if (!chip || !inno_dp_monitor_en(dp_mvid_sw_cacl))
		return -1;

	inno_dp = to_dp_device(chip);

	if (!inno_dp || inno_dp->current_mode.clock <= 0 || chip->lane_rate == 0)
		return -1;

	/* Link stream clock == pixel clock */
	f_strm_clk = inno_dp->current_mode.clock; /*KHZ*/

	/* Calculate Link Symbol clock */
	f_ls_clk = chip->lane_rate * 27000ULL; /*KHZ*/

	/* M / N == f_strm_clk / f_ls_clk
	 * M == f_strm_clk * N / f_ls_clk
	 */
	temp1 = f_strm_clk * ASYNC_CLOCK_MSA_NVID;
	mod = do_div(temp1, f_ls_clk);

	/* round up to five */
	mvid = temp1 + ((mod * 10ULL / f_ls_clk >= 5) ? 1 : 0);
	if (mvid >= (1<<24)) {
		mvid = -1;
		fh2m_innodpu_info(chip->dev, DPU_UT_DP, "[BAD] Invalid mvid\n");
	}

	fh2m_innodpu_info(chip->dev, DPU_UT_DP, "[INNO DP] MSA-MVID:%d "
			"f_strm_clk:%lld KHZ f_ls_clk:%lld KHZ mod:%d",
			mvid, f_strm_clk, f_ls_clk, mod);

	return mvid;
}

int inno_dp_sink_power_ctrl(struct dp_device_t *inno_dp, bool power_on)
{
	u8 value = 0;
	int err = 0;

	/* DP_SET_POWER register is only available on DPCD v1.1 and later */
	if (!inno_dp || inno_dp->dpcd[DP_DPCD_REV] < DP_REV_11)
		return 0;

	if (!inno_dp_monitor_en(dp_link_pwr_ctl))
		return 0;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "sink_power_ctrl:%s\n",
			power_on ? "power on" : "powern down");

	err = drm_dp_dpcd_read(&inno_dp->aux, DP_SET_POWER, &value, 1);
	if (err < 0)
		return err;

	value &= ~DP_SET_POWER_MASK;
	/* Sink - (State3 Sleep):
	 * 1. Hpd asserted
	 * 2. Aux enabled for differential signal monitoring,
	 * 3. Main-link Rx disabled
	 */

	/* Sink - (State 2 standby):
	 * 1. Hpd asserted
	 * 2. Aux enabled for differential signal monitoring,
	 * 3. Main-link Rx enabled
	 */

	value |= (power_on ? DP_SET_POWER_D0 : DP_SET_POWER_D3);

	err = drm_dp_dpcd_write(&inno_dp->aux, DP_SET_POWER, &value, 1);
	if (err < 0)
		return err;

	if (power_on) {
		/*
		 * According to the DP 1.1 specification, a "Sink Device must exit the
		 * power saving state within 1 ms" (Section 2.5.3.1, Table 5-52, "Sink
		 * Control Field" (register 0x600).
		 */
		if (inno_dp->is_R1) {
			fh2m_inno_udelay(dp_mdelay * 1000);
		} else {
			if (inno_dp->use_rate_select) {
				 /* For an embedded connection, a Sink device may take up to 20 ms from a power-save mode
				  * until it is ready to reply to an AUX request transaction
				  */
				 fh2m_inno_usleep_range(20000, 20100);
			} else {
				 fh2m_inno_usleep_range(1000, 2000);
			}
		}
	} else {
		if (inno_dp->is_R1) {
			/* timing T10, at least 100ms */
			fh2m_inno_udelay(100000);
		}
	}
	return 0;
}

static void inno_dp_rate_choose(struct dp_device_t *inno_dp)
{
	int i = 0;
	struct dp_chip_t *chip = &inno_dp->chip;

	/* For EDP 1.4 */
	if (inno_dp->use_rate_select) {
		chip->lane_rate = inno_dp->max_sink_rates;
		fh2m_innodpu_info(chip->dev, DPU_UT_DP, "use rate select:%#.8x\n", chip->lane_rate);
	}

	if (chip->lane_rate > chip->max_source_rate)
		chip->lane_rate = INNODP_LINK_BW_5_4;

	i = inno_dp_rate_index(inno_bw_table, INNO_ARRAY_SIZE(inno_bw_table), chip->lane_rate);
	if (i < 0) {
		dp_info(chip->dev,
				"[BAD] Link rate matching failed using 5.4Gbps\n");
		chip->lane_rate = INNODP_LINK_BW_5_4;
		chip->phy_rate = 0x2;
	} else {
		chip->phy_rate = i;
	}
}

static void inno_dp_voltage_swing_adjust(struct dp_device_t *inno_dp)
{
	int j = 0;
	uint8_t train_set[4] = {0};

	if (!inno_dp)
		return;

	for (j = 0; j < inno_dp->chip.lane_count; j++) {
		train_set[j] = inno_dp->chip.lane_swing[j];
		if (inno_dp->chip.lane_swing[j] >= DP_TRAIN_VOLTAGE_SWING_LEVEL_2)
			train_set[j] |= DP_TRAIN_MAX_SWING_REACHED;

		train_set[j] |= (inno_dp->chip.lane_emphasis[j]);
		if (inno_dp->chip.lane_emphasis[j] >= DP_TRAIN_PRE_EMPH_LEVEL_2)
			train_set[j] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
	}

	if ((inno_dp_monitor_en(dp_swing_adjust) || inno_dp->compliance.test_active) &&
		inno_dp->chip.dp_source_swing_adjust)
		inno_dp->chip.dp_source_swing_adjust(&inno_dp->chip, s_dp_swing_param);

	drm_dp_dpcd_write(&inno_dp->aux, DP_TRAINING_LANE0_SET,
		train_set, inno_dp->chip.lane_count);
}

static void inno_dp_get_adjust_train(struct dp_device_t *inno_dp,
		const uint8_t link_status[DP_LINK_STATUS_SIZE])
{
	int lane = 0;

	if (!inno_dp)
		return;

	for (lane = 0; lane < inno_dp->chip.lane_count; lane++) {
		inno_dp->chip.lane_swing[lane] = drm_dp_get_adjust_request_voltage(link_status, lane);
		inno_dp->chip.lane_emphasis[lane] = drm_dp_get_adjust_request_pre_emphasis(link_status, lane);
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
				"adjust request lane%d:lane_swing:%d lane_emphasis:%d ",
				lane, inno_dp->chip.lane_swing[lane],
				inno_dp->chip.lane_emphasis[lane] >> DP_TRAIN_PRE_EMPHASIS_SHIFT);
	}
}

static void inno_edp_caps_set(struct dp_device_t *inno_dp)
{
	u8 rate_select = 0, scramble_reset = 0;
	int rate_index = 0;

	if (!inno_dp)
		return;

	if (inno_dp->use_rate_select) {

		inno_dp_voltage_swing_adjust(inno_dp);

		rate_index = inno_dp_rate_index(inno_dp->sink_rates,
					inno_dp->num_sink_rates, inno_dp->chip.lane_rate);
		if (fh2m_inno_warn_on(rate_index < 0))
			rate_index = 0;

		rate_select = rate_index;

		drm_dp_dpcd_write(&inno_dp->aux, DP_LINK_RATE_SET,
				&rate_select, 1);

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "EDP Rate Set[%#x]\n", inno_dp->chip.lane_rate);

		if (inno_dp->chip.alternate_scrambler_reset) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "dp alternate scrambler reest enable\n");
			scramble_reset = DP_ALTERNATE_SCRAMBLER_RESET_ENABLE;
			drm_dp_dpcd_write(&inno_dp->aux, DP_EDP_CONFIGURATION_SET, &scramble_reset, 1);
		}
	}
}

static void inno_edp_init_dpcd(struct dp_device_t *inno_dp)
{
	int i = 0;
	int val = 0;
	__le16 sink_rates[DP_MAX_SUPPORTED_RATES];

	if (!inno_dp)
		return;

	/*
	 * Read the eDP display control registers.
	 *
	 * Do this independent of DP_DPCD_DISPLAY_CONTROL_CAPABLE bit in
	 * DP_EDP_CONFIGURATION_CAP, because some buggy displays do not have it
	 * set, but require eDP 1.4+ detection (e.g. for supported link rates
	 * method). The display control registers should read zero if they're
	 * not supported anyway.
	 */
	if (drm_dp_dpcd_read(&inno_dp->aux, DP_EDP_DPCD_REV,
			     inno_dp->edp_dpcd, sizeof(inno_dp->edp_dpcd)) ==
			     sizeof(inno_dp->edp_dpcd)) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,"eDP DPCD: %*ph\n",
			    (int)sizeof(inno_dp->edp_dpcd),
			    inno_dp->edp_dpcd);
	}

	/* Clear the default sink rates */
	inno_dp->num_sink_rates = 0;
	inno_dp->max_sink_rates = 0;

	/* Read the eDP 1.4+ supported link rates. */
	if (inno_dp->edp_dpcd[0] >= DP_EDP_14) {
		drm_dp_dpcd_read(&inno_dp->aux, DP_SUPPORTED_LINK_RATES,
				sink_rates, sizeof(sink_rates));

		for (i = 0; i < INNO_ARRAY_SIZE(sink_rates); i++) {
			val = le16_to_cpu(sink_rates[i]);

			if (val == 0)
				break;

			/* Value read multiplied by 200kHz gives the per-lane
			 * link rate in kHz.
			 */
			inno_dp->sink_rates[i] = (val * 200) / 270000;
			if (inno_dp->max_sink_rates < inno_dp->sink_rates[i] &&
				inno_dp->chip.max_source_rate >= inno_dp->sink_rates[i] &&
				inno_dp_rate_index(inno_bw_table, INNO_ARRAY_SIZE(inno_bw_table), inno_dp->sink_rates[i]) >= 0) {
				inno_dp->max_sink_rates = inno_dp->sink_rates[i];
			}

			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "edp sink rates-%#.8x\n", inno_dp->sink_rates[i]);
		}
		inno_dp->num_sink_rates = i;
	}

	/*
	 * Use DP_LINK_RATE_SET if DP_SUPPORTED_LINK_RATES are available,
	 * default to DP_MAX_LINK_RATE and DP_LINK_BW_SET otherwise.
	 */
	if (inno_dp->num_sink_rates) {
		inno_dp->use_rate_select = true;
		inno_dp->connector.connector_type = DRM_MODE_CONNECTOR_eDP;
		inno_dp->connector.force = DRM_FORCE_ON;
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "edp max sink rate-%#.8x\n", inno_dp->max_sink_rates);
	} else {
		inno_dp->use_rate_select = false;
	}
}

static bool inno_dp_rate_valid(int lane_rate, int lane_count, int clock, int bpc)
{
	unsigned long requirement, capacity;

	capacity = lane_rate * 27 * 1000 * 8 * lane_count;
	requirement = clock * bpc * 3;

	if (capacity >= requirement)
		return true;

	return false;
}

static const unsigned int inno_misc0[][5] = {
	{
	/* Legacy RGB mode*/
		0x0,  /* 6bpc */
		0x20, /* 8bpc */
		0x40, /* 10bpc*/
		0x60, /* 12bpc*/
		0x80, /* 16bpc*/
//	}, {
//	/* CEA RGB */
//		0x8,  /* 6bpc */
//		0x28, /* 8bpc */
//		0x48, /* 10bpc*/
//		0x68, /* 12bpc*/
//		0x88, /* 16bpc*/
	}, {
	/* Ycbcr 444 */
		0,	  /* unsupport 6bpc */
		0x2c, /* 8bpc */
		0x4c, /* 10bpc*/
		0x6c, /* 12bpc*/
		0x8c, /* 16bpc*/
	}, {
	/* Ycbcr 422 */
		0,	  /* unsupport 6bpc */
		0x2a, /* 8bpc */
		0x4a, /* 10bpc*/
		0x6a, /* 12bpc*/
		0x8a, /* 16bpc*/
	},
};

static const unsigned int inno_dp_bpc[5] = {6, 8, 10, 12, 16};

static int inno_dp_bpc_choose(struct dp_device_t *inno_dp)
{
	int index = 0;
	struct dp_display_info *info = &inno_dp->chip.display_info;

	index = inno_dp_rate_index(inno_dp_bpc, INNO_ARRAY_SIZE(inno_dp_bpc), info->bpc);
	if (index < 0) {
		index = 1;
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "[BAD]Invalid bpc, Using 8 bpc\n");
	}

	while (index >= 0) {
		if (inno_dp_rate_valid(inno_dp->chip.lane_rate, inno_dp->chip.lane_count,
			inno_dp->current_mode.clock, inno_dp_bpc[index]))
			break;

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "[BAD]Invalid bpp, Try to lower the bit depth\n");
		index--;
	}

	if (index < 0) {
		fh2m_innodpu_err(inno_dp->dev, "Timing of dp exceeds maximum limit\n");
		index = 1;
	}

	return index;
}

static void inno_dp_video_timing_prepare(struct dp_device_t *inno_dp)
{
	int index = 0;
	struct dp_display_info *info = &inno_dp->chip.display_info;
	const struct drm_display_info *drm_info = &inno_dp->connector.display_info;

	/* default:
	 * bpc = 8
	 * color formats: RGB4:4:4
	*/
	info->bpc = 8;
	info->color_formats = INNO_COLOR_FORMAT_RGB444;

	if (inno_dp_monitor_en(dp_bpc_use_edid)) {
		info->bpc = drm_info->bpc;
		info->color_formats = INNO_COLOR_FORMAT_RGB444;
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "Maximum bpc supported by the monitor:%d\n", info->bpc);

		if (inno_dp_monitor_en(dp_yuv_support)) {
			if (drm_info->color_formats & INNO_COLOR_FORMAT_YCBCR444) {
				info->color_formats = INNO_COLOR_FORMAT_YCBCR444;
				fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "color formats:YUV444\n");
			} else if (drm_info->color_formats & INNO_COLOR_FORMAT_YCBCR422) {
				info->color_formats = INNO_COLOR_FORMAT_YCBCR422;
				fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "color formats:YUV422\n");
			}
		}
	}

	index = inno_dp_bpc_choose(inno_dp);
	info->bpc = inno_dp_bpc[index];
	info->misc0 = inno_misc0[info->color_formats >> 1][index];
	info->video_map = index + INNO_COLOR_FORMAT_YCBCR422 * (info->color_formats >> 1);

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "inno_dp display info"
			"[bpc:%d misc0:%#x video_map:%d]\n",
			info->bpc, info->misc0, info->video_map);
}

static void inno_dp_lane_choose(struct dp_device_t *inno_dp)
{
	const int lanes[3] = {1, 2, 4};
	char buf[MONITOR_NAME_LEN] = {0};
	struct dp_chip_t *chip = &inno_dp->chip;
	/* Unis displays flicker at 2560x1400@75HZ and 1600x900@60HZ,
	 * lowering the number of links can fix it!
	 * */
	const char *monitor_array[] = {
		"Unis V271Q"
	};

	inno_dp->reduce_lane_count = false;

	/* Maximum number of links used by default */
	if (inno_dp_rate_index(lanes, INNO_ARRAY_SIZE(lanes), chip->lane_count) < 0) {
		chip->lane_count = 0x4;
		inno_dp->reduce_lane_count = true;
		dp_info(inno_dp->dev,
				"[BAD] Link count matching failed using 4 lane\n");
	}

	if (dp_get_edid(chip) && !inno_dp_monitor_en(dp_lanes_adjust)) {
		drm_edid_get_monitor_name(dp_get_edid(chip), buf, INNO_ARRAY_SIZE(buf));
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "monitor-%s\n", buf);

		if (match_string(monitor_array, INNO_ARRAY_SIZE(monitor_array), buf) >= 0) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "monitor-reduce lane_count!\n");
			inno_dp->reduce_lane_count = true;
		}
	}

	if ((inno_dp->reduce_lane_count || inno_dp_monitor_en(dp_lanes_adjust)) && (chip->lane_count > 1)) {
		if (inno_dp_rate_valid(chip->lane_rate, chip->lane_count >> 1,
			inno_dp->current_mode.clock, inno_dp_bpc[1])) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "reduce the number of lanes\n");
			chip->lane_count = chip->lane_count >> 1;
		}
	}

	inno_dp->chip.phy_lanes = inno_dp_rate_index(lanes, INNO_ARRAY_SIZE(lanes), chip->lane_count);
}

static void inno_dp_rate_adjust(struct dp_device_t *inno_dp, bool adjust)
{
	int i = 0;
	u8 reduce_phy_rate = 0;
	u8 reduce_lane_rate = 0;

	if (adjust && inno_dp->current_mode.clock) {
		i = inno_dp_rate_index(inno_bw_table, INNO_ARRAY_SIZE(inno_bw_table), inno_dp->chip.lane_rate);
		if (i > 0 && i < INNO_ARRAY_SIZE(inno_bw_table)) {
			reduce_lane_rate = inno_bw_table[i - 1];
			reduce_phy_rate = i - 1;
		} else {
			return;
		}

		if (inno_dp->use_rate_select)
			i = inno_dp_rate_index(inno_dp->sink_rates,
					inno_dp->num_sink_rates, reduce_lane_rate);

		if (inno_dp_rate_valid(reduce_lane_rate, inno_dp->chip.lane_count,
			inno_dp->current_mode.clock, inno_dp->chip.display_info.bpc) && i >= 0) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "reduce bit rate!!!!!!\n");
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
					"old:lane_rate-%#x, phy_rate-%#d"
					"new:lane_rate-%#x, phy_rate-%d\n",
					inno_dp->chip.lane_rate,inno_dp->chip.phy_rate,
					reduce_lane_rate, reduce_phy_rate);

			inno_dp->chip.lane_rate = reduce_lane_rate;
			inno_dp->chip.phy_rate = reduce_phy_rate;
		}
	}
}

static int inno_dp_caps_prepare(struct dp_device_t *inno_dp)
{
	if (!inno_dp)
		return -EFAULT;

	if (inno_dp->compliance.test_type == DP_TEST_LINK_TRAINING) {
		inno_dp->chip.lane_rate = inno_dp->compliance.test_link_rate;
		inno_dp->chip.lane_count = inno_dp->compliance.test_lane_count;
	} else {
		/* sink - max link rate */
		inno_dp->chip.lane_rate = inno_dp->dpcd[DP_MAX_LINK_RATE];
		/* sink - max link count */
		inno_dp->chip.lane_count = inno_dp->dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;
	}

	/* sink - enhanced mode support */
	inno_dp->chip.enhance_mode = drm_dp_enhanced_frame_cap(inno_dp->dpcd) ? 1 : 0;

	inno_dp->chip.tps3_support = drm_dp_tps3_supported(inno_dp->dpcd);

	/* sink - ASSR support, For edp devices only */
	if (inno_dp_monitor_en(alternate_scrambler_reset)) {
		if (inno_dp->use_rate_select) {
			inno_dp->chip.alternate_scrambler_reset = inno_dp->dpcd[DP_EDP_CONFIGURATION_CAP] & DP_ALTERNATE_SCRAMBLER_RESET_CAP;
		} else {
			inno_dp->chip.alternate_scrambler_reset = 0;
		}
	} else {
		inno_dp->chip.alternate_scrambler_reset = 0;
	}
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"DpcdRev-%#x MaxLineCount-%d MaxLinkRate-%#x ASSR-%d Enhance-%d Tps3-%s\n",
			inno_dp->dpcd[DP_DPCD_REV],
			inno_dp->chip.lane_count,
			inno_dp->chip.lane_rate,
			inno_dp->chip.alternate_scrambler_reset,
			inno_dp->chip.enhance_mode,
			inno_dp->chip.tps3_support ? "ture" : "false");

	/* Match to a link rate supported by source */
	inno_dp_rate_choose(inno_dp);

	/* Adjust the numger of links in the source to avoid flickering of
	 * individual monitor with specific resolutions
	 * */
	inno_dp_lane_choose(inno_dp);

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"using %u lanes at %u kHz (phy lanes:%d phy rates:%d)\n",
			inno_dp->chip.lane_count, inno_dp->chip.lane_rate * 27000,
			inno_dp->chip.phy_lanes, inno_dp->chip.phy_rate);

	return 0;
}

void inno_dp_check_sink_connection(struct dp_device_t *inno_dp)
{
	unsigned long timeout = 0;
	int ret = 0, retry = 0, status = 0;

	if (!inno_dp->chip.connector_detect) {
		fh2m_innodpu_err(inno_dp->chip.dev, "ConnectorDetect NULL!!!");
		return;
	}

	if ((inno_dp->connector.force == DRM_FORCE_ON ||
		 inno_dp->connector.force == DRM_FORCE_ON_DIGITAL) &&
		inno_dp->connected) {
		dp_set_hpg_status(&inno_dp->chip, INNODP_HPD_IN);
		return;
	}

	dp_info(inno_dp->dev, "sink connection check\n");

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);

	/*Minimum TImes after asserting HPD at the end of IRQ_HPD:2ms */
	fh2m_inno_usleep_range(5000, 6000);

	/*
	 * Attempt to read sink revision, retry in case the sink may not be ready.
	 *
	 * Sinks are *supposed* to come up within 1ms from an off state, but
	 * some docks need more time to power up.
	 */

	status = inno_dp->chip.connector_detect(&inno_dp->chip);
	if (status == inno_connector_status_connected)
		fh2m_inno_usleep_range(15000, 20000);

	if (inno_dp->connector.force == DRM_FORCE_ON ||
		 inno_dp->connector.force == DRM_FORCE_ON_DIGITAL) {
		dp_set_hpg_status(&inno_dp->chip, INNODP_HPD_IN);
	}

	timeout = jiffies + fh2m_inno_msecs_to_jiffies(2000);

	while (fh2m_inno_time_before(timeout)) {

		if (dp_get_hpg_status(&inno_dp->chip) == INNODP_HPD_OUT) {
			dp_info(inno_dp->dev, "HpdDisconnect\n");
			inno_dp->connected = false;
			break;
		}

		ret = drm_dp_dpcd_read(&inno_dp->aux, DP_DPCD_REV, inno_dp->dpcd, DP_RECEIVER_CAP_SIZE);
		if (ret == DP_RECEIVER_CAP_SIZE &&
			inno_dp->dpcd[DP_DPCD_REV] >= DP_REV_10 &&
			inno_dp->dpcd[DP_DPCD_REV] <= DP_REV_14) {
			inno_dp->connected = true;

			inno_edp_init_dpcd(inno_dp);

			break;
		} else {
			dp_info(inno_dp->dev,
					"sink connection check-retry[%d]\n", ++retry);
			inno_dp->connected = false;
		}

		fh2m_inno_usleep_range(5000, 10000);
	}

	dp_info(inno_dp->dev,
			"sink connection check end[%s]\n", inno_dp->connected ? "connect" : "disconnect");

	fh2m_inno_mutex_unlock(inno_dp->aux_mutex);
}

static int inno_dp_detect_ctx(struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx, bool force)
{
	struct dp_device_t *inno_dp = to_dp_device(connector);
	int status = inno_connector_status_unknown;

	if (!inno_dp)
		return -EFAULT;

	if (!inno_dp->chip.connector_detect) {
		fh2m_innodpu_err(inno_dp->dev, "Not support %s chip detect handle !!!\n",
				inno_dp->name);
		return -EFAULT;
	}

	if (connector->force == DRM_FORCE_ON ||
		connector->force == DRM_FORCE_ON_DIGITAL) {
		return connector_status_connected;
	}

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);

	status = inno_dp->chip.connector_detect(&inno_dp->chip);
	if (status == inno_connector_status_connected) {
		if (inno_dp_monitor_en(dp_rxctx_detect) && !inno_dp->connected) {
			dp_info(inno_dp->dev,
					"[BAD]hpd is detected, but aux channel no answer.\n");
			status = inno_connector_status_disconnected;
			/**
			 * hpd pull high, aux read and write normal, do not need to reset ctrl trigger hpd signal,
			 * only hpd pull high, aux can not read and write, the software triggers once the source
			 * side of the hpd signal to circumvent the:
			 * 1. after the first power on, the source side of the aux initialisation abnormalities
			 * 	caused by the connection state detection error
			 * 2. individual monitors can still detect the hpd signal after switching off the monitor
			 * 	power supply. but aux can not read and write
			 */
			if (inno_dp->chip.dp_hpd_signal_retrigger && !inno_dp->hpd_notify &&
				READ_ONCE(inno_dp->connector.dpms) != DRM_MODE_DPMS_ON) {
				dp_info(inno_dp->dev,
						"hpd signal retrigger.\n");
				inno_dp->hpd_notify = true;
				/**
				 * Note-
				 * dp hpd signal retrigger will reset ctrl, video timing is restored to the default
				 * value, does not affect phy, aux read and write
				 */
				inno_dp->chip.dp_hpd_signal_retrigger(&inno_dp->chip);
			}
			goto end_detect;
		}
	}

	inno_dp->hpd_notify = false;

end_detect:
	fh2m_inno_mutex_unlock(inno_dp->aux_mutex);
	return status;
}

static enum drm_connector_status inno_dp_detect(struct drm_connector *connector, bool force)
{
    return inno_dp_detect_ctx(connector, NULL, force);
}

static bool inno_dp_clock_recover(struct dp_device_t *inno_dp)
{
	int i = 0;
	int cdr_delay = 0;
	uint8_t link_config[2];
	uint8_t link_status[DP_LINK_STATUS_SIZE];

	if (inno_dp->chip.dp_training_pattern_set)
		inno_dp->chip.dp_training_pattern_set(&inno_dp->chip, DP_TRAINING_PATTERN_1);

	inno_edp_caps_set(inno_dp);

	/* Write the link configuration data */

	link_config[0] = inno_dp->chip.lane_rate; /* DP_LINK_BW_SET */
	link_config[1] = inno_dp->chip.lane_count | (inno_dp->chip.enhance_mode << 7); /* DP_LANE_COUNT_SET */
	drm_dp_dpcd_write(&inno_dp->aux, DP_LINK_BW_SET, link_config, 2);

	link_config[0] = 0;
	link_config[1] = DP_SET_ANSI_8B10B;
	drm_dp_dpcd_write(&inno_dp->aux, DP_DOWNSPREAD_CTRL, link_config, 2);

	link_config[0] = DP_TRAINING_PATTERN_1; /* DP_TRAINING_PATTERN_SET */
	drm_dp_dpcd_write(&inno_dp->aux, DP_TRAINING_PATTERN_SET, link_config, 1);

	cdr_delay = inno_dp->dpcd[DP_TRAINING_AUX_RD_INTERVAL] & 0x7f;
	if (cdr_delay <= 0) {
		cdr_delay = 4000;
	} else {
		cdr_delay *= 4000; /* us */
	}
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "clock recover delay %d us\n", cdr_delay);

	for (i = 0; i < 5; i++) {

		fh2m_inno_usleep_range(cdr_delay, cdr_delay + 100);

		if (drm_dp_dpcd_read(&inno_dp->aux, DP_LANE0_1_STATUS, link_status,
			DP_LINK_STATUS_SIZE) != DP_LINK_STATUS_SIZE) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "[BAD]failed to get link status\n");
			return false;
		}

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "lane0-1 status:%#.2x lane2-3 status:%#.2x\n",
				link_status[0], link_status[1]);

		if (drm_dp_clock_recovery_ok(link_status, inno_dp->chip.lane_count)) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "CDR Training Succeeded at %d Loop.\n", i + 1);
			break;
		} else {
			inno_dp_get_adjust_train(inno_dp, link_status);
			inno_dp_voltage_swing_adjust(inno_dp);
		}
	}

	return true;
}

static bool inno_dp_channel_balance(struct dp_device_t *inno_dp)
{
	int i = 0;
	int eq_delay = 0;
	uint8_t link_config[2];
	int pattern = DP_TRAINING_PATTERN_2;
	uint8_t link_status[DP_LINK_STATUS_SIZE];

	if (inno_dp->chip.tps3_support)
		pattern = DP_TRAINING_PATTERN_3;

	if (inno_dp->chip.dp_training_pattern_set)
		inno_dp->chip.dp_training_pattern_set(&inno_dp->chip, pattern);

	link_config[0] = pattern; /* DP_TRAINING_PATTERN_SET */
	drm_dp_dpcd_write(&inno_dp->aux, DP_TRAINING_PATTERN_SET, link_config, 1);

	eq_delay = inno_dp->dpcd[DP_TRAINING_AUX_RD_INTERVAL] & 0x7f;
	if (eq_delay <= 0) {
		eq_delay = 4000;
	} else {
		eq_delay *= 4000; /* us */
	}

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "eq delay %d us\n", eq_delay);

	for (i = 0; i < 5; i++) {

		fh2m_inno_usleep_range(eq_delay, eq_delay + 100);

		if (drm_dp_dpcd_read(&inno_dp->aux, DP_LANE0_1_STATUS, link_status,
					DP_LINK_STATUS_SIZE) != DP_LINK_STATUS_SIZE) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "[BAD]failed to get link status\n");
			return false;
		}

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "lane0-1 status:%#.2x lane2-3 status:%#.2x\n",
				link_status[0], link_status[1]);

		if (drm_dp_channel_eq_ok(link_status, inno_dp->chip.lane_count)) {
			fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "EQ Training Succeeded at %d Loop.\n", i + 1);
			break;
		} else {
			inno_dp_get_adjust_train(inno_dp, link_status);
			inno_dp_voltage_swing_adjust(inno_dp);
		}
	}

	return true;
}

static void inno_dp_link_start(struct dp_device_t *inno_dp)
{
	int i = 0;
	uint8_t link_config[9] = {0};

	if (inno_dp->chip.dp_training_pattern_set)
		inno_dp->chip.dp_training_pattern_set(&inno_dp->chip, DP_TRAINING_PATTERN_DISABLE);

	link_config[0] = DP_TRAINING_PATTERN_DISABLE;
	drm_dp_dpcd_write(&inno_dp->aux, DP_TRAINING_PATTERN_SET, link_config, 1);

	fh2m_inno_usleep_range(10000, 11000);

	drm_dp_dpcd_read(&inno_dp->aux, DP_LINK_BW_SET, link_config, sizeof(link_config));

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "Set Rx Lane rate :%#.2x\n", link_config[0]);
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "Set Rx Lane count:%#.2x\n", link_config[1] & DP_LANE_COUNT_MASK);
	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "Set Rx enhanced_frame_en:%#.1x\n",
			(link_config[1] & DP_LANE_COUNT_ENHANCED_FRAME_EN));

	for (i = 0; i < 4; i++) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"Training lane-%d set "
			"voltage_swing:%#.2x max_swing_reached:%d "
			"lane_emphasis :%#.2x max_emphasis_reached:%d\n", i,
			(link_config[i + 3]) & 0x3,
			(link_config[i + 3] >> 2) & 0x1,
			(link_config[i + 3] >> 3) & 0x3,
			(link_config[i + 3] >> 5) & 0x1);
	}

	drm_dp_dpcd_read(&inno_dp->aux, DP_LANE0_1_STATUS, link_config, sizeof(link_config));

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "lane0-1 status:%#.2x lane2-3 status:%#.2x\n",
			link_config[0], link_config[1]);
}

int sink_lane_status_get(struct dp_device_t *inno_dp, u8 status[DP_LINK_STATUS_SIZE])
{
	int ret = 0;

	if (drm_dp_dpcd_read_link_status(&inno_dp->aux, status) == DP_LINK_STATUS_SIZE) {
		ret = DP_AUX_NATIVE_REPLY_ACK;
	} else {
		ret = DP_AUX_NATIVE_REPLY_NACK;
	}

	return ret;
}

static bool inno_dp_need_link_train(struct dp_device_t *inno_dp)
{
	bool ret = false;
	u8 link_status[DP_LINK_STATUS_SIZE];

	if (dp_get_hpg_status(&inno_dp->chip) == INNODP_HPD_OUT) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "dp disconnected\n");
		return ret;
	}

	if (inno_dp->encoder.crtc &&
		inno_dp->encoder.crtc->state &&
		inno_dp->encoder.crtc->state->active) {
		if (sink_lane_status_get(inno_dp, link_status) == 0) {
			if (!drm_dp_clock_recovery_ok(link_status, inno_dp->chip.lane_count) ||
				!drm_dp_channel_eq_ok(link_status, inno_dp->chip.lane_count) ||
				!inno_dp_rate_valid(inno_dp->chip.lane_rate, inno_dp->chip.lane_count,
					inno_dp->current_mode.clock, inno_dp->chip.display_info.bpc)) {
				ret = true;
			}
		}
	} else {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "crtc disable\n");
	}

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "lane_count-%d lane_status-%*ph"
		" mode.clock-%d KHZ link_train-%s\n",
		inno_dp->chip.lane_count, sizeof(link_status), link_status,
		inno_dp->current_mode.clock, ret ? "true" : "false");

	return ret;
}

static void inno_dp_update_audio(struct dp_device_t *inno_dp)
{
	if (!inno_dp || !inno_dp->audio.ac)
		return;

#ifdef CONFIG_DRM_INNO_AUDIO
	if(inno_dp->audio.ac->report_jack) {
		int conn_st, dpms;
		dpms = inno_dp->connector.dpms;
		conn_st = inno_dp->connected;
		inno_dp->audio.ac->has_audio = dp_get_audio_status(&inno_dp->chip);
		inno_dp->audio.ac->update_eld(inno_dp->audio.ac, inno_dp->connector.eld, MAX_ELD_BYTES);
		inno_dp->audio.ac->report_jack(inno_dp->audio.ac, dpms, conn_st);
	}
#endif
}

bool inno_dp_is_skip_replace(const struct drm_display_mode *mode)
{
	struct resolution_info resolution[] = {
		RESOLUTION_INFO_ITEM(3840, 2160, -1, 533250),
		RESOLUTION_INFO_ITEM(3840, 2160, -1, 537600),
		RESOLUTION_INFO_ITEM(3840, 2160, -1, 573410),
		RESOLUTION_INFO_ITEM(3840, 2160, -1, 529910),
	};
	struct resolution_info  *match_resolution = NULL;

	if (!mode)
		return false;

	match_resolution = innodpu_resolution_match((const inno_drm_display_mode *)mode, resolution, INNO_ARRAY_SIZE(resolution));
	if (match_resolution) {
		if (fh2m_hal_get_s_dpu_debug() & DPU_UT_DP) {
			fh2m_inno_printk(KERN_INFO"match resolution(%dx%d@%d %dKHz), skip replace\n",
				match_resolution->hdisplay, match_resolution->vdisplay,
				match_resolution->vrefresh, match_resolution->clock);
		}
		return true;
	}

	return false;
}

static uint8_t inno_dp_autotest_link_training(struct dp_device_t *inno_dp)
{
	int status = 0;
	uint8_t test_link_rate = 0;
	uint8_t test_lane_count = 0;

	/* Read the TEST_LANE_COUNT and TEST_LINK_RTAE fields (DP CTS 3.1.4) */
	status = drm_dp_dpcd_readb(&inno_dp->aux, DP_TEST_LANE_COUNT,
				   &test_lane_count);
	if (status <= 0) {
		dp_info(inno_dp->dev, "lane count read Failed\n");
		return DP_TEST_NAK;
	}

	test_lane_count &= DP_MAX_LANE_COUNT_MASK;

	status = drm_dp_dpcd_readb(&inno_dp->aux, DP_TEST_LINK_RATE,
				   &test_link_rate);
	if (status <= 0) {
		dp_info(inno_dp->dev, "Link Rate read failed\n");
		return DP_TEST_NAK;
	}

	inno_dp->compliance.test_lane_count = test_lane_count;
	inno_dp->compliance.test_link_rate = test_link_rate;

	dp_info(inno_dp->dev, "autotest link rate:%d lane count:%d\n",
		test_link_rate, test_lane_count);

	return DP_TEST_ACK;
}

static int inno_dp_autotest_phy_pattern_set(struct dp_device_t *inno_dp, u8 test_pattern)
{
	int err, i;

	if (inno_dp->dpcd[DP_DPCD_REV] < 0x12) {
		test_pattern = (test_pattern << 2) & DP_LINK_QUAL_PATTERN_11_MASK;
		err = drm_dp_dpcd_writeb(&inno_dp->aux, DP_TRAINING_PATTERN_SET,
					 test_pattern);
		if (err < 0)
			return err;

	} else {
		for (i = 0; i < inno_dp->chip.lane_count; i++) {
			err = drm_dp_dpcd_writeb(&inno_dp->aux,
						 DP_LINK_QUAL_LANE0_SET + i,
						 test_pattern);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static uint8_t inno_dp_autotest_phy_pattern(struct dp_device_t *inno_dp)
{
	int status = 0;
	uint8_t test_pattern = 0;
	uint8_t link_status[DP_LINK_STATUS_SIZE];

	status = drm_dp_dpcd_readb(&inno_dp->aux, INNODP_PHY_TEST_PATTERN,
				   &test_pattern);
	if (status <= 0) {
		dp_info(inno_dp->dev, "Link Rate read failed\n");
		return DP_TEST_NAK;
	}

	inno_dp->compliance.test_data.phy_pattern = test_pattern;
	inno_dp->compliance.test_active = true;
	dp_info(inno_dp->dev, "phy pattern:%d\n", test_pattern);

	if (drm_dp_dpcd_read(&inno_dp->aux, DP_LANE0_1_STATUS, link_status,
		DP_LINK_STATUS_SIZE) != DP_LINK_STATUS_SIZE) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "[BAD]failed to get link status\n");
		return DP_TEST_NAK;
	}

	inno_dp_get_adjust_train(inno_dp, link_status);
	inno_dp_voltage_swing_adjust(inno_dp);

	if (inno_dp->chip.dp_phy_pattern_update)
		inno_dp->chip.dp_phy_pattern_update(&inno_dp->chip, test_pattern);

	inno_dp_autotest_phy_pattern_set(inno_dp, test_pattern);

	return DP_TEST_ACK;
}

static void inno_dp_autotest_request(struct dp_device_t *inno_dp)
{
	uint8_t response = DP_TEST_NAK;
	uint8_t request = 0;
	int status = 0;

	dp_info(inno_dp->dev, "dp test request\n");

	status = drm_dp_dpcd_readb(&inno_dp->aux, DP_TEST_REQUEST, &request);
	if (status <= 0) {
		dp_info(inno_dp->dev, "[BAD]Could not read test request from sink\n");
		goto update_status;
	}

	switch (request) {
	case DP_TEST_LINK_TRAINING:
		dp_info(inno_dp->dev, "LINK_TRAINING test requested\n");
		response = inno_dp_autotest_link_training(inno_dp);
		break;
	case DP_TEST_LINK_VIDEO_PATTERN:
		dp_info(inno_dp->dev, "TEST_PATTERN test requested\n");
		break;
	case DP_TEST_LINK_EDID_READ:
		dp_info(inno_dp->dev, "EDID test requested\n");
		break;
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		dp_info(inno_dp->dev, "PHY_PATTERN test requested\n");
		response = inno_dp_autotest_phy_pattern(inno_dp);
		break;
	default:
		dp_info(inno_dp->dev, "Invalid test request '%02x'\n", request);
		break;
	}

	if (response & DP_TEST_ACK)
		inno_dp->compliance.test_type = request;

update_status:
	status = drm_dp_dpcd_writeb(&inno_dp->aux, DP_TEST_RESPONSE, response);
	if (status <= 0)
		dp_info(inno_dp->dev, "[BAD]Could not write test request from sink\n");
}

static void inno_dp_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode);
static void inno_dp_encoder_mode_enable(struct drm_encoder *encoder);

static void inno_dp_device_service_irq(struct dp_device_t *inno_dp)
{
	u8 sink_irq_vector = 0;

	/* Try to read the source of the interrupt */
	if (inno_dp->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (drm_dp_dpcd_readb(&inno_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, &sink_irq_vector) != 1 || !sink_irq_vector)
		return;

	/* Clear interrupt source */
	drm_dp_dpcd_writeb(&inno_dp->aux,
			   DP_DEVICE_SERVICE_IRQ_VECTOR,
			   sink_irq_vector);

	if (sink_irq_vector & DP_AUTOMATED_TEST_REQUEST)
		inno_dp_autotest_request(inno_dp);
	if (sink_irq_vector & (DP_CP_IRQ | DP_SINK_SPECIFIC_IRQ))
		dp_info(inno_dp->dev, "CP or sink specific irq unhandled\n");
}

static void inno_dp_link_service_irq(struct dp_device_t *inno_dp)
{
	u8 val = 0;

	if (inno_dp->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (drm_dp_dpcd_readb(&inno_dp->aux,
		DP_LINK_SERVICE_IRQ_VECTOR_ESI0, &val) != 1 || !val)
		return;

	if (drm_dp_dpcd_writeb(&inno_dp->aux,
		DP_LINK_SERVICE_IRQ_VECTOR_ESI0, val) != 1)
		return;

	if (val & INNODP_RX_CAP_CHANGED) {
		dp_info(inno_dp->dev, "DP_RX_CAP_CHANGE\n");
	}

	if (val & INNODP_LINK_STATUS_CHANGED) {
		dp_info(inno_dp->dev, "DP_LINK_STATUS_CHANGE\n");
	}

	if (val & INNODP_STREAM_STATUS_CHANGED) {
		dp_info(inno_dp->dev, "DP_STREAM_STATUS_CHANGED\n");
	}

	if (val & INNODP_HDMI_LINK_STATUS_CHANGED) {
		dp_info(inno_dp->dev, "DP_HDMI_LINK_STATUS_CHANGED");
	}
}

static bool inno_dp_short_pulse(struct dp_device_t *inno_dp)
{
	struct drm_display_mode tmp_mode = {
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
				  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
				  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};
	struct drm_display_mode *mode = NULL;

	if (!inno_dp->connected)
		return false;

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);

	inno_dp_device_service_irq(inno_dp);
	inno_dp_link_service_irq(inno_dp);

	if (inno_dp->compliance.test_type == DP_TEST_LINK_TRAINING) {
		dp_info(inno_dp->dev, "Link Training Compliance Test requested, start modeset\n");
		/* start modeset */
		if (inno_dp->current_mode.clock > 0)
			mode = &inno_dp->current_mode;
		else
			mode = &tmp_mode;

		inno_dp_encoder_mode_set(&inno_dp->encoder, NULL, mode);
		fh2m_inno_mutex_unlock(inno_dp->aux_mutex);

		inno_dp_encoder_mode_enable(&inno_dp->encoder);
	} else {
		fh2m_inno_mutex_unlock(inno_dp->aux_mutex);
	}

	return true;
}

static void inno_dp_long_pulse(struct dp_device_t *inno_dp)
{
	int dpms = READ_ONCE(inno_dp->connector.dpms);
	int drm_power_state = DRM_SWITCH_POWER_ON;
	bool changed = false;

	if (inno_dp->drm_dev)
		drm_power_state = inno_dp->drm_dev->switch_power_state;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"long pulse [drm_power_state=%d][dpms=%d][old_connector:%d][crtc:%s]\n",
			drm_power_state, dpms, inno_dp->connector.status,
			inno_dp->encoder.crtc ? "true" : "fales");

	/* 1. If the hot-plugging is triggered during the wake-up process of s4 and s3,
	 * at this time, the resume of drm has not been called, then ignore the report uevent.
	 * */
	if (drm_power_state == DRM_SWITCH_POWER_ON) {
		changed = fh2m_inno_dp_drm_helper_hpd_irq_event(inno_dp->drm_dev);
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
				"[hpd work]callback drm_helper_hpd_irq_event,"
				"uevent_report:%s\n", changed ? "true" : "false");
#ifdef CONFIG_DRM_INNO_AUDIO
		if (!inno_dp->connected)
			innodpu_connector_clear_eld(&inno_dp->connector);
#endif
	}
}

static void inno_dp_hpd_work(struct work_struct *work)
{
	struct delayed_work *hpd_work = to_delayed_work(work);
	struct dp_device_t *inno_dp = container_of(hpd_work, struct dp_device_t, hpd_work);
	struct dp_chip_t *chip = &inno_dp->chip;
	long long irq_status = 0;
	bool ret = false;

	irq_status = dp_get_hpg_status(chip);

	fh2m_innodpu_info(chip->dev, DPU_UT_DP,"dp hpd work\n");

	dp_set_poll_cnt(inno_dp, 0);
	cancel_delayed_work_sync(&inno_dp->dp_poll_work);

	inno_dp_check_sink_connection(inno_dp);

	/*
	 * Clearing compliance test variables to allow capturing
	 * of values for next automated test request.
	 */
	memset(&inno_dp->compliance, 0, sizeof(inno_dp->compliance));

	if (irq_status == INNODP_HPD_OUT)
		inno_dp_clear_edid_info(chip);

	if (irq_status & INNODP_HPD_IRQ)
		ret = inno_dp_short_pulse(inno_dp);

	if (!(irq_status & INNODP_HPD_IRQ) || !ret)
		inno_dp_long_pulse(inno_dp);

	if (inno_dp->compliance.test_type == DP_TEST_LINK_TRAINING && ret)
		return;

	/*If the crtc bound by the current encoder is active after generation the unplug event,
	 * the display link status is read, and of the link loses connection, the link capacity of
	 * the display is compared before and after the unplug, and if the capacity is the same,
	 * the link interaction is carried out to avoid the  upper layer not calling se_crtc resulting
	 * in the inability to display.*/
	if (inno_dp->encoder.crtc &&
		inno_dp->encoder.crtc->state &&
		inno_dp->encoder.crtc->state->active &&
		irq_status != INNODP_HPD_OUT) {
		dp_set_poll_cnt(inno_dp, 3);
		queue_delayed_work(inno_dp->poll_wq, &inno_dp->dp_poll_work, 0);
	}
}

static s32 inno_dp_get_edid_block(void *data, u8 * buf, u32 block, size_t len)
{
	struct dp_chip_t *chip = (struct dp_chip_t*)data;

	if (len > INNODP_EDID_BUF_LEN / 2) {
		return -EINVAL;
	}

	if (block % 2 == 0) {
		fh2m_inno_memcpy(buf, chip->edid_buf, len);
	} else {
		fh2m_inno_memcpy(buf, chip->edid_buf + INNODP_EDID_BUF_LEN / 2, len);
	}

	return 0;
}

static int inno_dp_get_modes_strpush(struct drm_connector *connector)
{
	struct dp_device_t *dp_dev = to_dp_device(connector);
	struct dp_chip_t *chip = &dp_dev->chip;
	int ret = 0;

	ret = fh2m_hal_dp_edid_data(fh2m_inno_dev_get_parent(chip->dev),
			dp_dev->dp_id, chip->edid_buf);
	if (ret) {
		fh2m_innodpu_err(chip->dev, "strpush edid error, ret:%d\n", ret);
		return -EFAULT;
	}

	chip->modes = innodpu_str_push_edid(chip->edid_buf, connector);

	return ret;
}

static int inno_dp_get_edid_user(struct drm_connector *connector)
{
	struct dp_device_t *dp_dev = to_dp_device(connector);
	struct dp_chip_t *chip = &dp_dev->chip;
	int ret = 0;
	int i = 0;

	ret = fh2m_hal_dp_edid_data(fh2m_inno_dev_get_parent(chip->dev),
			dp_dev->dp_id, chip->edid_buf);
	if (ret) {
		fh2m_innodpu_err(chip->dev, "get edid from user error, ret:%d\n", ret);
		return -EFAULT;
	}

	for (i = 0; i <= chip->edid_buf[0x7e] && i <= 1; i++) {
		if (!drm_edid_block_valid(chip->edid_buf + i * INNODP_EDID_BUF_LEN / 2, i, false, NULL)) {
			ret = -EFAULT;
			break;
		}
	}

	return ret;
}

int inno_dp_get_edid_monitor(struct drm_connector *connector)
{
	u8 offset = 0;
	struct dp_device_t *dp_dev = to_dp_device(connector);
	int block_valid = 0, retry = 0, ret = 0, i = 0, extension = 0;
	struct i2c_msg msgs[] = {
		{DDC_EDID_ADDR, 0, 1, &offset},
		{DDC_EDID_ADDR, I2C_M_RD, INNODP_EDID_BUF_LEN, dp_dev->chip.edid_buf},
	};

	fh2m_inno_mutex_lock(dp_dev->aux_mutex);

	for (retry = 0; retry < 4; retry++) {
		fh2m_inno_memset(dp_dev->chip.edid_buf, 0, sizeof(dp_dev->chip.edid_buf));
		if (i2c_transfer(&dp_dev->aux.ddc, msgs, INNO_ARRAY_SIZE(msgs)) == INNO_ARRAY_SIZE(msgs)) {
			block_valid = 0;
			extension = (dp_dev->chip.edid_buf[0x7e] >= 1) ? 1 : 0;
			for (i = 0; i <= extension; i++) {
				if (drm_edid_block_valid(dp_dev->chip.edid_buf + i * INNODP_EDID_BUF_LEN / 2, i, false, NULL))
					block_valid++;
				else {
					fh2m_innodpu_info(dp_dev->dev, DPU_UT_DP, "read edid failed!"
							"retry:%d block[%d]\n", retry, i);
					if (fh2m_hal_get_s_dpu_debug() & DPU_UT_DP) {
						fh2m_inno_print_hex_dump(KERN_NOTICE, " \t", 0, 16, 1,
								dp_dev->chip.edid_buf, INNODP_EDID_BUF_LEN, false);
					}
					ret = -EFAULT;
					break;
				}
			}

			if (block_valid == extension + 1) {
				fh2m_innodpu_info(dp_dev->dev, DPU_UT_DP, "read edid success!retry=%d\n", retry);
				ret = 0;
				break;
			}
		}
	}

	fh2m_inno_mutex_unlock(dp_dev->aux_mutex);

	return ret;
}

static int inno_dp_fixup_auo_monitor_quirk(struct edid *edid)
{
	int i;
	u8 sum = 0;
	int tmp = -1;
	int dtd = -1;
	u8 vblank_lo[4] = {0,};
	u8 vactive_vblank_hi[4] = {0,};
	struct detailed_timing *timings[4];
	struct detailed_pixel_timing *pixel_data = NULL;

	for (i = 0; i < 4; i++) {
		timings[i] = &(edid->detailed_timings[i]);
		pixel_data = &timings[i]->data.pixel_data;

		if (timings[i]->pixel_clock == 0)
			continue;

		vblank_lo[i] = pixel_data->vblank_lo;
		vactive_vblank_hi[i] = pixel_data->vactive_vblank_hi;
	}

	for (i = 0; i < 4; i++) {
		if (vblank_lo[i] == 0 && vactive_vblank_hi[i] == 0) {
			if (dtd != -1)
				continue;
			else
				dtd = i;
			continue;
		}

		if (vblank_lo[i] == 0x46 && vactive_vblank_hi[i] == 0x60)
			tmp = i;
	}

	if (tmp == -1)
		return tmp;
	if (dtd == -1)
		return dtd;

	memcpy(timings[dtd], timings[tmp], sizeof(struct detailed_timing));
	timings[dtd]->data.pixel_data.vblank_lo = 0x46;
	timings[dtd]->data.pixel_data.vactive_vblank_hi = 0x60;
	timings[dtd]->pixel_clock = 27275;

	for (i = 0; i < EDID_LENGTH; i++)
		sum += ((u8 *)edid)[i];

	if (sum) {
		if (sum <= edid->checksum)
			edid->checksum -= sum;
		else
			edid->checksum += 256 - sum;
	}

	return 0;
}

static int inno_dp_get_edid(struct drm_connector *connector, int edid_mode)
{
	struct dp_device_t *dp_dev =  to_dp_device(connector);
	struct dp_chip_t *chip = &dp_dev->chip;
	int ret = 0;

	fh2m_inno_memset(chip->edid_buf, 0, sizeof(chip->edid_buf));

	switch (edid_mode) {
	case EDID_AUTO_READ:
		ret = inno_dp_get_edid_monitor(connector);
	break;
	case EDID_STR_PUSH:
		ret = inno_dp_get_modes_strpush(connector);
	break;
	case EDID_USER_DEFINE:
		ret = inno_dp_get_edid_user(connector);
	break;
	default:
		fh2m_innodpu_err(dp_dev->dev, "get_edid failed, invalid edid_mode,"
				"read edid from monitor\n");
		edid_mode = EDID_AUTO_READ;
		ret = -EFAULT;
	}

	{
		struct mfc_monitor_info monitor = MFC_MONITOR_INFO_ITEM("AUO", "*");

		if (innodpu_is_mfc_monitor_match((unsigned char *)chip->edid_buf, &monitor)) {
			fh2m_innodpu_info(dp_dev->dev, DPU_UT_DP, "AUO monitor, try to deal with quirk\n");
			ret = inno_dp_fixup_auo_monitor_quirk((struct edid *)chip->edid_buf);
			if (ret)
				fh2m_innodpu_err(dp_dev->dev, "Handling AUO monitor quirk failed.\n");
		}
	}

	fh2m_innodpu_info(dp_dev->dev, DPU_UT_DP, "edid_mode:%d ret = %d\n",
			edid_mode, ret);

	if (ret == 0 && edid_mode != EDID_STR_PUSH) {
		dp_set_edid(chip, drm_do_get_edid(connector, inno_dp_get_edid_block, chip));
		chip->max_pclk_rx = innodpu_conn_get_monitor_max_clk((u8 *)chip->edid_buf);
		if (chip->max_pclk_rx <= 0) {
			chip->max_pclk_rx = 605000;
			fh2m_innodpu_info(dp_dev->dev, DPU_UT_DP, "[BAD]get dp max clock err, set it to 605MHz\n");
		}
	}

	if (ret || (edid_mode != EDID_STR_PUSH && dp_get_edid(chip) == NULL)) {
		if (dp_get_edid(chip)) {
			fh2m_inno_kfree(dp_get_edid(chip));
			dp_set_edid(chip, NULL);
		}
		if (ret != DP_AUX_NATIVE_REPLY_NACK && memchr_inv(chip->edid_buf, 0, INNODP_EDID_BUF_LEN))
			fh2m_innodpu_err(dp_dev->dev, "[%s] Edid Invalid, may be poor contact, please re-plug\n", connector->name);
		drm_connector_update_edid_property(connector, NULL);
		dp_set_audio_status(&dp_dev->chip, 0);
		dp_dev->chip.modes = innodpu_add_modes_without_edid(connector, NULL);
	} else if (edid_mode != EDID_STR_PUSH) {
		drm_connector_update_edid_property(connector, dp_get_edid(&dp_dev->chip));
		dp_set_audio_status(&dp_dev->chip, drm_detect_monitor_audio(dp_get_edid(&dp_dev->chip)));
		dp_dev->chip.modes = drm_add_edid_modes(connector, dp_get_edid(&dp_dev->chip));
	}

	return ret;
}

static bool inno_dp_encoder_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	struct dp_device_t *inno_dp = to_dp_device(encoder);
	const struct drm_display_mode *rmode = NULL;

	if (is_native_mode_valid(&inno_dp->native_mode) && is_virtual_mode(&inno_dp->native_mode, adjusted_mode)) {
		drm_mode_copy(adjusted_mode, &inno_dp->native_mode);
	}

	if (inno_dp->chip.replace_timing) {
		rmode = innodpu_modes_match_replace_table(adjusted_mode, NULL, inno_dp_is_skip_replace);
		if (!rmode)
			goto out;

		if(!is_special_mode(rmode)) {
			innodpu_modes_replace_timing(adjusted_mode, rmode);
		}

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_HDMI,"%s fixup: "DRM_MODE_FMT "\n",
			inno_dp->name, DRM_MODE_ARG(adjusted_mode));
	}

out:
	return true;
}

static void inno_dp_source_link_init(struct dp_device_t *inno_dp)
{
	if (fh2m_inno_is_err_or_null(inno_dp->chip.dp_source_link_set))
		return;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "source caps init\n");

	/* Initialise the number of links on the source side,
	 * the link rate, the average effective symbol transmission unit.
	 */
	inno_dp->chip.dp_source_link_set(&inno_dp->chip, &inno_dp->current_mode);
}

static void inno_dp_poll_execute(struct work_struct *work)
{
	u8 link_cfg[INNODP_LINK_CFG_SIZE];
	u8 link_status[DP_LINK_STATUS_SIZE];
	struct delayed_work *dp_poll_work = to_delayed_work(work);
	struct dp_device_t *inno_dp = to_dp_device(dp_poll_work);

	if (!inno_dp)
		return;

	/* if has panel(backlight), Does not retraining */
	if (inno_dp->panel || inno_dp->compliance.test_type) {
		dp_set_poll_cnt(inno_dp, 0);
		return;
	}

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);

	if (dp_get_poll_cnt(inno_dp) <= 0)
		goto unlock;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "sink status poll[%d]\n", dp_get_poll_cnt(inno_dp));

	if (inno_dp_need_link_train(inno_dp)) {
		if (inno_dp->chip.dp_source_link_get) {
			inno_dp->chip.dp_source_link_get(&inno_dp->chip, link_cfg);
			inno_dp_caps_prepare(inno_dp);
			inno_dp_rate_adjust(inno_dp, dp_get_poll_cnt(inno_dp) <= 2);
			if (link_cfg[INNODP_LINK_COUNT] != inno_dp->chip.lane_count ||
				link_cfg[INNODP_LINK_RATE]  != inno_dp->chip.lane_rate ||
				dp_get_poll_cnt(inno_dp) <= 2) {
				inno_dp_source_link_init(inno_dp);
			}
		}

		if (inno_dp_rate_valid(inno_dp->chip.lane_rate, inno_dp->chip.lane_count,
			inno_dp->current_mode.clock, inno_dp->chip.display_info.bpc)) {
			if (!inno_dp_clock_recover(inno_dp))
				goto fail_handle;
			if (!inno_dp_channel_balance(inno_dp))
				goto fail_handle;
			inno_dp_link_start(inno_dp);
			if (inno_dp_need_link_train(inno_dp) && (dp_get_poll_cnt(inno_dp) == 2) &&
				memchr_inv(inno_dp->chip.edid_buf, 0, INNODP_EDID_BUF_LEN)) {
				fh2m_innodpu_err(inno_dp->chip.dev, "Link invalid!\n");
			}
			inno_dp_update_audio(inno_dp);
		}
	}

fail_handle:
	dp_set_poll_cnt(inno_dp, dp_get_poll_cnt(inno_dp) - 1);
	if (dp_get_poll_cnt(inno_dp) >= 1) {
		queue_delayed_work(inno_dp->poll_wq, &inno_dp->dp_poll_work, fh2m_inno_msecs_to_jiffies(DP_POLL_PERIOD_MS));
	} else if (inno_dp_monitor_en(dp_link_monitor) && (sink_lane_status_get(inno_dp, link_status) != DP_AUX_NATIVE_REPLY_NACK)) {
		dp_set_poll_cnt(inno_dp, 1);
		queue_delayed_work(inno_dp->poll_wq, &inno_dp->dp_poll_work, fh2m_inno_msecs_to_jiffies(DP_POLL_PERIOD_MS * 200));
	}

unlock:
	fh2m_inno_mutex_unlock(inno_dp->aux_mutex);
}

static void inno_dp_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	int dpu_id = 0;
	struct dp_device_t *inno_dp = to_dp_device(encoder);

	if (!inno_dp || !adjusted_mode)
		return;

	dpu_id = innodpu_get_dpuid_bycrtc(encoder->crtc);

	drm_mode_copy(&inno_dp->current_mode, adjusted_mode);

	inno_dp_caps_prepare(inno_dp);

	inno_dp_video_timing_prepare(inno_dp);

	if (inno_dp->chip.encoder_modeset) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,"%s modeset: "DRM_MODE_FMT "\n",
			inno_dp->name, DRM_MODE_ARG(adjusted_mode));
		inno_dp->chip.encoder_modeset(&inno_dp->chip, dpu_id, false, adjusted_mode);
	}
}

#ifdef CONFIG_DRM_INNO_AUDIO
static void audio_infoframe_init(struct dp_device_t *inno_dp)
{
	u8 ct = 0;
	u8 cc = 0;
	u8 sf = 0;
	u8 ss = 0;
	int ss_map[] = {0, 16, 20, 24}; /* sample size map */
	int sf_map[] = {0, 32000, 44100, 48000, 88200, 96000, 176400, 192000};/* sample frequency:HZ */

	if (!inno_dp || !inno_dp->audio.ac)
		return;

	fh2m_inno_memset(&inno_dp->audio.sdp, 0, sizeof(struct innodp_sdp_format));

	inno_dp->audio.sdp.sdp_header.HB0 = 0x0;
	inno_dp->audio.sdp.sdp_header.HB1 = 0x84; /* Audio Infoframe Type = 0x4 */
	inno_dp->audio.sdp.sdp_header.HB2 = 0x1b; /* Date Byte = 28 byte */
	inno_dp->audio.sdp.sdp_header.HB3 = (0x12 << 2);
	inno_dp->audio.channels = inno_dp->audio.ac->data.channels;
	inno_dp->audio.sample_bits = inno_dp->audio.ac->data.sample_bits;
	inno_dp->audio.sample_rate = inno_dp->audio.ac->data.rate;

	if (inno_dp->audio.sample_rate == 0) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "force sample_rate 44.1Khz\n");
		inno_dp->audio.sample_rate = 44100;
	}

	if (inno_dp->audio.channels != 2) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "force 2 channels\n");
		inno_dp->audio.channels = 2;
	}

	/* Audio Coding Type:L-PCM Coding */
	/* Audio Stream Encoding Standard: IEC 60985-3 */
	ct = 0x01;

	/* sample size */
	ss = inno_dp_rate_index(ss_map, INNO_ARRAY_SIZE(ss_map),
		inno_dp->audio.ac->data.sample_bits);
	if (ss < 0) {
		fh2m_innodpu_err(inno_dp->dev, "invalid ss parameters\n");
		return;
	}

	if (inno_dp_monitor_en(audio_infoframe_dm_init)) {

		/* Audio Channel Count:
		 * 0x1: 2 channel, 0x2: 3 channel, 0x3: 4 channel
		 * 0x4: 5 channel, 0x5: 6 channel, 0x6: 7 channel
		 * 0x7: 8 channel */
		cc = inno_dp->audio.channels - 1;

		/* sampling Frequency */
		sf = inno_dp_rate_index(sf_map, INNO_ARRAY_SIZE(sf_map),
			inno_dp->audio.sample_rate);
		if (sf < 0) {
			fh2m_innodpu_err(inno_dp->dev, "invalid sf parameters\n");
			return;
		}
	} else {
		cc = 0x01; /* 2 channel */
		sf = 0x02; /* sampling frequency: 44.1Khz */
	}

	inno_dp->audio.sdp.valid = true;
	inno_dp->audio.sdp.DB0 = (ct << 4) | cc;
	inno_dp->audio.sdp.DB1 = (sf << 2) | ss;
	inno_dp->audio.sdp.DB2 = 0x00;
	inno_dp->audio.sdp.DB3 = 0x13; /* audio channel allocation/speaker placement */

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
		"[audio info] channels:%d sample_bits:%d sampling frequency:%d "
		"ct:%d cc:%d sf:%d ss:%d\n",
		inno_dp->audio.channels,
		inno_dp->audio.sample_bits,
		inno_dp->audio.sample_rate,
		ct, cc, sf, ss);
}

static void inno_dp_audio_enable(struct audio_conn *ac)
{
	struct dp_device_t *inno_dp = NULL;

	if (!ac || !ac->priv) {
		return;
	}

	inno_dp = (struct dp_device_t *)ac->priv;

	if (inno_dp->chip.dp_audio_enable &&
		dp_get_audio_status(&inno_dp->chip)) {
		inno_dp->audio.enable = true;
		audio_infoframe_init(inno_dp);
		inno_dp->chip.dp_audio_enable(&inno_dp->chip, &inno_dp->audio);

		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "audio enable\n");
	}
}

static void inno_dp_audio_disable(struct audio_conn *ac)
{
	struct dp_device_t *inno_dp = NULL;

	if (!ac || !ac->priv) {
		return;
	}

	inno_dp = (struct dp_device_t *)ac->priv;

	if (!inno_dp->audio.enable)
		return;

	if (inno_dp->chip.dp_audio_disable) {
		inno_dp->audio.enable = false;
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "audio disable\n");
		inno_dp->chip.dp_audio_disable(&inno_dp->chip);
	}
}

#else
static inline void inno_dp_audio_enable(struct audio_conn *ac){}
static inline void inno_dp_audio_disable(struct audio_conn *ac){}
#endif

static void inno_dp_encoder_mode_disable(struct drm_encoder *encoder)
{
	struct dp_device_t *inno_dp = to_dp_device(encoder);

	if (fh2m_inno_is_err_or_null(inno_dp))
		return;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"inno %s encoder mode disable\n",inno_dp->name);

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);

	inno_dp_sink_power_ctrl(inno_dp, false);

	/* Backlight off */
	inno_panel_disable(inno_dp->panel);

	/* Source
	 * 1. Main Tx disabled
	 * */
	if (inno_dp->chip.encoder_disable)
		inno_dp->chip.encoder_disable(&inno_dp->chip);

	/*notification of audio devices */
	inno_dp_update_audio(inno_dp);

	dp_set_poll_cnt(inno_dp, 0);
	fh2m_inno_mutex_unlock(inno_dp->aux_mutex);

	cancel_delayed_work_sync(&inno_dp->dp_poll_work);

	/* Switch off the power */
	inno_panel_unprepare(inno_dp->panel);
	if (inno_dp->connector.connector_type == DRM_MODE_CONNECTOR_eDP) {
		fh2m_inno_msleep(500);
	}
}

static void inno_dp_encoder_mode_enable(struct drm_encoder *encoder)
{
	struct dp_device_t *inno_dp = to_dp_device(encoder);

	if (fh2m_inno_is_err_or_null(inno_dp))
		return;

	fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"inno %s encoder mode enable\n", inno_dp->name);

	fh2m_inno_mutex_lock(inno_dp->aux_mutex);
	inno_dp_sink_power_ctrl(inno_dp, true);
	inno_dp_clock_recover(inno_dp);
	inno_dp_channel_balance(inno_dp);

	/*Enable video stream-dp_device*/
	if (inno_dp->chip.encoder_enable)
		inno_dp->chip.encoder_enable(&inno_dp->chip);
	if (inno_dp->is_R1) {
		inno_panel_enable(inno_dp->panel);
	}
	inno_dp_link_start(inno_dp);

	if (inno_dp->audio.enable)
		inno_dp_audio_enable(inno_dp->audio.ac);

	inno_dp_update_audio(inno_dp);

	fh2m_inno_mutex_unlock(inno_dp->aux_mutex);

	inno_panel_enable(inno_dp->panel);

	dp_set_poll_cnt(inno_dp, 5);
	queue_delayed_work(inno_dp->poll_wq, &inno_dp->dp_poll_work,
			fh2m_inno_msecs_to_jiffies(DP_POLL_PERIOD_MS));
}

static void inno_dp_encoder_destroy(struct drm_encoder *encoder)
{
	if (fh2m_inno_is_err_or_null(encoder))
		return;

	drm_encoder_cleanup(encoder);
}

static void inno_dp_connector_destroy(struct drm_connector *connector)
{
	if (fh2m_inno_is_err_or_null(connector))
		return;

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static void inno_dp_cleanup_connector_encoder(
			struct dp_device_t *inno_dp)
{
	/*
	 * After the dp reference count is zeroed, drm_connector_free will be called
	 * encoder autorelease function without reference counting
	 */
	inno_dp_connector_destroy(&inno_dp->connector);
	inno_dp_encoder_destroy(&inno_dp->encoder);
}

static enum drm_mode_status inno_dp_connector_helper_mode_valid(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	inno_drm_mode_status status = MODE_OK;
	struct dp_device_t *inno_dp = to_dp_device(connector);

	if (fh2m_inno_is_err_or_null(inno_dp))
		return MODE_OK;

	if (!inno_dp_rate_valid(inno_dp->chip.max_source_rate, 4, mode->clock, inno_dp_bpc[1])) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
				"[BAD]clock required is too high,""[%dx%d clock:%d]kHZ\n",
				mode->hdisplay, mode->vdisplay, mode->clock);
		return MODE_CLOCK_HIGH;
	}

	if (inno_dp->chip.connector_mode_valid)
		status = inno_dp->chip.connector_mode_valid(&inno_dp->chip, mode);

	if (status != MODE_OK) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP,
			"%s connector mode invalid: %dx%d@%d valid:%d",
			inno_dp->name, mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode), status);
	}

	return status;
}

static bool inno_dp_native_mode_filter(struct drm_connector *connector,
	struct drm_display_mode *mode, bool scaling_filter)
{
	struct dp_device_t *inno_dp = to_dp_device(connector);
#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	struct drm_encoder *encoder = &inno_dp->encoder;
#endif
	bool combi_en = true;
	int ret = 0;

	if (!mode)
		return true;

	combi_en = inno_dp->chip.combi_en;
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

	if (inno_dp->chip.max_pclk_rx > 0 && mode->clock > inno_dp->chip.max_pclk_rx)
		return true;

	if (connector->helper_private && connector->helper_private->mode_valid) {
		ret = connector->helper_private->mode_valid(&inno_dp->connector, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}

#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	if (encoder->helper_private && encoder->helper_private->mode_valid) {
		ret = encoder->helper_private->mode_valid(&inno_dp->encoder, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}
#endif

	return false;
}

static void inno_dp_common_mode_add(struct drm_connector * connector)
{
	struct drm_display_mode *native_mode = NULL;
	struct dp_device_t *inno_dp = to_dp_device(connector);

	if (fh2m_inno_is_err_or_null(inno_dp))
		return;

	// get native mode
	drm_mode_sort(&connector->probed_modes);
	native_mode = innodpu_get_native_mode(connector, inno_dp_native_mode_filter);

	// add common mode
	if (native_mode) {
		fh2m_innodpu_info(inno_dp->dev, DPU_UT_DP, "%s native mode: "DRM_MODE_FMT ", status:%d\n",
			inno_dp->name, DRM_MODE_ARG(native_mode), native_mode->status);
		fh2m_inno_memcpy(&inno_dp->native_mode, native_mode, sizeof(inno_dp->native_mode));
		drm_mode_set_crtcinfo(&inno_dp->native_mode, CRTC_INTERLACE_HALVE_V);
		inno_dp->native_mode.status = MODE_OK;
		inno_dp->chip.modes += innodpu_connector_add_common_modes(connector, &inno_dp->native_mode, false);
	} else {
		fh2m_inno_memset(&inno_dp->native_mode, 0, sizeof(inno_dp->native_mode));
	}
}

static int inno_dp_connector_helper_get_modes(struct drm_connector *connector)
{
	struct dp_device_t *inno_dp = to_dp_device(connector);

	if (fh2m_inno_is_err_or_null(inno_dp))
		return 0;

	if (dp_get_edid(&inno_dp->chip)) {
		drm_connector_update_edid_property(connector, dp_get_edid(&inno_dp->chip));
		dp_set_audio_status(&inno_dp->chip, drm_detect_monitor_audio(dp_get_edid(&inno_dp->chip)));
		inno_dp->chip.modes = drm_add_edid_modes(connector, dp_get_edid(&inno_dp->chip));
	}

	if (!dp_get_edid(&inno_dp->chip) || (inno_dp->chip.hal_edid_mode == EDID_STR_PUSH))
		inno_dp_get_edid(connector, inno_dp->chip.hal_edid_mode);

	/*
	 * Keep a known-good mode available even when EDID exists. On FH2M the
	 * internal AUO panel can expose EDID but still leave the DRM connector
	 * mode list empty during fbdev setup, which prevents fbcon/tty1 from appearing.
	 */
	if (connector->status == connector_status_connected)
		inno_dp->chip.modes += innodpu_add_modes_without_edid(connector, NULL);

#if (DRM_VERSION <= KERNEL_VERSION(4, 16, 0))
	drm_edid_to_eld(connector, dp_get_edid(&inno_dp->chip));
#endif

	inno_dp->chip.modes -= innodpu_modes_drop_repeat(connector);

	// fixup 1366x768 and more modes for /dev/fb0
	innodpu_modes_fixup_preferred_nonaligned_modes(connector);

	if (fh2m_hal_get_s_dpu_debug() & DPU_UT_DP) {
		fh2m_inno_print_hex_dump(KERN_NOTICE, " \t", 0, 16, 1,
			inno_dp->chip.edid_buf, INNODP_EDID_BUF_LEN, false);
	}

	inno_dp_common_mode_add(connector);

	return inno_dp->chip.modes;
}

static ssize_t
inno_dp_aux_transfer(struct drm_dp_aux *aux, struct drm_dp_aux_msg *msg)
{
	int ret = 0, j = 0;
	struct aux_cfg *aux_vcp = NULL;
	struct dp_device_t *dp = to_dp_device(aux);

	if (fh2m_inno_is_err_or_null(dp))
		return -EFAULT;

	aux_vcp = fh2m_inno_kzalloc_kernel(sizeof(struct aux_cfg));
	if (fh2m_inno_is_err_or_null(aux_vcp))
		return -ENOMEM;

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	{
		aux_vcp->aux_cmd  = msg->request;
		aux_vcp->dpcd_addr = msg->address;
		aux_vcp->length = msg->size ? (msg->size - 1) : 0x10;
		aux_vcp->read  = 0;
		if (msg->size) {
			for (j = 0; j < msg->size; j++)
				aux_vcp->wr_buff[j / 4] |= (((u8 *)msg->buffer)[j] << ((j % 4) * 8));
		}

		ret |= inno_dp_aux_rw(dp, aux_vcp);
	}
	break;
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ:
	{
		aux_vcp->aux_cmd = msg->request;
		aux_vcp->dpcd_addr = msg->address;
		aux_vcp->length = msg->size ? (msg->size - 1) : 0x10;
		aux_vcp->read = 1;
		ret |= inno_dp_aux_rw(dp, aux_vcp);
		for (j = 0; j < msg->size; j++)
			((u8 *)msg->buffer)[j] = (aux_vcp->rd_buff[j / 4] >> (8 *(j % 4))) & 0xff;
	}
	break;
	default:
		ret = -EINVAL;
		break;
	}

	msg->reply = ret;

	if (msg->reply & DP_AUX_NATIVE_REPLY_DEFER ||
		msg->reply & DP_AUX_I2C_REPLY_DEFER) {
		dp_info(dp->dev, "[Note]Aux Defer:%d\n", msg->reply);
	}

	fh2m_inno_kfree(aux_vcp);

	return msg->size;
}

static const struct drm_connector_funcs s_innodp_connector_funcs = {
#if ((DRM_VERSION <= KERNEL_VERSION(4, 13, 0)))
	.dpms       = drm_atomic_helper_connector_dpms,
#else
	.dpms       = drm_helper_connector_dpms,
#endif
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = inno_dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = inno_dp_late_register,
	.early_unregister = inno_dp_early_unregister,
	.detect = inno_dp_detect,
};

static struct drm_connector_helper_funcs s_innodp_connector_helper_funcs = {
	.get_modes = inno_dp_connector_helper_get_modes,
	.mode_valid = inno_dp_connector_helper_mode_valid,
#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	.detect_ctx = inno_dp_detect_ctx,
#endif
};

static const struct drm_encoder_helper_funcs s_innodp_encoder_helper_funcs = {
	.mode_fixup = inno_dp_encoder_mode_fixup,
	.mode_set = inno_dp_encoder_mode_set,
	.disable = inno_dp_encoder_mode_disable,
	.enable = inno_dp_encoder_mode_enable,
};

static const struct drm_encoder_funcs s_innodp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int inno_dp_connector_create(struct drm_device *dev, struct drm_connector *connector)
{
	int connector_type = 0;
	struct dp_device_t * dp_dev = to_dp_device(connector);

	if (NULL == dev || NULL == connector) {
		DRM_ERROR("Invalid argument: the pointer of dev is %p, connector is %p!\n", dev, connector);
		return -EFAULT;
	}

	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	if (dp_dev->chip.output_mode) {
		connector_type = dp_dev->chip.output_mode->mode_connector_item;
	} else {
		connector_type = DRM_MODE_CONNECTOR_DisplayPort;
	}

	drm_connector_init(dev, connector, &s_innodp_connector_funcs, connector_type);
	drm_connector_helper_add(connector, &s_innodp_connector_helper_funcs);

	return 0;
}

static int inno_dp_encoder_create(struct drm_device *drm_dev,
		struct drm_encoder *encoder, unsigned int possible_crtc)
{
	int retcode = 0;

	retcode = drm_encoder_init(drm_dev, encoder, &s_innodp_encoder_funcs, DRM_MODE_ENCODER_TMDS, NULL);
	if (retcode) {
		DRM_ERROR("Failed to initialise DP encoder\n");
		return retcode;
	}

	drm_encoder_helper_add(encoder, &s_innodp_encoder_helper_funcs);
	encoder->possible_crtcs = possible_crtc;

	return retcode;
}

static int innodp_encoder_connector_attach(struct drm_device *drm_dev, struct dp_device_t *inno_dp)
{
	int ret = 0;

	if (NULL == drm_dev || NULL == inno_dp) {
		DRM_ERROR("Invalid argument: drm_dev or dp is NULL!\n");
		return -1;
	}

	ret = inno_dp_encoder_create(drm_dev, &inno_dp->encoder, inno_dp->chip.possible_crtc);
	if (ret) {
		fh2m_innodpu_err(inno_dp->dev, " DP innodp_encoder_create failed!\n");
		goto err_config_cleanup;
	}

	ret = inno_dp_connector_create(drm_dev, &inno_dp->connector);
	if (ret) {
		fh2m_innodpu_err(inno_dp->dev, "DP innodp_connector_create failed!\n");
		goto err_config_cleanup;
	}

	ret = drm_connector_attach_encoder(&inno_dp->connector, &inno_dp->encoder);

	fh2m_innodpu_info(drm_dev->dev, DPU_UT_DP, "dp attach [ENCODER:%d] to [CONNECTOR:%d] (ret = %d)\n",
				inno_dp->encoder.base.id, inno_dp->connector.base.id, ret);
	goto attach_exit;

err_config_cleanup:
	drm_mode_config_cleanup(drm_dev);
attach_exit:
	return ret;
}

static int inno_dp_chip_init(struct dp_device_t *inno_dp)
{
	chip_type_e plat;
	int retcode = 0;

	inno_dp->chip.lane_rate = 0x14;
	inno_dp->chip.lane_count = 0x04;
	inno_dp->chip.phy_lanes = 0x2;
	inno_dp->chip.phy_rate = 0x1;
	inno_dp->chip.enhance_mode = 0x01;
	inno_dp->chip.lane_swing[0] = 0x2;
	inno_dp->chip.lane_swing[1] = 0x2;
	inno_dp->chip.lane_swing[2] = 0x2;
	inno_dp->chip.lane_swing[3] = 0x2;
	inno_dp->chip.lane_emphasis[0] = 0x03 << DP_TRAIN_PRE_EMPHASIS_SHIFT;
	inno_dp->chip.lane_emphasis[1] = 0x03 << DP_TRAIN_PRE_EMPHASIS_SHIFT;
	inno_dp->chip.lane_emphasis[2] = 0x03 << DP_TRAIN_PRE_EMPHASIS_SHIFT;
	inno_dp->chip.lane_emphasis[3] = 0x03 << DP_TRAIN_PRE_EMPHASIS_SHIFT;
	inno_dp->chip.dp_blight_value = 90;

	inno_dp->chip.hal_edid_mode = fh2m_hal_dp_edid_mode(inno_dp->parent, inno_dp->dp_id);
	if (inno_dp->chip.hal_edid_mode < EDID_AUTO_READ) {
		dp_info(inno_dp->dev, "hal_dp-%d edid_mode parse error%d\n",
				inno_dp->dp_id, inno_dp->chip.hal_edid_mode);
		inno_dp->chip.hal_edid_mode = EDID_AUTO_READ;
	}

	inno_dp->chip.drm_dev = (void *)inno_dp->drm_dev;

	if(dp_ext_init(&inno_dp->chip))
		fh2m_innodpu_warn(inno_dp->dev, "dp ext init error");

	plat = fh2m_hal_get_chiptype(inno_dp->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		retcode = g1_soc_dp_chip_init(&inno_dp->chip,
				inno_dp->dev, inno_dp->dp_id);
		break;
	case CHIP_G0_SOC:
		retcode = g0_soc_dp_chip_init(&inno_dp->chip,
				inno_dp->dev, inno_dp->dp_id);
		break;
	case CHIP_G1P_SOC:
		retcode = g1p_soc_dp_chip_init(&inno_dp->chip,
				inno_dp->dev, inno_dp->dp_id);
		break;
	case CHIP_G0M_SOC:
		retcode = g0m_soc_dp_chip_init(&inno_dp->chip,
				inno_dp->dev, inno_dp->dp_id);
		break;
	default:
		fh2m_innodpu_err(inno_dp->dev, "%s does not currently support %d platform.\n",
			inno_dp->name, plat);
		retcode = -EINVAL;
		break;
	}

	inno_dp->chip.output_mode = innodpu_get_connector_output_mode(inno_dp->dev, REG_M_DP);

	if (inno_dp->chip.output_mode) {
		if (inno_dp->chip.output_mode->mode_connector_item == DRM_MODE_CONNECTOR_VGA) {
			inno_dp->chip.max_width = 1920;
			inno_dp->chip.max_height = 1080;
		}
		dp_info(inno_dp->dev, "mode_connector:%d convert:%s",
				inno_dp->chip.output_mode->mode_connector_item, inno_dp->chip.output_mode->convert_name);
	}

	dp_info(inno_dp->dev, "hal_dp-%d edid_mode %d\n",
			inno_dp->dp_id, inno_dp->chip.hal_edid_mode);

	return retcode;
}

static void inno_dp_chip_fini(struct dp_device_t *inno_dp)
{
	chip_type_e plat;

	plat = fh2m_hal_get_chiptype(inno_dp->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		g1_soc_dp_chip_fini(&inno_dp->chip);
		break;
	case CHIP_G0_SOC:
		g0_soc_dp_chip_fini(&inno_dp->chip);
		break;
	case CHIP_G1P_SOC:
		g1p_soc_dp_chip_fini(&inno_dp->chip);
		break;
	case CHIP_G0M_SOC:
		g0m_soc_dp_chip_fini(&inno_dp->chip);
		break;
	default:
		fh2m_innodpu_err(inno_dp->dev, "%s does not currently support %d platform.\n",
			inno_dp->name, plat);
		break;
	}

	dp_ext_fini(&inno_dp->chip);
}

static void inno_dp_audio_register(struct dp_device_t *inno_dp)
{
#ifdef CONFIG_DRM_INNO_AUDIO
	struct audio_conn *pac = NULL;

	if (is_output_type_dp(inno_dp->chip.output_mode) || \
			is_output_type_hdmi(inno_dp->chip.output_mode)) {
		pac = kzalloc(sizeof(struct audio_conn), fh2m_hal_get_inno_gfp_kernel());
		if (!pac) {
			fh2m_innodpu_err(inno_dp->dev, "Alloc audio_conn failed.\n");
			return;
		}

		pac->conn_st = 0;
		pac->dev = (void *)inno_dp->dev;
		pac->id = inno_dp->dp_id;
		if(is_output_type_hdmi(inno_dp->chip.output_mode)){
			pac->type = INNOAUDIO_CONNECTOR_TYPE_DP2HDMI;
		}else{
			pac->type = INNOAUDIO_CONNECTOR_TYPE_DP;
		}

		pac->type = INNOAUDIO_CONNECTOR_TYPE_DP;
		pac->has_audio = 1; //todo
		pac->priv = (void *)inno_dp;
		pac->enable  = inno_dp_audio_enable;
		pac->disable = inno_dp_audio_disable;
		inno_dp->audio.ac = pac;
		if (fh2m_innoaudio_register_connector(pac)) {
			dp_info(inno_dp->dev, "register audio_conn failed.\n");
			inno_dp->audio.ac = NULL;
			fh2m_inno_kfree(pac);
		} else {
			dp_info(inno_dp->dev, "register audio_conn ok.\n");
			inno_dp->audio.ac = pac;
		}
	}
#endif
}

static int innodpu_dp_bind(struct device *dev, struct device *master, void *data)
{
	int retcode = 0;
	struct drm_device * drm_dev = data;
	struct dp_device_t *inno_dp = NULL;
	plat_data_t *pdata = fh2m_inno_dev_get_platdata(dev);

	BUG_ON(!dev);
	BUG_ON(!data);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_DP0 + pdata->dev_idx)) {
		dp_info(dev, "possible_crtc = 0, do not bind dp-%d\n", pdata->dev_idx);
		return retcode;
	}

	inno_dp = fh2m_inno_devm_kmalloc_kernel(dev, sizeof(*inno_dp));
	if (!inno_dp) {
		fh2m_innodpu_err(dev, "Alloc dp handle failed. Short of memory.\n");
		return -ENOMEM;
	}
	fh2m_inno_memset(inno_dp, 0, sizeof(*inno_dp));
	inno_dp->dp_id = pdata->dev_idx;
	inno_dp->dev = fh2m_inno_get_device(dev);
	inno_dp->parent = fh2m_inno_dev_get_parent(dev);
	inno_dp->drm_dev = drm_dev;
	inno_dp->name = fh2m_inno_kasprintf(fh2m_hal_get_inno_gfp_kernel(), "inno-dp-%d",
		inno_dp->dp_id);
	if (!inno_dp->name) {
		fh2m_innodpu_err(dev, "Alloc dp-%d name failed. Short of memory.\n",
			pdata->dev_idx);
		retcode = -ENOMEM;
		goto err_out_name;
	}
	inno_dp->aux_msg.buffer = fh2m_inno_kzalloc_kernel(INNODP_EDID_BUF_LEN);
	if (!inno_dp->aux_msg.buffer) {
		fh2m_innodpu_err(dev, "Alloc aux_msg failed. Short of memory.\n");
		retcode = -ENOMEM;
		goto err_out_aux;
	}

	dev_set_drvdata(dev, inno_dp);

	retcode = inno_dp_chip_init(inno_dp);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s chip Init failed-%d.\n", inno_dp->name, retcode);
		goto err_chip_init;
	}

	retcode = innodp_encoder_connector_attach(drm_dev, inno_dp);
	if (retcode) {
		fh2m_innodpu_err(inno_dp->dev, "innodp_encoder_connector_attach failed, ret-%d!\n", retcode);
		goto err_attach_encoder;
	}

	inno_dp->aux_mutex = fh2m_inno_mutex_alloc();
	inno_dp->hpd_wq = fh2m_inno_create_singlethread_wq("dp hpd_wq");
	inno_dp->poll_wq = fh2m_inno_create_singlethread_wq("dp poll_wq");
	if (fh2m_inno_is_err_or_null(inno_dp->hpd_wq)   ||
		fh2m_inno_is_err_or_null(inno_dp->poll_wq)) {
		fh2m_innodpu_err(inno_dp->dev, "innodp hpd_work create failed!\n");
		goto err_hpd_work;
	}
	INIT_DELAYED_WORK(&inno_dp->hpd_work, inno_dp_hpd_work);
	INIT_DELAYED_WORK(&inno_dp->dp_poll_work, inno_dp_poll_execute);

	retcode = fh2m_hal_set_irq_handler(dev->parent, inno_dp->chip.hal_module, inno_dp_irq_handle, &inno_dp->chip);
	if (retcode) {
		fh2m_innodpu_err(dev, "failed to set interrupt handler (err = %d)\n", retcode);
		goto err_enable_irq;
	}

	retcode = inno_dp_hw_init(inno_dp);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s hw Init failed-%d.\n", inno_dp->name, retcode);
		goto err_hw_init;
	}

	fh2m_hal_dev_enable_irq(dev->parent, inno_dp->chip.hal_module);
	inno_dp_hw_irq_enable(inno_dp);

#if (DRM_VERSION >= KERNEL_VERSION(5, 14, 0))
	inno_dp->aux.drm_dev = drm_dev;
	inno_dp->aux.name = kasprintf(fh2m_hal_get_inno_gfp_kernel(), "inno-dp[%d] aux", pdata->dev_idx);
#endif
	drm_dp_aux_init(&inno_dp->aux);
	inno_dp->aux.dev = inno_dp->dev;
	inno_dp->aux.transfer = inno_dp_aux_transfer;

	inno_dp_audio_register(inno_dp);

	return retcode;

err_hpd_work:
err_enable_irq:
	inno_dp_cleanup_connector_encoder(inno_dp);
err_attach_encoder:
	inno_dp_hw_fini(inno_dp);
err_hw_init:
	inno_dp_chip_fini(inno_dp);
err_chip_init:
	fh2m_inno_kfree(inno_dp->aux_msg.buffer);
err_out_aux:
	fh2m_inno_kfree(inno_dp->name);
err_out_name:
	put_device(dev);
	fh2m_inno_devm_kfree(dev, inno_dp);

	return retcode;
}

static void innodpu_dp_unbind(struct device *dev, struct device *master, void *data)
{
	struct dp_device_t *inno_dp = NULL;
	plat_data_t *pdata = fh2m_inno_dev_get_platdata(dev);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_DP0 + pdata->dev_idx)) {
		dp_info(dev, "possible_crtc = 0, do not unbind dp-%d\n", pdata->dev_idx);
		return;
	}

	inno_dp = dev_get_drvdata(dev);
	if (!inno_dp) {
		fh2m_innodpu_err(dev, "dp handle is NULL\n");
		return;
	}

	inno_dp_hw_irq_disable(inno_dp);
	fh2m_hal_dev_disable_irq(dev->parent, inno_dp->chip.hal_module);
	fh2m_hal_set_irq_handler(dev->parent,
		inno_dp->chip.hal_module, NULL, NULL);

	cancel_delayed_work_sync(&inno_dp->hpd_work);
	fh2m_inno_destroy_workqueue(inno_dp->hpd_wq);
	cancel_delayed_work_sync(&inno_dp->dp_poll_work);
	fh2m_inno_destroy_workqueue(inno_dp->poll_wq);
	fh2m_inno_mutex_free(inno_dp->aux_mutex);

	inno_dp_hw_fini(inno_dp);
	inno_dp_chip_fini(inno_dp);
	dev_set_drvdata(dev, NULL);

	if (inno_dp->aux_msg.buffer)
		fh2m_inno_kfree(inno_dp->aux_msg.buffer);
	if (inno_dp->name)
		fh2m_inno_kfree(inno_dp->name);
	if (inno_dp->aux.name)
		fh2m_inno_kfree(inno_dp->aux.name);

#ifdef CONFIG_DRM_INNO_AUDIO
	if(inno_dp->audio.ac){
		fh2m_innoaudio_unregister_connector(inno_dp->audio.ac);
		fh2m_inno_kfree(inno_dp->audio.ac);
	}
#endif
	put_device(dev);
	fh2m_inno_devm_kfree(dev, inno_dp);

	return;
}

static const struct component_ops s_innodpu_dp_ops = {
	.bind = innodpu_dp_bind,
	.unbind = innodpu_dp_unbind,
};

static int innodpu_dp_probe(struct platform_device *pdev)
{
	if (NULL == pdev) {
		DRM_ERROR("Invalid argument: the value of pdev is NULL!\r\n");
		return -1;
	}

	return component_add(&pdev->dev, &s_innodpu_dp_ops);
}

static int innodpu_dp_remove(struct platform_device *pdev)
{
	if (NULL == pdev) {
		DRM_ERROR("Invalid argument: the value of pdev is NULL!\r\n");
		return -EFAULT;
	}
	component_del(&pdev->dev, &s_innodpu_dp_ops);
	return 0;
}

static struct platform_device_id s_innodpu_dp_platform_device_id_table[] = {
	{.name = INNO_DP_DEVICE_NAME,.driver_data = 0},
	{},
};
MODULE_DEVICE_TABLE(platform, s_innodpu_dp_platform_device_id_table);

static int innodpu_dp_suspend(struct device *dev)
{
	struct dp_device_t *dp = dev_get_drvdata(dev);
	plat_data_t *pdata = fh2m_inno_dev_get_platdata(dev);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_DP0 + pdata->dev_idx)) {
		dp_info(dev, "possible_crtc = 0, do not suspend dp-%d\n", pdata->dev_idx);
		return 0;
	}

	if (dp) {
		fh2m_innodpu_info(dp->dev, DPU_UT_DP, "suspend\n");

		cancel_delayed_work_sync(&dp->hpd_work);
		cancel_delayed_work_sync(&dp->dp_poll_work);

		inno_dp_hw_irq_disable(dp);
		fh2m_hal_dev_disable_irq(dev->parent, dp->chip.hal_module);

		inno_dp_hw_fini(dp);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int innodpu_dp_resume(struct device *dev)
{
	struct dp_device_t *dp = dev_get_drvdata(dev);
	plat_data_t *pdata = fh2m_inno_dev_get_platdata(dev);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_DP0 + pdata->dev_idx)) {
		dp_info(dev, "possible_crtc = 0, do not resume dp-%d\n", pdata->dev_idx);
		return 0;
	}

	if (dp) {
		fh2m_innodpu_info(dp->dev, DPU_UT_DP, "resume\n");

		inno_dp_hw_init(dp);
		fh2m_hal_dev_enable_irq(dev->parent, dp->chip.hal_module);
		inno_dp_hw_irq_enable(dp);
		inno_dp_check_sink_connection(dp);

		inno_dp_sink_power_ctrl(dp, false);
	}

	return 0;
}
#endif

static void innodpu_dp_shutdown(struct platform_device *pdev)
{
	struct dp_device_t *dp = NULL;
	plat_data_t *pdata =  dev_get_platdata(&pdev->dev);

	if (!innodpu_detect_is_valid_output(&pdev->dev, CONNECTOR_M_DP0 + pdata->dev_idx)) {
		dp_info(&pdev->dev, "possible_crtc = 0, do not shutdown dp-%d\n", pdata->dev_idx);
		return;
	}

	dp = fh2m_inno_platform_get_drvdata(pdev);
	if (!dp)
		return;

	inno_dp_encoder_mode_disable(&dp->encoder);
	innodpu_dp_suspend(&pdev->dev);
}

static const struct dev_pm_ops innodpu_dp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(innodpu_dp_suspend, innodpu_dp_resume)
};

struct platform_driver g_innogpu_dp_driver = {
	.probe = innodpu_dp_probe,
	.remove = innodpu_dp_remove,
	.shutdown = innodpu_dp_shutdown,
	.driver = {
		.name = INNO_DP_DEVICE_NAME,
		.pm = &innodpu_dp_pm_ops,
	},
	.id_table = s_innodpu_dp_platform_device_id_table,
};
