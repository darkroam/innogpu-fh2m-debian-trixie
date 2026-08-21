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

#ifndef __INNO_DP_H__
#define __INNO_DP_H__

#include "innodpu_common.h"
#include "hal_interface.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif

#include "innodpu_parse_edid.h"
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
#include "innodpu_dp_common.h"
#include "innodpu_connector.h"
#include "innodpu_common_drm_panel.h"

#include <asm/types.h>

struct inno_dp_compliance_data {
	unsigned long edid;
	uint8_t video_pattern;
	uint16_t hdisplay, vdisplay;
	uint8_t bpc;
	u8 phy_pattern;
};

struct inno_dp_compliance {
	unsigned long test_type;
	struct inno_dp_compliance_data test_data;
	bool test_active;
	u8 test_link_rate;
	u8 test_lane_count;
};

struct dp_device_t {
	char *name;
	unsigned int dp_id;
	inno_dev *dev;
	inno_dev *parent;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
	struct drm_dp_desc desc;
#endif
	struct drm_dp_aux  aux;
	struct drm_dp_aux_msg aux_msg;
	struct backlight_device *bl;
	struct inno_panel *panel;
	struct drm_display_mode current_mode; // pdp->max_mode from current_mode
	struct drm_display_mode native_mode;

	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE];
	/* sink rates as reported by DP_MAX_LINK_RATE/DP_SUPPORTED_LINK_RATES */
	int max_sink_rates;
	int num_sink_rates;
	int sink_rates[DP_MAX_SUPPORTED_RATES];
	bool use_rate_select;

	/* Displayport compliance testing */
	struct inno_dp_compliance compliance;

	inno_workqueue *hpd_wq;
	struct delayed_work hpd_work;

	inno_workqueue *poll_wq;
	struct delayed_work dp_poll_work;
	atomic64_t poll_cnt;

	inno_mutex *aux_mutex;

	struct inno_dp_audio audio;

	struct dp_chip_t chip;

	bool connected;
	bool reduce_lane_count;
	bool hpd_notify;
	bool is_R1;
};

bool inno_dp_is_skip_replace(const struct drm_display_mode *mode);
int g0_soc_dp_chip_init(struct dp_chip_t *chip, inno_dev *dev, unsigned int dp_id);
int g1_soc_dp_chip_init(struct dp_chip_t *chip, inno_dev *dev, unsigned int dp_id);
int g1p_soc_dp_chip_init(struct dp_chip_t *chip, inno_dev *dev, unsigned int dp_id);
int g0m_soc_dp_chip_init(struct dp_chip_t *chip, inno_dev *dev, unsigned int dp_id);

void g0_soc_dp_chip_fini(struct dp_chip_t *chip);
void g1_soc_dp_chip_fini(struct dp_chip_t *chip);
void g1p_soc_dp_chip_fini(struct dp_chip_t *chip);
void g0m_soc_dp_chip_fini(struct dp_chip_t *chip);

int inno_dp_get_edid_monitor(struct drm_connector *connector);
int sink_lane_status_get(struct dp_device_t *inno_dp, u8 status[DP_LINK_STATUS_SIZE]);
int inno_dp_mvid_calc(struct dp_chip_t *chip);
bool inno_dp_monitor_en(enum dp_monitor_status info);
void inno_dp_check_sink_connection(struct dp_device_t *inno_dp);
int inno_dp_sink_power_ctrl(struct dp_device_t *inno_dp, bool power_on);
#endif
