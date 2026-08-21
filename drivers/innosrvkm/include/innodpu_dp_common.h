/*************************************************************************/ /*!
@File			innodpu_dp.h
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

#ifndef __INNO_DP_COMMON_H__
#define __INNO_DP_COMMON_H__
//#include "innodpu_common.h"
#include "innodpu_common.h"
#include "innodpu_connector.h"

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

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif

#define INNODP_PCLK_594_00M						(594000)
#define INNODP_PCLK_534_00M						(533250)
#define INNODP_PCLK_297_00M						(297000)
#define INNODP_PCLK_241_50M						(241500)
#define INNODP_PCLK_220_36M						(220364)
#define INNODP_PCLK_162_00M						(162000)
#define INNODP_PCLK_148_50M						(148500)
#define INNODP_PCLK_146_25M						(146250)
#define INNODP_PCLK_121_75M						(121750)
#define INNODP_PCLK_108_00M						(108000)
#define INNODP_PCLK_106_50M						(106500)
#define INNODP_PCLK_101_00M						(101000)
#define INNODP_PCLK_85_50M						(85500)
#define INNODP_PCLK_83_50M						(83500)
#define INNODP_PCLK_75_00M						(75000)
#define INNODP_PCLK_74_25M						(74250)
#define INNODP_PCLK_72_00M						(72000)
#define INNODP_PCLK_65_00M						(65000)
#define INNODP_PCLK_40_00M						(40000)
#define INNODP_PCLK_36_00M						(36000)
#define INNODP_PCLK_27_00M						(27000)
#define INNODP_PCLK_25_175M						(25175)
#define INNODP_PCLK_AUTO_CALC						(0)

#define INNODP_DEFAULT_FRAME_RATE (120)
#define TU_SIZE   		(30)
#define BIST_EN 		(1)
#define INNODP_HSYNC_POLARITY(n)				(n << 0)
#define INNODP_VSYNC_POLARITY(n)				(n << 1)

#define INNODP_AUDIO_SAMPLE_16 (16)
#define INNODP_AUDIO_SAMPLE_20 (20)
#define INNODP_AUDIO_SAMPLE_24 (24)

#define DP_POLL_PERIOD_MS (50)

#define INNODP_LINK_BW_1_62 (0x06)
#define INNODP_LINK_BW_2_7  (0x0a)
#define INNODP_LINK_BW_5_4  (0x14)
#define INNODP_LINK_BW_8_1  (0x1e)
#define INNODP_ADJUST_VOLTAGE_SWING_MASK 		(0x3)
#define INNODP_ADJUST_PRE_EMPHASIS_MASK 		(0x3)
#define INNODP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT (0x4)
#define INNODP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT  (0x6)
#define INNODP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT  (0x2)

#define INNODP_AUX_TIMEOUTED (50)

#define INNODP_PHY_TEST_PATTERN                 0x248
#define INNODP_PHY_TEST_PATTERN_SEL_MASK       0x7
#define INNODP_PHY_TEST_PATTERN_NONE           0x0
#define INNODP_PHY_TEST_PATTERN_D10_2          0x1
#define INNODP_PHY_TEST_PATTERN_ERROR_COUNT    0x2
#define INNODP_PHY_TEST_PATTERN_PRBS7          0x3
#define INNODP_PHY_TEST_PATTERN_80BIT_CUSTOM   0x4
#define INNODP_PHY_TEST_PATTERN_CP2520         0x5

#define INNODP_RX_CAP_CHANGED                      (1 << 0) /* 1.2 */
#define INNODP_LINK_STATUS_CHANGED                 (1 << 1)
#define INNODP_STREAM_STATUS_CHANGED               (1 << 2)
#define INNODP_HDMI_LINK_STATUS_CHANGED            (1 << 3)
#define INNODP_CONNECTED_OFF_ENTRY_REQUESTED       (1 << 4)

#define DP_REV_10 0x10
#define DP_REV_11 0x11
#define DP_REV_12 0x12
#define DP_REV_13 0x13
#define DP_REV_14 0x14

#define ASYNC_CLOCK_MSA_NVID 32768

#define INNODP_LINK_CFG_SIZE 2
#define INNODP_LINK_COUNT 0
#define INNODP_LINK_RATE  1

#define INNODP_HPD_IRQ (0x1)
#define INNODP_HPD_IN  (0x2)
#define INNODP_HPD_OUT (0x4)

#define to_dp_device(x) container_of(x, struct dp_device_t, x)

#define dp_poll_timeout(chip, entity, val, cond, sleep_us, timeout_us)  \
({ \
	inno_ktime timeout = fh2m_inno_ktime_add_us(fh2m_inno_ktime_get(), timeout_us); \
	for (;;) { \
		fh2m_hal_reg_read32(fh2m_inno_dev_get_parent(chip->dev), chip->reg_module, entity, &val); \
		if (cond) \
			break; \
		if (timeout_us && fh2m_inno_ktime_compare(fh2m_inno_ktime_get(), timeout) > 0) { \
			fh2m_hal_reg_read32(fh2m_inno_dev_get_parent(chip->dev), chip->reg_module, entity, &val); \
			break; \
		} \
		if (sleep_us) { \
			fh2m_inno_usleep_range(DIV_ROUND_UP(sleep_us, 4), sleep_us); \
		} \
	} \
	(cond) ? 0 : -ETIMEDOUT;  \
})

#define INNODPU_DP_SPRINTF(m, fmt, ...) \
do { \
	int len = 0; \
	\
	if (m->count < m->size) { \
		len += fh2m_inno_sprintf(m->buf + m->count, m->size - m->count, fmt, ##__VA_ARGS__); \
		if (m->count + len < m->size) { \
			m->count += len; \
			break; \
		} \
	} \
	\
	m->count = m->size; \
} while (0)

struct innodp_sdp_header {
	u8 HB0; /* Secondary Data Packet ID */
	u8 HB1; /* Secondary Data Packet Type */
	u8 HB2; /* Least Significant Eight Bits of (Data Byte Count - 1) */
	u8 HB3; /* 1:0 Must Significant Two Bits of (Data Byte Count - 1) 7:2 INFOFRAME SDP Version Number */
};

struct innodp_sdp_format {
	bool valid;
	struct innodp_sdp_header sdp_header;
	u8 DB0;
	u8 DB1;
	u8 DB2;
	u8 DB3;
	u8 DB4;
	u8 DB5;
	u8 DB6;
	u8 DB7;
	u8 DB8_31[24];
};

struct inno_dp_audio {
	struct innodp_sdp_format sdp;
	struct audio_conn *ac;
	int sample_rate;
	int sample_bits;
	int channels;
	bool enable;
};

enum dp_monitor_status {
	/* whether to send the segment pointer when edid is read
	 * */
	dp_edid_segment = 0,

	/* Enable or disable work queue listening monitor side link status
	 * */
	dp_link_monitor = 1,

	/* Whether to adjust the voltage swing and pre-emphasis of the
	 * graphics card dp interface
	 * */
	dp_swing_adjust = 2,

	/* Whether or not the monitor is connected by accessing dpcd via
	 * aux during hot-plug detection
	 * */
	dp_rxctx_detect = 3,

	/* Whether dp reduces the number of links during negotiation */
	dp_lanes_adjust = 4,

	/* Whether the video stream timestamp value M(M-vid) in MSA
	 * is calculated by software instead of hardware
	 * */
	dp_mvid_sw_cacl = 5,

	/* Whether to enable the function of replace_timing non-standard
	 * timing with standard timing.
	 * */
	dp_timing_swap = 6,

	/* The display timing bit depth of the dp is obtained from the edid */
	dp_bpc_use_edid = 7,

	/* dp display timing support YUV */
	dp_yuv_support = 8,

	/* dp link powet ctrl en */
	dp_link_pwr_ctl = 9,

	/* edp alternate_scrambler_reset */
	alternate_scrambler_reset = 10,

	/* audio infoframe dynamic init */
	audio_infoframe_dm_init = 11,
};

struct aux_cfg {
	uint32_t aux_cmd;
	uint32_t dpcd_addr;
	uint32_t wr_buff[5];
	uint32_t rd_buff[5];
	uint32_t length;
	uint32_t rd_print;
	uint32_t read;
};

struct i2c_algo_dp_aux_data {
	bool running;
	u16 address;
	struct dp_device *dp;
	int (*aux_ch) (struct i2c_adapter * adapter, int mode, uint8_t write_byte, uint8_t * read_byte);
};

struct dp_sprint_file {
	char *buf;
	size_t size;
	size_t count;
};

struct dp_display_info {
	/**
	 * @bpc: Maximum bits per color channel. Used by HDMI and DP outputs.
	 */
	unsigned int bpc;

	/*
	 * #define DRM_COLOR_FORMAT_RGB444		(1<<0)
	 * #define DRM_COLOR_FORMAT_YCBCR444	(1<<1)
	 * #define DRM_COLOR_FORMAT_YCBCR422	(1<<2)
	 * #define DRM_COLOR_FORMAT_YCBCR420	(1<<3)
	*/
	u32 color_formats;

	/*
	 * Main stream attribute field for indication of
	 * color Encoding format and content color Gamut
	*/
	unsigned int misc0;

	/* video mapping the bit width of each color */
	unsigned int video_map;
};

struct dp_chip_t {
	char *name;
	inno_dev *dev;
	inno_dev *parent;
	void *drm_dev;
	int id;
	unsigned int hal_module;
	unsigned int reg_module;
	unsigned int possible_crtc;
	unsigned int max_width;
	unsigned int max_height;
	bool replace_timing;
	bool combi_en;
	unsigned int max_pclk_rx;
	int dp_blight_value;

	struct dp_ext_t *dp_ext;
	struct inno_panel *panel;

	struct dp_display_info display_info;

	inno_waitqueue_head *dp_hotplug_wq;
	inno_waitqueue_head *dp_aux_wq;

	/* Maximum link rate on the display side */
	unsigned char lane_rate;

	/* Maximum link count on the display side */
	unsigned char lane_count;
	unsigned char phy_rate;
	unsigned char phy_lanes;

	/**
	 * @enhanced_framing:
	 *
	 * enhanced framing capability (mandatory as of DP 1.2)
	 */
	unsigned char enhance_mode;

	/**
	 * @alternate_scrambler_reset:
	 *
	 * eDP alternate scrambler reset capability
	 */
	unsigned char alternate_scrambler_reset;

	/**
	 * tps3_support:
	 *
	 * training pattern sequence 3 supported for equalization
	 */
	bool tps3_support;

	/* Display voltage swing */
	unsigned char lane_swing[4];

	/* Display side pre-emphasis */
	unsigned char lane_emphasis[4];

	/* Maximum link rate supported by source */
	unsigned char max_source_rate;

	bool bist_enable;
	int  hw_self_test_mode;

	unsigned char edid_buf[INNODP_EDID_BUF_LEN];

	int modes;

	int  hal_edid_mode;
	struct connector_output_mode *output_mode;

	int (*dp_hw_init)(struct dp_chip_t *chip);
	void (*dp_hw_fini)(struct dp_chip_t *chip);
	int (*dp_irq_handle)(void *data);
	void (*dp_irq_enable)(struct dp_chip_t *chip);
	void (*dp_irq_disable)(struct dp_chip_t *chip);

	void (*dp_hpd_signal_retrigger)(struct dp_chip_t *chip);

	int (*dp_aux_channel_run)(struct aux_cfg *aux_get_rx, struct dp_chip_t *chip);
	void (*dp_training_pattern_set)(struct dp_chip_t *chip, int pattern);

	// encoder funcs
	int (*encoder_atomic_check)(struct dp_chip_t *chip,
				inno_drm_crtc_state *crtc_state, inno_drm_connector_state *conn_state);
	inno_drm_mode_status (*encoder_mode_valid)(struct dp_chip_t *chip,
			const inno_drm_display_mode *mode);
	void (*encoder_modeset)(struct dp_chip_t *chip,
			int dpu_id, bool test_mode, inno_drm_display_mode *mode);
	void (*encoder_disable)(struct dp_chip_t *chip);
	void (*encoder_enable)(struct dp_chip_t *chip);

	// connector funcs
	inno_drm_mode_status (*connector_mode_valid)(struct dp_chip_t *chip, inno_drm_display_mode *mode);
	int (*connector_detect)(struct dp_chip_t *chip);

	void (*dp_bist_test)(struct dp_chip_t *chip, inno_drm_display_mode *mode);
	void (*dp_blight_set)(struct dp_chip_t *chip, uint32_t val);

	inno_drm_device * (*get_drm_dev)(struct dp_chip_t *chip);

	int (*dp_audio_enable)(struct dp_chip_t *chip, struct inno_dp_audio *audio);
	int (*dp_audio_disable)(struct dp_chip_t *chip);

	void (*dp_source_link_get)(struct dp_chip_t *chip, u8 link_cfg[INNODP_LINK_CFG_SIZE]);
	void (*dp_source_link_set)(struct dp_chip_t *chip, inno_drm_display_mode *mode);

	void (*dp_source_swing_adjust)(struct dp_chip_t *chip, int param);
	int (*dp_source_cfg_show)(struct dp_chip_t *chip, struct dp_sprint_file *m);

	void (*dp_phy_pattern_update)(struct dp_chip_t *chip, u8 pattern);
};



int dp_ext_init(struct dp_chip_t *chip);
void dp_ext_fini(struct dp_chip_t *chip);
void dp_set_aux_status(struct dp_chip_t *chip, int val);
void dp_set_hpg_status(struct dp_chip_t *chip, int val);
void dp_set_audio_status(struct dp_chip_t *chip, int val);
long long dp_get_aux_status(struct dp_chip_t *chip);
long long dp_get_hpg_status(struct dp_chip_t *chip);
long long dp_get_audio_status(struct dp_chip_t *chip);
void *dp_get_edid(struct dp_chip_t *chip);
void dp_set_edid(struct dp_chip_t *chip, void *val);
void *dp_match_crtc(void *dev);

#endif

