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

#ifndef __INNO_HDMI_COMMON_H__
#define __INNO_HDMI_COMMON_H__

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

#include "innodpu_common.h"
#include "innodpu_connector.h"


#define INNOVIDEO_PCLK_594_00M	(594000)
#define INNOVIDEO_PCLK_534_00M	(534000)
#define INNOVIDEO_PCLK_348_50M	(348500)
#define INNOVIDEO_PCLK_297_00M	(297000)
#define INNOVIDEO_PCLK_277_25M	(277250)
#define INNOVIDEO_PCLK_262_75M	(262750)
#define INNOVIDEO_PCLK_241_50M	(241500)
#define INNOVIDEO_PCLK_229_50M	(229500)
#define INNOVIDEO_PCLK_202_50M	(202500)
#define INNOVIDEO_PCLK_193_25M	(193250)
#define INNOVIDEO_PCLK_189_00M	(189000)
#define INNOVIDEO_PCLK_175_50M	(175500)
#define INNOVIDEO_PCLK_162_00M	(162000)
#define INNOVIDEO_PCLK_154_00M	(154000)
#define INNOVIDEO_PCLK_148_50M	(148500)
#define INNOVIDEO_PCLK_146_25M	(146250)
#define INNOVIDEO_PCLK_121_75M	(121750)
#define INNOVIDEO_PCLK_108_00M	(108000)
#define INNOVIDEO_PCLK_106_50M	(106500)
#define INNOVIDEO_PCLK_85_50M	(85500)
#define INNOVIDEO_PCLK_85_00M	(85000)
#define INNOVIDEO_PCLK_83_50M	(83500)
#define INNOVIDEO_PCLK_75_00M	(75000)
#define INNOVIDEO_PCLK_74_25M	(74250)
#define INNOVIDEO_PCLK_65_00M	(65000)
#define INNOVIDEO_PCLK_54_00M	(54000)
#define INNOVIDEO_PCLK_40_00M	(40000)
#define INNOVIDEO_PCLK_27_00M	(27000)
#define INNOVIDEO_PCLK_25_175M	(25175)

#define INNOVIDEO_PCLK_MAX_FREQ	(600000)
#define INNOVIDEO_PCLK_MIN_FREQ	(25000)
#define pclk_is_invalid(_p) (((_p) < INNOVIDEO_PCLK_MIN_FREQ) ||\
		((_p) > INNOVIDEO_PCLK_MAX_FREQ))

#define INNOHDMI_FRAME_RATE_30	(30)
#define INNOHDMI_FRAME_RATE_60	(60)
#define INNOHDMI_FRAME_RATE_75	(75)
#define INNOHDMI_DEFAULT_FRAME_RATE	(INNOHDMI_FRAME_RATE_60)

#define INNOHDMI_TMDS_THRESHOLD	(340000)

#define HDMI_IRQ_HPD_MASK	INNO_BIT(0)
#define HDMI_IRQ_SCDC_MASK	INNO_BIT(1)
#define HDMI_IRQ_PLLLOC_KMASK	INNO_BIT(2)
#define HDMI_IRQ_EDID_MASK	INNO_BIT(3)
#define HDMI_IRQ_HDCP0_MASK	INNO_BIT(4)
#define HDMI_IRQ_HDCP1_MASK	INNO_BIT(5)
#define HDMI_IRQ_CEX_TX_MASK	INNO_BIT(6)
#define HDMI_IRQ_CEX_RX_MASK	INNO_BIT(7)
#define HDMI_IRQ_MASKALL (0xffff)


#define AVMUTE_WAIT_ONEFRAME   INNO_BIT(0)
#define AVMUTE_KEEP_GCPPACKET  INNO_BIT(1)


struct hdmi_chip_t;

struct hdmi_debugfs {
	struct dentry   *debugfs_root;
	struct list_head debugfs_list;
	struct mutex     debugfs_lock; /* Protects debugfs_list. */
};

struct hdmi_ext_t {
	//atomic64_t edid_status;
	atomic64_t hpg_status;
	//atomic64_t ddc_status;
	//atomic64_t scdc_status;
	atomic64_t prepll_lock;
	atomic64_t postpll_lock;

	struct  hdmi_debugfs custom_edidparse;
	struct  hdmi_debugfs custom_bisttest;
	struct  hdmi_debugfs custom_hw_self_test;
	struct  hdmi_debugfs custom_hdmi_status;
};

struct hdmi_chip_regops_t {
	int (*write)(u32 entity, u32 value, struct hdmi_chip_t *chip);
	int (*read)(u32 entity, struct hdmi_chip_t *chip);
};

struct hdmi_chip_i2c_t {
	struct hdmi_chip_t *chip;
	inno_mutex *mutex;
	void (*set_ddc_clk)(struct hdmi_chip_t *chip);

	int hwi2c_status;
	inno_completion *hwi2c_comp;
	int  (*hwi2c_transfer)(struct hdmi_chip_t *chip, u16 addr, u8 *buf, u16 len);
	bool hwi2c_inited;

	void (*setsda)(struct hdmi_chip_t *chip, int state);
	void (*setscl)(struct hdmi_chip_t *chip, int state);
	int  (*getsda)(struct hdmi_chip_t *chip);
	int  (*getscl)(struct hdmi_chip_t *chip);
	int  (*pre_xfer)(struct hdmi_chip_t *chip);
	void (*post_xfer)(struct hdmi_chip_t *chip);
	bool swi2c_inited;

	int edid_status;
	inno_completion *edid_comp;
	bool edid_inited;

	int scdc_status;
	inno_completion *scdc_comp;
	bool scdc_inited;
};

struct hdmi_chip_t {
	char *name;
	inno_dev *dev;
	inno_dev *parent;
	int id;
	inno_drm_device *drm_dev;
	inno_drm_display_mode *adjusted_mode;
	bool test_mode;
	bool replace_timing; /* 对于非标的时序，是否替换为标准的时序 */

	unsigned int reg_module;
	unsigned int hal_module;
	unsigned int possible_crtc;
	unsigned int max_width;
	unsigned int max_height;
	bool combi_en;
	unsigned int max_pclk_rx;

	struct hdmi_ext_t *hdmi_ext;

	bool sink_is_hdmi; // drm_detect_hdmi_monitor
	bool sink_has_audio; // drm_detect_monitor_audio
	bool clk_invert;

	/*hdcp*/
	bool bksv_pass;
	bool hdmi_cfg_hdcp14;

	int modes;

	int  hal_edid_mode;
	int  hw_self_test_mode;
	struct connector_output_mode *output_mode;

	unsigned int flags_avmute;

	void __iomem *i2c_reg;

	int  (*hw_init)(struct hdmi_chip_t *chip);
	void (*hw_fini)(struct hdmi_chip_t *chip);
	unsigned int (*irq_handle)(struct hdmi_chip_t *chip);
	void (*irq_enable)(struct hdmi_chip_t *chip, unsigned int flag);
	void (*irq_disable)(struct hdmi_chip_t *chip, unsigned int flag);

	int (*hpd_status_detect)(struct hdmi_chip_t *chip);
	int (*ddc_status_detect)(struct hdmi_chip_t *chip);

	// encoder funcs
	int (*encoder_atomic_check)(struct hdmi_chip_t *chip,
		inno_drm_crtc_state *crtc_state, inno_drm_connector_state *conn_state);
	inno_drm_mode_status (*encoder_mode_valid)(struct hdmi_chip_t *chip,
		                        const inno_drm_display_mode *mode); // TODO drop it
	void (*encoder_modeset)(struct hdmi_chip_t *chip,
		                    int dpu_id, bool test_mode, inno_drm_display_mode *mode);
	void (*encoder_disable)(struct hdmi_chip_t *chip);
	void (*encoder_enable)(struct hdmi_chip_t *chip, inno_drm_crtc *crtc);

	// connector funcs
	inno_edid *(*connector_get_edid)(inno_drm_connector *connector,
		                    struct hdmi_chip_t *chip); // TODO drop it in the feature
	inno_drm_mode_status (*connector_mode_valid)(struct hdmi_chip_t *chip,
		                    inno_drm_display_mode *mode);
	int (*connector_detect)(struct hdmi_chip_t *chip);

	int  (*hdmi_edid_read)(struct hdmi_chip_t *chip);
	void (*hdmi_edid_parse)(inno_seq_file *seq, struct hdmi_chip_t *chip);

	bool (*hdmi_prepll_islock)(struct hdmi_chip_t *chip);
	bool (*hdmi_postpll_islock)(struct hdmi_chip_t *chip);
	int (*hdmi_read_rx_scramb)(struct hdmi_chip_t *chip, uint32_t *rdata);
	int (*hdmi_read_tx_scramb)(struct hdmi_chip_t *chip);

	bool audio_enable;  //record audio status
	int (*hdmi_enable_audio)(struct hdmi_chip_t *chip);
	int (*hdmi_disable_audio)(struct hdmi_chip_t *chip);

	/*NEW Framework*/
	const struct hdmi_chip_regops_t *regops;

	struct hdmi_chip_i2c_t *chipi2c;
	unsigned char edid_buf[INNOHDMI_EDID_BUF_LEN];
	unsigned int total_block;

	struct i2c_adapter hwi2c_adapter;
	struct i2c_adapter bit_adapter;
	struct i2c_algo_bit_data bit_data;
};


int hdmi_ext_init(struct hdmi_chip_t *chip);
void hdmi_ext_fini(struct hdmi_chip_t *chip);
void hdmi_set_edid_status(struct hdmi_chip_t *chip, int val);
long long hdmi_get_edid_status(struct hdmi_chip_t *chip);
void hdmi_set_hpg_status(struct hdmi_chip_t *chip, int val);
long long hdmi_get_hpg_status(struct hdmi_chip_t *chip);
void hdmi_set_scdc_status(struct hdmi_chip_t *chip, int val);
long long hdmi_get_scdc_status(struct hdmi_chip_t *chip);
void *hdmi_match_crtc(void *dev);

int g0_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip);
int g0_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip);
int g0_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip);
int g0_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip);

int g1_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip);
int g1_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip);
int g1_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip);
int g1_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip);

int g1p_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip);
int g1p_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip);
int g1p_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip);
int g1p_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip);

int g0m_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip);
int g0m_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip);
int g0m_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip);
int g0m_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip);

void *inno_hdmi_get_chip_adapater(struct hdmi_chip_t *chip);

#endif

