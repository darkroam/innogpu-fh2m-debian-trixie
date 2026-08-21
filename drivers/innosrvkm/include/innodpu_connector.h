/*************************************************************************/ /*!
@File			innodpu_connector.h
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
#ifndef __INNODPU_CONNECTOR_H__
#define __INNODPU_CONNECTOR_H__
#include <drm/drm_modes.h>
#include "innodpu_common.h"
#include "inno_drm.h"
#include "inno_drm_mode.h"

#define SCALE_MAX_MODE 2048

#define DPU_CONNECT_NUM  (8)

#define CONNECTOR_MAP_ITEM(type, width, height, crtc, rep, combi) [type] = \
	{width, height, INNO_BIT(crtc), rep, combi}
#define CONNECTOR_MAP_EMPTY_ITEM(type) [type] = {0, 0, 0, 0, 0}

#define CONNECTOR_ID_ITEM(type, id) [type] = (id)

enum custom_output_mode {
	/* enable dpu combine */
	CUSTOM_OUTPUT_MODE_0 = 0,
	CUSTOM_OUTPUT_MODE_1,
	CUSTOM_OUTPUT_MODE_2,
	CUSTOM_OUTPUT_MODE_3,
	CUSTOM_OUTPUT_MODE_4,
	CUSTOM_OUTPUT_MODE_5,
	CUSTOM_OUTPUT_MODE_6,
	CUSTOM_OUTPUT_MODE_7,
	CUSTOM_OUTPUT_MODE_8,
	CUSTOM_OUTPUT_MODE_9,
	CUSTOM_OUTPUT_MODE_10,
	CUSTOM_OUTPUT_MODE_11,
	CUSTOM_OUTPUT_MODE_12,
	CUSTOM_OUTPUT_MODE_13,
	CUSTOM_OUTPUT_MODE_14,
	CUSTOM_OUTPUT_MODE_15,
	CUSTOM_OUTPUT_MODE_16,
	CUSTOM_OUTPUT_MODE_17,
	CUSTOM_OUTPUT_MODE_18,
	CUSTOM_OUTPUT_MODE_19,
	CUSTOM_OUTPUT_MODE_20,
	CUSTOM_OUTPUT_MODE_21,
	CUSTOM_OUTPUT_MODE_22,
	CUSTOM_OUTPUT_MODE_23,
	CUSTOM_OUTPUT_MODE_24,
	CUSTOM_OUTPUT_MODE_25,
	CUSTOM_OUTPUT_MODE_26,
	CUSTOM_OUTPUT_MODE_27,
	CUSTOM_OUTPUT_MODE_28,
	CUSTOM_OUTPUT_MODE_29,
	CUSTOM_OUTPUT_MODE_30,
	CUSTOM_OUTPUT_MODE_31,
	CUSTOM_OUTPUT_MODE_32,

	/* disable dpu combine */
	CUSTOM_OUTPUT_MODE_128 = 128,
	CUSTOM_OUTPUT_MODE_129,
	CUSTOM_OUTPUT_MODE_130,
	CUSTOM_OUTPUT_MODE_131,
	CUSTOM_OUTPUT_MODE_132,
	CUSTOM_OUTPUT_MODE_133,
	CUSTOM_OUTPUT_MODE_134,
	CUSTOM_OUTPUT_MODE_135,
	CUSTOM_OUTPUT_MODE_136,
	CUSTOM_OUTPUT_MODE_137,
	CUSTOM_OUTPUT_MODE_138,
	CUSTOM_OUTPUT_MODE_139,
	CUSTOM_OUTPUT_MODE_140,

	CUSTOM_OUTPUT_MODE_141,
	CUSTOM_OUTPUT_MODE_142,
	CUSTOM_OUTPUT_MODE_MAX,
};

#define DISPLAY_MODE_LEN 	(32)
#define MFC_NAME_LEN		(4U)
#define MONITOR_NAME_LEN 	(13U)

#define HW_ODM_VENDOR_LEN    (4U)
#define HW_PCB_VERSION_LEN  (28U)

#define INNO_COLOR_FORMAT_RGB444	(1<<0)
#define INNO_COLOR_FORMAT_YCBCR444	(1<<1)
#define INNO_COLOR_FORMAT_YCBCR422	(1<<2)
#define INNO_COLOR_FORMAT_YCBCR420	(1<<3)

#define INNO_COLOR_FORMAT_RGB444	(1<<0)
#define INNO_COLOR_FORMAT_YCBCR444	(1<<1)
#define INNO_COLOR_FORMAT_YCBCR422	(1<<2)
#define INNO_COLOR_FORMAT_YCBCR420	(1<<3)

enum {
	HDMI0_ZOOM_ENABLE = 0,
	HDMI1_ZOOM_ENABLE,
	DP0_ZOOM_ENABLE,
	LVDS_ZOOM_ENABLE,
	HDMI2_ZOOM_ENABLE,
	HDMI3_ZOOM_ENABLE,
	DP1_ZOOM_ENABLE,
	VGA_ZOOM_ENABLE,
};

enum connector_module {
	CONNECTOR_M_HDMI0 = 0,
	CONNECTOR_M_HDMI1,
	CONNECTOR_M_HDMI2,
	CONNECTOR_M_HDMI3,
	CONNECTOR_M_DP0,
	CONNECTOR_M_DP1,
	CONNECTOR_M_VGA,
	CONNECTOR_M_LVDS,
	CONNECTOR_M_HDDP0,
	CONNECTOR_M_HDDP1,
	CONNECTOR_M_HDDP2,
	CONNECTOR_M_MAX,
};

enum connector_backlight_mode {
	CONNECTOR_BACKLIGHT_DDCCI = 0,
	CONNECTOR_BACKLIGHT_PWM0,
	CONNECTOR_BACKLIGHT_AUX_VESA,
	CONNECTOR_BACKLIGHT_AUX_HDR,
};

struct connector_map_prop {
	unsigned int max_width;
	unsigned int max_height;
	unsigned int possible_crtc;
	bool replace_timing;
	bool possible_crtc_combi;
};

struct monitor_non_replace {
	char monitor_name[MONITOR_NAME_LEN];
	char non_replace_name[DISPLAY_MODE_LEN];
	int clock;
};

struct connector_output_mode {
	int mode_connector_item;
	unsigned char convert_name[20];

#define CONNECTOR_FLAGS_NONE       (0x0U)
	/*
	 * for some conversion chip, such as CS5801(HDMI2DP) etc.
	 * when DP cable is removed and the DP2HDMI's HPD has been pulled down,
	 * we can still read EDID from the chip.
	 * So we must skip ddc detect, and wait longer time after HPD IRQ.
	 * */
#define CONNECTOR_FLAGS_SKIP_DDC   (0x1U)
#define CONNECTOR_FLAGS_LONGER_HPD (0x2U)

	/* 1.kylin system for hot-plugging processing,single-screen, dual-screen phenomenon is
	 * inconsistent, single-screen unplug the monitor, the display interface will not go
	 * disable, source will always send stream data.
	 * 2. ch7517 firmware, hpd pin high and low by the display is connected, whether to read
	 * the display edid, dp source this side of the main link whether there is a data stream
	 * of the three conditions, only all three are satisfied, ch7517 unreset after the hpd pin will
	 * be pulled high!
	 * 3. bug id:7013
	 * */
#define CONNECTOR_FLAGS_EN_PLUGIN (0x4U)

	unsigned int flags;

	/*
	 * Optional.
	 * when flags is CONNECTOR_FLAGS_LONGER_HPD, the data is delay_ms.
	 * etc.
	 * */
	int data;
};


struct hw_board_info {
	const char *odm_vendor;
	const char *pcb_version;
};
#define HW_BOARD_INFO_ITEM(_ov, _pv) {.odm_vendor=(_ov), .pcb_version=(_pv)}

struct mfc_monitor_info {
	const char *mfc_name;
	const char *monitor_name;
};
#define MFC_MONITOR_INFO_ITEM(_mfc, _monitor) {.mfc_name=(_mfc), .monitor_name=(_monitor)}


struct resolution_info {
	int hdisplay;
	int vdisplay;
	int vrefresh;
	int clock;
};
#define RESOLUTION_INFO_ITEM(_h, _v, _vr, _c) {.hdisplay=(_h), .vdisplay=(_v), .vrefresh=(_vr), .clock=(_c)}

bool innodpu_modes_equal(const struct drm_display_mode *mode1,
		    const struct drm_display_mode *mode2);

bool is_special_mode(const struct drm_display_mode *mode);
bool is_virtual_mode(const struct drm_display_mode *native_mode, const struct drm_display_mode *mode);
bool is_native_mode_valid(const struct drm_display_mode *native_mode);

const struct drm_display_mode *innodpu_modes_match_replace_table(
		const struct drm_display_mode * mode,
		bool (*is_need_replace)(const struct drm_display_mode *),
		bool (*is_skip_replace)(const struct drm_display_mode *));

void innodpu_modes_replace_timing(struct drm_display_mode *mode, const struct drm_display_mode *rmode);

int innodpu_modes_fixup_preferred_nonaligned_modes(struct drm_connector *connector);
int innodpu_modes_drop_repeat(struct drm_connector *connector);

int innodpu_add_modes_without_edid(struct drm_connector *connector, \
		bool is_valid_mode(const struct drm_display_mode *));

bool is_output_type_hdmi(struct connector_output_mode *om);
bool is_output_type_dvi(struct connector_output_mode *om);
bool is_output_type_vga(struct connector_output_mode *om);
bool is_output_type_dp(struct connector_output_mode *om);
bool is_output_type_edp(struct connector_output_mode *om);
bool is_output_type_lvds(struct connector_output_mode *om);
bool is_flags_skip_ddc_detect(struct connector_output_mode *om);
bool is_flags_wait_longer_hpd(struct connector_output_mode *om);
bool is_flags_data_en_plugin(struct connector_output_mode *om);

struct resolution_info *innodpu_resolution_match(const inno_drm_display_mode *mode, struct resolution_info *resolution, int count);
#define innodpu_is_resolution_match(_m, _r) ({\
			void* _x=(void *)innodpu_resolution_match(_m, _r, 1);\
			((_x)!=NULL)?true:false; })

struct mfc_monitor_info *innodpu_mfc_monitor_match(unsigned char* edid, struct mfc_monitor_info *monitor_info, int count);
#define innodpu_is_mfc_monitor_match(_e, _m) ({\
			void* _x=(void *)innodpu_mfc_monitor_match(_e, _m, 1);\
			((_x)!=NULL)?true:false; })

struct hw_board_info *innodpu_odm_pcb_match(inno_dev *pcie_dev, struct hw_board_info *board_info, int count);
#define innodpu_is_odm_pcb_match(_p, _b) ({\
			void* _x=(void *)innodpu_odm_pcb_match(_p, _b, 1);\
			((_x)!=NULL)?true:false; })

int innodpu_custom_init(struct drm_device *drm_dev, struct innodpu_drm_private *dev_priv);
void innodpu_custom_fini(struct innodpu_drm_private *dev_priv);
struct connector_map_prop *innodpu_find_connector_map_module(struct device *dev, enum connector_module module);
bool innodpu_detect_is_valid_output(struct device *dev, enum connector_module module);
int innodpu_str_push_edid(const char *buf, struct drm_connector *connector);
int innodpu_modes_noedid(struct drm_connector *connector);
int innodpu_get_display_mode(void);



int innodpu_get_odm_info(struct device *device, char *odm_info, inno_dev *dev);

int innodpu_connector_add_common_modes(struct drm_connector *connector,
		struct drm_display_mode *native_mode, bool support_force);

struct drm_display_mode *
innodpu_get_native_mode(struct drm_connector *connector,
		bool (*filter)(struct drm_connector *pconnector, struct drm_display_mode *pmode, bool priv_filter));

struct connector_output_mode *innodpu_get_connector_output_mode(inno_dev *dev, unsigned int reg_module);
int innodpu_get_connector_backlight_mode(inno_dev *dev, unsigned int reg_module);

int innodpu_detailed_block_monitor_range_replace(u8 *raw_edid);
int innodpu_conn_get_monitor_max_clk(u8 *raw_edid);
void innodpu_connector_clear_eld(struct drm_connector *connector);

int innodpu_connector_cnt_detect(struct drm_device *drm_dev);

#endif

