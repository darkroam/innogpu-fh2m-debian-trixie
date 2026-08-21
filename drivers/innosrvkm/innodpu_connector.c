/*************************************************************************/ /*!
@File			innodpu_connector.c
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
#include "innodpu_connector.h"
#include "innodpu_common.h"
#include "innodpu_dp_common.h"
#include "innogpu_drm.h"
#include <linux/acpi.h>

extern unsigned int s_dpu_match;

static const struct drm_display_mode s_noedid_modes[] = {
	/*
	 * FH2M notebook internal AUO panel: EDID can be readable while the final
	 * probed DRM mode list is empty during fbdev setup. This reduced-blanking
	 * mode has been verified on the device and gives fbcon a safe pre-X mode.
	 */
	{DRM_MODE("1920x1200", DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED, 154000, 1920, 1968,
			  2000, 2080, 0, 1200, 1203, 1209, 1235, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),},
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
};

static const struct drm_display_mode s_special_modes[] = {
	// mode table invalid mode
	{ DRM_MODE("100x100", DRM_MODE_TYPE_DRIVER, 100000, 100, 200,
			   300, 400, 0, 100, 200, 300, 400, 0,
			   0) },
};

static const struct drm_display_mode s_replace_modes[] = {
	{ DRM_MODE("2240x1400", DRM_MODE_TYPE_DRIVER, 212500, 2240, 2240+48,
			   2240+48+32, 2240+220, 0, 1400, 1400+3, 1400+3+6, 1400+40, 0,
			   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = 0, }, // invalid picture_aspect_ratio

	{ DRM_MODE("2240x1400", DRM_MODE_TYPE_DRIVER, 141696, 2240, 2240+48,
			   2240+48+32, 2240+220, 0, 1400, 1400+3, 1400+3+6, 1400+40, 0,
			   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = 0, }, // invalid picture_aspect_ratio
	/*
		support for F4
		CVT-RB Modeline 	   Modeline "2160x1400_29.98" 98.75 2160 2208 2240 2320 1400 1403 1413 1420 +HSync -VSync
		CVT-RB Modeline 	   Modeline "2160x1400_59.94" 200.25 2160 2208 2240 2320 1400 1403 1413 1440 +HSync -VSync
		CVT-RB Modeline        Modeline "2160x1400_74.93" 252.25 2160 2208 2240 2320 1400 1403 1413 1451 +HSync -VSync
	*/
	/* 2160x1400@30Hz 16:9 */
	{ DRM_MODE("2160x1400", DRM_MODE_TYPE_DRIVER, 98750, 2160, 2208,
		   2240, 2320, 0, 1400, 1403, 1413, 1420, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = 0, }, // invalid picture_aspect_ratio

	/* 2160x1400@60Hz 16:9 */
	{ DRM_MODE("2160x1400", DRM_MODE_TYPE_DRIVER, 200250, 2160, 2208,
		   2240, 2320, 0, 1400, 1403, 1413, 1440, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = 0, }, // invalid picture_aspect_ratio

	/* 2160x1400@75Hz 16:9 */
	{ DRM_MODE("2160x1400", DRM_MODE_TYPE_DRIVER, 252250, 2160, 2208,
		   2240, 2320, 0, 1400, 1403, 1413, 1451, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = 0, }, // invalid picture_aspect_ratio

	/* 2560x1080@60Hz 16:9 */
	{ DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 181000, 2560, 2578,
		   2610, 2720, 0, 1080, 1082, 1090, 1111, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, },

	/* 2560x1080@75Hz 16:9 */
	{ DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 226640, 2560, 2578,
		   2610, 2720, 0, 1080, 1082, 1090, 1111, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, },

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

	/* 2560x1440@100Hz 16:9 */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 405280, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1490, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1440@120Hz 16:9 */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 486340, 2560, 2608,
		   2640, 2720, 0, 1440, 1445, 1450, 1490, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1600@30Hz 16:9 */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 128541, 2560, 2568,
		   2600, 2640, 0, 1600, 1609, 1617, 1623, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1600@48Hz 16:9 */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 216990, 2560, 2608,
		   2640, 2720, 0, 1600, 1603, 1608, 1662, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1600@60Hz 16:9 */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 271240, 2560, 2608,
		   2640, 2720, 0, 1600, 1603, 1608, 1662, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1600@60Hz 16:9 */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 260726, 2560, 2568,
		   2600, 2640, 0, 1600, 1632, 1640, 1646, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 2560x1600@75Hz 16:9 */
	{ DRM_MODE("2560x1600", DRM_MODE_TYPE_DRIVER, 328284, 2560, 2568,
		   2600, 2640, 0, 1600, 1644, 1652, 1658, 0,
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

	/* 3840x1080@60Hz 32:9*/
	{ DRM_MODE("3840x1080", DRM_MODE_TYPE_DRIVER, 292350, 3840, 4018,
		  4178, 4388, 0, 1080, 1083, 1093, 1111, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 3440x1440@60 */
	{ DRM_MODE("3440x1440", DRM_MODE_TYPE_DRIVER, 336450, 3440, 3548,
			3648, 3788, 0, 1440, 1443, 1453, 1481, 0,
			DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

	/* 3440x1440@30 */
	{ DRM_MODE("3440x1440", DRM_MODE_TYPE_DRIVER, 165972, 3440, 3548,
			3648, 3788, 0, 1440, 1443, 1453, 1461, 0,
			DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },

};

static bool innodpu_modes_match_timings(const struct drm_display_mode *mode1,
				const struct drm_display_mode *mode2)
{
	return mode1->hdisplay == mode2->hdisplay &&
		mode1->hsync_start == mode2->hsync_start &&
		mode1->hsync_end == mode2->hsync_end &&
		mode1->htotal == mode2->htotal &&
		mode1->vdisplay == mode2->vdisplay &&
		mode1->vsync_start == mode2->vsync_start &&
		mode1->vsync_end == mode2->vsync_end &&
		mode1->vtotal == mode2->vtotal;
}

static bool innodpu_modes_match_clock(const struct drm_display_mode *mode1,
			    const struct drm_display_mode *mode2)
{
	if (mode1->clock && mode2->clock) {
		return KHZ2PICOS(mode1->clock) == KHZ2PICOS(mode2->clock);
	} else {
		return mode1->clock == mode2->clock;
	}
}

bool innodpu_modes_equal(const struct drm_display_mode *mode1,
		    const struct drm_display_mode *mode2)
{
	if (!mode1 || !mode2) {
		return false;
	}

	if (!innodpu_modes_match_timings(mode1, mode2)) {
		return false;
	}

	if (!innodpu_modes_match_clock(mode1, mode2)) {
		return false;
	}
	return true;
}

static bool innodpu_modes_match(const struct drm_display_mode *mode1,
			    const struct drm_display_mode *mode2)
{
	if (mode1->hdisplay == mode2->hdisplay && \
		mode1->vdisplay == mode2->vdisplay && \
		abs(drm_mode_vrefresh(mode1) - drm_mode_vrefresh(mode2)) <= 1) {
		return true;
	}

	return false;
}


bool is_special_mode(const struct drm_display_mode *mode)
{
	return innodpu_modes_equal(mode, &s_special_modes[0]);
}

bool is_virtual_mode(const struct drm_display_mode *native_mode, \
		const struct drm_display_mode *mode)
{
	if (!native_mode || !mode)
		return false;

	if ((native_mode->htotal == mode->htotal) &&
		(native_mode->vtotal == mode->vtotal) &&
		(native_mode->clock == mode->clock) &&
		((native_mode->hdisplay != mode->hdisplay) ||
		(native_mode->vdisplay != mode->vdisplay))) {
		return true;
	}

	return false;
}

bool is_native_mode_valid(const struct drm_display_mode *native_mode)
{
	if ((native_mode->clock > 0) && \
		(native_mode->hdisplay > 0) && \
		(native_mode->hdisplay < 8192)) {
		return true;
	}

	return false;
}

static int innodpu_modes_find_table(const struct drm_display_mode *mode,
	const struct drm_display_mode *table, int count)
{
	int i = 0;
	const struct drm_display_mode *rmode = table;

	if (!mode || !rmode || count < 0)
		return -1;

	for (i = 0; i < count; i++) {
		if (innodpu_modes_match(mode, rmode)) {
			//find it and return;
			return i;
		}
		rmode ++;
	}

	return -1;
}

void innodpu_modes_replace_timing(struct drm_display_mode *mode, const struct drm_display_mode *rmode)
{
	if (!mode || !rmode)
		return;

	/*the code could be deleted, because it could be replace when equal.*/
	if (innodpu_modes_equal(mode, rmode))
		return;

	if (innodpu_modes_match(mode, rmode)) {
		mode->clock = rmode->clock;
		mode->hdisplay = rmode->hdisplay;
		mode->hsync_start = rmode->hsync_start;
		mode->hsync_end = rmode->hsync_end;
		mode->htotal = rmode->htotal;
		mode->hskew = rmode->hskew;
		mode->vdisplay = rmode->vdisplay;
		mode->vsync_start = rmode->vsync_start;
		mode->vsync_end = rmode->vsync_end;
		mode->vtotal = rmode->vtotal;
		mode->vscan = rmode->vscan;
		mode->flags = rmode->flags;
		mode->picture_aspect_ratio = rmode->picture_aspect_ratio;
		mode->type = (mode->type & DRM_MODE_TYPE_PREFERRED) ? \
					 (rmode->type | DRM_MODE_TYPE_PREFERRED) : rmode->type;
	}

	/* update mode->crtc_* value */
	drm_mode_set_crtcinfo(mode, DRM_MODE_FLAG_INTERLACE);
}

static bool is_need_replace_helper(const struct drm_display_mode *mode)
{
	if (drm_mode_validate_size(mode, \
				INNODPU_COMBINE_WIDTH, INNODPU_COMBINE_HEIGHT) && \
			!drm_match_cea_mode(mode)) {
		return true;
	}

	return false;
}

static bool is_skip_replace_helper(const struct drm_display_mode *mode)
{
	/*default*/
	return false;
}

const struct drm_display_mode *innodpu_modes_match_replace_table(
		const struct drm_display_mode * mode,
		bool (*is_need_replace)(const struct drm_display_mode *),
		bool (*is_skip_replace)(const struct drm_display_mode *))
{
	int index = -1;

	if (!mode)
		return NULL;

	if (!is_need_replace) {
		is_need_replace = is_need_replace_helper;
	}

	if (!is_skip_replace) {
		is_skip_replace = is_skip_replace_helper;
	}

	if (!is_need_replace(mode)) {
		return NULL;
	}

	if (is_skip_replace(mode)) {
		return NULL;
	}

	index = innodpu_modes_find_table(mode, \
			s_replace_modes, INNO_ARRAY_SIZE(s_replace_modes));
	if (index < 0) {
		return &s_special_modes[0];
	}

	return &s_replace_modes[index];
}

static bool is_non_aligned_stride_modes(const struct drm_display_mode *mode)
{
	/*
	 * aligned 32bytes: (hdisplay * bpp / 8) % 32 == 0
	 * */
	if (mode->hdisplay % 8)
		return true;

	return false;
}

/*
 * NOTE:
 * The PDP had problems reading some images(such as 1366x768) due to hardware limitations of
 * stride 32bytes alignment, which caused /dev/fb0 to be unable to output that
 * resolution.
 *
 * To circumvent this problem, we must ensure that 1366x768 is not
 * perferred mode. This way /dev/fb0 will not select the 1366x768 output.
 *
 * */
int innodpu_modes_fixup_preferred_nonaligned_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode = NULL;
	bool set_next = false;
	int  hdisplay = 4096, vdisplay = 4096;

	list_for_each_entry(mode, &connector->probed_modes, head) {
		if (is_non_aligned_stride_modes(mode)) {
			if (mode->type & DRM_MODE_TYPE_PREFERRED) {
				mode->type &= ~DRM_MODE_TYPE_PREFERRED;
				hdisplay = mode->hdisplay;
				vdisplay = mode->vdisplay;
				set_next = true;
				//pr_err("drop preferred: " DRM_MODE_FMT "\n",  DRM_MODE_ARG(mode));
			}
		} else if (set_next) {
				/*eg: when 1366x768 is original preferred mode, only smaller mode is set
				 * to new preferred mode.*/
				if ((mode->hdisplay > hdisplay) || (mode->vdisplay > vdisplay))
					continue;

				mode->type |= DRM_MODE_TYPE_PREFERRED;
				set_next = false;
				//pr_err("set preferred: " DRM_MODE_FMT "\n",  DRM_MODE_ARG(mode));
		}
	}

	return 0;
}


int innodpu_modes_drop_repeat(struct drm_connector *connector)
{
	struct drm_display_mode *mode1 = NULL;
	struct drm_display_mode *mode2 = NULL;
	struct drm_display_mode *m = NULL;
	int nums = 0;

	list_for_each_entry(mode1, &connector->probed_modes, head) {
		list_for_each_entry_safe(mode2, m, &connector->probed_modes, head) {
			if (!mode1 || !mode2 || mode1 == mode2)
				continue;

			if (innodpu_modes_match(mode1, mode2)) {
				list_del(&mode2->head);
				drm_mode_destroy(connector->dev, mode2);
				nums ++;
			}
		}
	}

	return nums;
}

int innodpu_add_modes_without_edid(struct drm_connector *connector, \
		bool is_valid_mode(const struct drm_display_mode *))
{
	int i = 0;
	int num_modes = 0;
	struct drm_display_mode *mode = NULL;
	const struct drm_display_mode *pmode = NULL;

	for (i = 0; i < INNO_ARRAY_SIZE(s_noedid_modes); i++) {
		pmode = &s_noedid_modes[i];

		if (is_valid_mode && !is_valid_mode(pmode)) {
			continue;
		}

		mode = drm_mode_duplicate(connector->dev, pmode);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}

	return num_modes;
}

int innodpu_str_push_edid(const char *buf, struct drm_connector *connector)
{
	struct inno_custom_info_mode {
		int clock;
		u16 hdisplay;
		u16 hsync_start;
		u16 hsync_end;
		u16 htotal;
		u16 hskew;
		u16 vdisplay;
		u16 vsync_start;
		u16 vsync_end;
		u16 vtotal;
		u16 vscan;
		u32 flags;
	};

	int i = 0;
	int num_modes = 0;
	struct drm_display_mode *ptr = NULL;
	struct drm_display_mode *mode = NULL;
	struct inno_custom_info_mode custom_info_mode = {0};
	int size = sizeof(struct inno_custom_info_mode);

	ptr = kzalloc(sizeof(struct drm_display_mode), fh2m_hal_get_inno_gfp_kernel());
	if (!ptr || IS_ERR(ptr)) {
		fh2m_innodpu_err(connector->dev->dev, "[%s %d] \n", __func__, __LINE__);
		return 0;
	}

	for (i = 0; (i < INNO_EDID_BUF_LEN) && ((INNO_EDID_BUF_LEN - i) >= size); i += size) {
		memset(&custom_info_mode, 0, size);
		memcpy(&custom_info_mode, buf + i, size);
		if (custom_info_mode.clock) {
			memset(ptr, 0, sizeof(struct drm_display_mode));
			ptr->clock       = be32_to_cpu(custom_info_mode.clock);
			ptr->hdisplay    = be16_to_cpu(custom_info_mode.hdisplay);
			ptr->hsync_start = be16_to_cpu(custom_info_mode.hsync_start);
			ptr->hsync_end   = be16_to_cpu(custom_info_mode.hsync_end);
			ptr->htotal      = be16_to_cpu(custom_info_mode.htotal);
			ptr->hskew       = be16_to_cpu(custom_info_mode.hskew);
			ptr->vdisplay    = be16_to_cpu(custom_info_mode.vdisplay);
			ptr->vsync_start = be16_to_cpu(custom_info_mode.vsync_start);
			ptr->vsync_end   = be16_to_cpu(custom_info_mode.vsync_end);
			ptr->vtotal      = be16_to_cpu(custom_info_mode.vtotal);
			ptr->vscan       = be16_to_cpu(custom_info_mode.vscan);
			ptr->flags       = be32_to_cpu(custom_info_mode.flags);
			snprintf(ptr->name, sizeof(ptr->name), "%dx%d", ptr->hdisplay, ptr->vdisplay);

			mode = drm_mode_duplicate(connector->dev, ptr);
			if (mode) {
				drm_mode_probed_add(connector, mode);
				++num_modes;
			}
		}
	}

	kfree(ptr);

	return num_modes;
}

static struct connector_map_prop s_connector_map[CUSTOM_OUTPUT_MODE_MAX][CHIP_TYPE_MAX][CONNECTOR_M_MAX] = {
	/* enable pdp combine */
	[CUSTOM_OUTPUT_MODE_1] = {
		[CHIP_G1_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G1P_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, true, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_2] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G1P_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_3] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G1P_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_4] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),

		},
		[CHIP_G1P_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_5] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 0, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_6] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 0, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_7] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 2, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_8] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 0, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_9] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 0, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_10] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 2, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_11] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 0, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_12] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 1, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_13] = {
		[CHIP_G0_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_LVDS, 2048, 2048, 2, false, false),
		},
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 3, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_14] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_15] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_16] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_17] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_18] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_19] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_20] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_21] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_22] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 0, true, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 1, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_23] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_24] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_25] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_26] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_27] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_28] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_29] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_30] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_31] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2048, 2048, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1440, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	/* disable pdp combine */
	[CUSTOM_OUTPUT_MODE_128] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1600, 3, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1600, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_129] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_130] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_131] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_132] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1600, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_133] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1600, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_134] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1600, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_135] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_136] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_137] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP0),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_138] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_139] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI0),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_140] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1600, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 4096, 4096, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_VGA, 1920, 1200, 2, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_141] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 3840, 2160, 0, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 2560, 1440, 1, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2880, 1800, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},

	[CUSTOM_OUTPUT_MODE_142] = {
		[CHIP_G0M_SOC] = {
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI0, 2560, 1440, 1, false, false),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_HDMI1, 3840, 2160, 0, false, false),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI2),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_HDMI3),
			CONNECTOR_MAP_ITEM(CONNECTOR_M_DP0, 2880, 1800, 2, false, true),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_DP1),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_VGA),
			CONNECTOR_MAP_EMPTY_ITEM(CONNECTOR_M_LVDS),
		},
	},
};

static int g0_soc_connector_map_get(int sk_select, int sk_support, int dpu_match)
{
	int ret = 0;
	unsigned long bit_map_sk = 0;

	if (dpu_match > 0 || s_dpu_match > 0) {
		if (s_dpu_match > 0) {
			ret = s_dpu_match;
		} else {
			ret = dpu_match;
			s_dpu_match = ret;
		}
	 } else {
		if (sk_select >= 0) {
			bit_map_sk = sk_select;
		} else {
			bit_map_sk = sk_support;
		}

		if (bit_map_sk & INNO_BIT(CONNECTOR_M_HDMI1)) {
			ret = CUSTOM_OUTPUT_MODE_3;
		} else if (bit_map_sk & INNO_BIT(CONNECTOR_M_HDMI0)) {
			ret = CUSTOM_OUTPUT_MODE_1;
		} else if (bit_map_sk & INNO_BIT(CONNECTOR_M_DP0)) {
			ret = CUSTOM_OUTPUT_MODE_2;
		} else {
			ret = CUSTOM_OUTPUT_MODE_3;
		}
	}

	return ret;
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))

#define ACPI_CONFIG
#define TARGET_PATH "\\_SB_.PCI0.MX12.PEG0"
static const guid_t dpu_uuid =
	GUID_INIT(0xdaffd814, 0x6eba, 0x4d8c,
		  0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01);

static int innogpu_acpi_dpu_match(struct drm_device *drm_dev)
{
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *pkg;
	acpi_handle handle;
	acpi_status status;
	int dpu_match = 0;
	int i, j, last_package;

#ifdef ACPI_CONFIG
	handle = ACPI_HANDLE(drm_dev->dev);
	if (!handle) {
		conn_info(drm_dev->dev, "Failed to get ACPI handle\n");
		return -ENODEV;
	}
#else
	status = acpi_get_handle(NULL, TARGET_PATH, &handle);
	if (ACPI_FAILURE(status)) {
		conn_info(drm_dev->dev, "Failed to get ACPI handle for %s\n", TARGET_PATH);
		return -ENODEV;
	}
#endif

	status = acpi_evaluate_object(handle, "_DSD", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		conn_info(drm_dev->dev, "Failed to evaluate _DSD method\n");
		return -EIO;
	}

	pkg = (union acpi_object *)buf.pointer;
	if (pkg->type != ACPI_TYPE_PACKAGE) {
		conn_info(drm_dev->dev, "Invalid _DSD format\n");
		goto cleanup;
	}

	for (i = 0; i < pkg->package.count; i += 2) {
		const union acpi_object *guid;
		union acpi_object *properties;

		guid = &pkg->package.elements[i];
		properties = &pkg->package.elements[i + 1];

		if (guid->type != ACPI_TYPE_BUFFER ||
		    properties->type != ACPI_TYPE_PACKAGE)
			break;

		if (guid_equal((guid_t *)guid->buffer.pointer, &dpu_uuid)) {
			last_package = properties->package.count - 1;

			for (j = 0; j < properties->package.count; j++) {
				union acpi_object *entry = &properties->package.elements[j];
				union acpi_object *key;
				union acpi_object *value;

				key = &entry->package.elements[0];
				value = &entry->package.elements[1];

				if (j != last_package) {
					if (key->type == ACPI_TYPE_STRING &&
					    !fh2m_inno_strcmp("DPU_ID", key->string.pointer) &&
					    value->type == ACPI_TYPE_BUFFER &&
					    !fh2m_inno_strcmp("1ec8", value->buffer.pointer))
						continue;
					else
						goto cleanup;
				}

				if (j == last_package &&
				    key->type == ACPI_TYPE_STRING &&
				    !fh2m_inno_strcmp("DPU_MATCH", key->string.pointer) &&
				    value->type == ACPI_TYPE_INTEGER) {
					dpu_match = (u32)value->integer.value;
					conn_info(drm_dev->dev, "acpi dpu match success: %u\n", dpu_match);
					break;
				}

				conn_info(drm_dev->dev, "acpi dpu match fail\n");
			}
		}
	}

cleanup:
	kfree(buf.pointer);
	return dpu_match;
}
#else
static int innogpu_acpi_dpu_match(struct drm_device *drm_dev)
{
	return -EINVAL;
}
#endif

static int g0m_soc_connector_map_get(int dpu_match)
{
	int ret = 0;

	if (s_dpu_match || dpu_match <= 0) {
		if (s_dpu_match) {
			ret = s_dpu_match;
		} else {
			ret = CUSTOM_OUTPUT_MODE_8;
		}
	} else {
		ret = dpu_match;
	}

	s_dpu_match = ret;

	return ret;
}

static void innodpu_logo_extime_blacklist(struct drm_device *drm_dev)
{
	struct hw_board_info *match_board = NULL;
	struct hw_board_info board[] = {
		HW_BOARD_INFO_ITEM("ZY", "YF27_1"), //VGA+HDMI2VGA+DP2LVDS
		HW_BOARD_INFO_ITEM("ZY", "YF27_2"), //VGA+HDMI2VGA+DP2VGA
	};

	if (!drm_dev || s_logo_extime <= 0)
		return;

	match_board = innodpu_odm_pcb_match(drm_dev->dev, board, INNO_ARRAY_SIZE(board));
	if (match_board) {
		s_logo_extime = 0;
		conn_info(drm_dev->dev, "match board[%s/%s], No logo delay\n",
				match_board->odm_vendor, match_board->pcb_version);
	}
}

static int innodpu_get_interface_mode(struct device *dev, unsigned int reg_module, int hal_version)
{
	int ret = 0;
	int output_mode = 0;
	unsigned char val = 0;

	if (hal_version >= 0x7) {
		if (reg_module == REG_M_HDMI) {
			ret = fh2m_hal_get_output_mode(dev, 0, &val);
			if (ret < 0)
				val = 0;
		}
		if (reg_module == REG_M_HDMI1) {
			ret = fh2m_hal_get_output_mode(dev, 1, &val);
			if (ret < 0)
				val = 0;
		}
		if (reg_module == REG_M_DP) {
			ret = fh2m_hal_get_output_mode(dev, 2, &val);
			if (ret < 0)
				val = 0;
		}
	} else {
		if (reg_module == REG_M_HDMI)
			ret = fh2m_hal_getflag_hdmi2dvi(dev, 0);
		if (reg_module == REG_M_HDMI1)
			ret = fh2m_hal_getflag_hdmi2dvi(dev, 1);
		if (reg_module == REG_M_DP)
			ret = fh2m_hal_getflag_dp2vga(dev, 0);

		val = (ret > 0) ? 1 : 0;
	}

	if (reg_module == REG_M_HDMI || reg_module == REG_M_HDMI1) {
		switch (val) {
		case 0:
			output_mode = INNO_DRM_MODE_CONNECTOR_HDMIA;
		break;
		case 1:
			output_mode = INNO_DRM_MODE_CONNECTOR_DVII;
		break;
		case 3:
			output_mode = INNO_DRM_MODE_CONNECTOR_DisplayPort;
		break;
		case 4:
			output_mode = INNO_DRM_MODE_CONNECTOR_LVDS;
		break;
		case 2:
		case 5:
		case 6:
			output_mode = INNO_DRM_MODE_CONNECTOR_VGA;
		break;
		}
	} else if (reg_module == REG_M_DP) {
		switch (val) {
		case 0:
			output_mode = INNO_DRM_MODE_CONNECTOR_DisplayPort;
		break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			output_mode = INNO_DRM_MODE_CONNECTOR_VGA;
		break;
		case 9:
			output_mode = INNO_DRM_MODE_CONNECTOR_eDP;
		break;
		case 10:
		case 11:
		case 12:
			output_mode = INNO_DRM_MODE_CONNECTOR_LVDS;
		break;
		}
	}

	return output_mode;
}

static void innodpu_detect_interface_type(struct device *dev, struct disp_interface_info *interface,
										  int hal_version, unsigned int interface_en)
{
	int hdmi_output_mode = 0;
	int hdmi1_output_mode = 0;
	int dp_output_mode = 0;

	if (!interface)
		return;

	if (interface_en & BIT(0)) {
		hdmi_output_mode = innodpu_get_interface_mode(dev, REG_M_HDMI, hal_version);
		interface->interface_type = (hdmi_output_mode != 0) ? hdmi_output_mode : INNO_DRM_MODE_CONNECTOR_HDMIA;
	} else if (interface_en & BIT(1)) {
		hdmi1_output_mode = innodpu_get_interface_mode(dev, REG_M_HDMI1, hal_version);
		interface->interface_type = (hdmi1_output_mode != 0) ? hdmi1_output_mode : INNO_DRM_MODE_CONNECTOR_HDMIA;
	} else if (interface_en & BIT(2)) {
		dp_output_mode = innodpu_get_interface_mode(dev, REG_M_DP, hal_version);
		interface->interface_type = (dp_output_mode != 0) ? dp_output_mode : INNO_DRM_MODE_CONNECTOR_DisplayPort;
	} else if (interface_en & BIT(3)) {
		interface->interface_type = INNO_DRM_MODE_CONNECTOR_VGA;
	} else {
		interface->interface_type = INNO_DRM_MODE_CONNECTOR_Unknown;
	}
}

static void innodpu_get_interface_max_resolution(int custom, chip_type_e plat,
												 struct disp_interface_info *interface, unsigned int interface_en)
{
	if (interface_en & BIT(0)) {
		if (s_connector_map[custom][plat][CONNECTOR_M_HDMI0].max_width == 4096 ||
			s_connector_map[custom][plat][CONNECTOR_M_HDMI0].max_width == 3840) {
			interface->max_hdisp = 3840;
			interface->max_vdisp = 2160;
			interface->max_refresh = 60;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_HDMI0].max_width == 2048) {
			interface->max_hdisp = 1920;
			interface->max_vdisp = 1200;
			interface->max_refresh = 120;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_HDMI0].max_width == 2560) {
			interface->max_hdisp = 2560;
			interface->max_vdisp = 1440;
			interface->max_refresh = 100;
		}
	} else if (interface_en & BIT(1)) {
		if (s_connector_map[custom][plat][CONNECTOR_M_HDMI1].max_width == 4096 ||
			s_connector_map[custom][plat][CONNECTOR_M_HDMI1].max_width == 3840) {
			interface->max_hdisp = 3840;
			interface->max_vdisp = 2160;
			interface->max_refresh = 60;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_HDMI1].max_width == 2048) {
			interface->max_hdisp = 1920;
			interface->max_vdisp = 1200;
			interface->max_refresh = 120;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_HDMI1].max_width == 2560) {
			interface->max_hdisp = 2560;
			interface->max_vdisp = 1440;
			interface->max_refresh = 100;
		}
	} else if (interface_en & BIT(2)) {
		if (s_connector_map[custom][plat][CONNECTOR_M_DP0].max_width == 4096) {
			interface->max_hdisp = 3840;
			interface->max_vdisp = 2160;
			interface->max_refresh = 60;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_DP0].max_width == 2048) {
			interface->max_hdisp = 1920;
			interface->max_vdisp = 1200;
			interface->max_refresh = 120;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_DP0].max_width == 2560) {
			interface->max_hdisp = 2560;
			interface->max_vdisp = 1440;
			interface->max_refresh = 100;
		} else if (s_connector_map[custom][plat][CONNECTOR_M_DP0].max_width == 2880) {
			interface->max_hdisp = 2880;
			interface->max_vdisp = 1800;
			interface->max_refresh = 60;
		}
	} else if (interface_en & BIT(3)) {
		if (s_connector_map[custom][plat][CONNECTOR_M_VGA].max_width != 0) {
			interface->max_hdisp = 1920;
			interface->max_vdisp = 1200;
			interface->max_refresh = 60;
		}
	}
}

static void innodpu_clear_interface_en_bit(unsigned int *interface_en)
{
	if (*interface_en & BIT(0)) {
		*interface_en &= ~BIT(0);
	} else if (*interface_en & BIT(1)) {
		*interface_en &= ~BIT(1);
	} else if (*interface_en & BIT(2)) {
		*interface_en &= ~BIT(2);
	} else if (*interface_en & BIT(3)) {
		*interface_en &= ~BIT(3);
	}
}

static void innodpu_fill_interface_info(struct drm_device *drm_dev,
											  struct innodpu_drm_private *dev_priv,
											  int custom, chip_type_e plat)
{
	int i;
	int hal_version = 0;
	int interface_cnt = 0;
	unsigned int interface_en = 0x0; // BIT(0): HDMI0, BIT(1): HDMI1, BIT(2): DP0, BIT(3): VGA
	unsigned int tmp = 0x0;
	void *interface_info = NULL;
	struct device *dev = drm_dev->dev;

	interface_en = fh2m_hal_detect_registered_interface(dev);

	/* TODO: others plat */
	if (plat == CHIP_G0M_SOC) {
		if (0 == s_connector_map[custom][CHIP_G0M_SOC][CONNECTOR_M_HDMI0].possible_crtc)
			interface_en &= ~BIT(0);
		if (0 == s_connector_map[custom][CHIP_G0M_SOC][CONNECTOR_M_HDMI1].possible_crtc)
			interface_en &= ~BIT(1);
		if (0 == s_connector_map[custom][CHIP_G0M_SOC][CONNECTOR_M_DP0].possible_crtc)
			interface_en &= ~BIT(2);
		if (0 == s_connector_map[custom][CHIP_G0M_SOC][CONNECTOR_M_VGA].possible_crtc)
			interface_en &= ~BIT(3);
	}

	tmp = interface_en;
	while (tmp) {
		if (tmp & BIT(0))
			interface_cnt++;
		tmp >>= 1;
	}
	fh2m_hal_set_interface_nums(dev, interface_cnt);

	interface_info = kzalloc(sizeof(struct disp_interface_info) * interface_cnt, fh2m_hal_get_inno_gfp_kernel());
	if (interface_info) {
		dev_priv->interface_info = interface_info;
	} else {
		conn_err(dev, "interface_info mem alloc failed.\n");
		dev_priv->interface_info = NULL;
		return;
	}
	fh2m_hal_set_interface_info(dev, interface_info);

	hal_version = fh2m_hal_hwinfo_version(dev);
	for (i = 0; i < interface_cnt; i++) {
		struct disp_interface_info interface;
		innodpu_detect_interface_type(dev, &interface, hal_version, interface_en);
		innodpu_get_interface_max_resolution(custom, plat, &interface, interface_en);
		fh2m_hal_append_interface_info(dev, i, &interface);

		innodpu_clear_interface_en_bit(&interface_en);
	}
}

static int custom_mode = 0;
int innodpu_custom_init(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	int i;
	chip_type_e plat;
	int retcode = 0;
	int dpu_match = 0;
	int sk_select = 0;
	int sk_support = 0;
	int ret = 0;
	plat_data_t *drm_plat_data = NULL;
	struct device *dev = drm_dev->dev;

	ret = innogpu_drm_get_platdata(drm_dev, &drm_plat_data);
	if (ret) {
		return ret;
	};

	plat = fh2m_hal_get_chiptype(drm_dev->dev);
	sk_select = fh2m_hal_select_4k(drm_dev->dev);
	sk_support = fh2m_hal_is_support_4k(drm_dev->dev);
	dpu_match = innogpu_acpi_dpu_match(drm_dev);
	if (dpu_match <= 0)
		dpu_match = fh2m_hal_get_dpu_match(drm_dev->dev);

	innodpu_logo_extime_blacklist(drm_dev);

	conn_info(dev, "select 4k: %d, support 4k:%d, dpu_match:%d", sk_select, sk_support, dpu_match);

	switch (plat) {
	case CHIP_G1_SOC:
		conn_info(dev, "DPU start init g1 soc.\n");
		s_dpu_nums = 0xc;
		custom_mode = CUSTOM_OUTPUT_MODE_1;
		dev_priv->role_id = drm_plat_data->dev_idx * 2;
	break;
	case CHIP_G1P_SOC:
		conn_info(dev, "DPU start init g1p soc.\n");
		s_dpu_nums = 0x3;
		custom_mode = CUSTOM_OUTPUT_MODE_1;
		dev_priv->role_id = drm_plat_data->dev_idx * 2;
	break;
	case CHIP_G0_SOC:
		conn_info(dev, "DPU start init g0 soc.\n");
		s_dpu_nums = 0x07;
		dev_priv->role_id = 0;
		custom_mode = g0_soc_connector_map_get(sk_select, sk_support, dpu_match);
	break;
	case CHIP_G0M_SOC:
		conn_info(dev, "DPU start init g0m soc.\n");
		s_dpu_nums = 0x0f;
		dev_priv->role_id = 0;
		custom_mode = g0m_soc_connector_map_get(dpu_match);
	break;
	case CHIP_G3_SOC:
	case CHIP_G3_PAL:
	case CHIP_G3_NE:
		conn_info(dev, "DPU start init g3.\n");
		dev_priv->role_id = 0;
	break;
	default:
		conn_err(drm_dev->dev, "DPU does not currently support %d platform.\n", plat);
		retcode = -EINVAL;
	break;
	}

	if (custom_mode >= CUSTOM_OUTPUT_MODE_MAX ||
		custom_mode <= CUSTOM_OUTPUT_MODE_0) {
		conn_err(drm_dev->dev, "custom mode parse error!,Use display mode1\n");
		s_dpu_match = custom_mode = CUSTOM_OUTPUT_MODE_1;
	}

	conn_info(dev, "custom_mode:%d", custom_mode);
	for (i = CONNECTOR_M_HDMI0; i <= CONNECTOR_M_HDMI3; i++) {
		hdmi_info(dev, "hdmi-%d max_width:%d max_height:%d possible_crtc:%d", i,
				  s_connector_map[custom_mode][plat][i].max_width,
				  s_connector_map[custom_mode][plat][i].max_height,
				  s_connector_map[custom_mode][plat][i].possible_crtc);

	}
	for (i = CONNECTOR_M_DP0; i <= CONNECTOR_M_DP1; i++) {
		dp_info(dev, "dp-%d max_width:%d max_height:%d possible_crtc:%d", i - CONNECTOR_M_DP0,
				  s_connector_map[custom_mode][plat][i].max_width,
				  s_connector_map[custom_mode][plat][i].max_height,
				  s_connector_map[custom_mode][plat][i].possible_crtc);

	}
	vga_info(dev, "vga max_width:%d max_height:%d possible_crtc:%d",
			  s_connector_map[custom_mode][plat][CONNECTOR_M_VGA].max_width,
			  s_connector_map[custom_mode][plat][CONNECTOR_M_VGA].max_height,
			  s_connector_map[custom_mode][plat][CONNECTOR_M_VGA].possible_crtc);
	lvds_info(dev, "lvds max_width:%d max_height:%d possible_crtc:%d",
			  s_connector_map[custom_mode][plat][CONNECTOR_M_LVDS].max_width,
			  s_connector_map[custom_mode][plat][CONNECTOR_M_LVDS].max_height,
			  s_connector_map[custom_mode][plat][CONNECTOR_M_LVDS].possible_crtc);

	/* add connector info to hal api */
	innodpu_fill_interface_info(drm_dev, dev_priv, custom_mode, plat);

	return retcode;
}

void innodpu_custom_fini(struct innodpu_drm_private *dev_priv)
{
	if (dev_priv->interface_info)
		kfree(dev_priv->interface_info);
}

struct connector_map_prop *innodpu_find_connector_map_module(struct device *dev, enum connector_module module)
{
	chip_type_e plat = fh2m_hal_get_chiptype(dev->parent);
	struct connector_map_prop *connector_map_module = NULL;

	switch (plat) {
	case CHIP_G1_SOC:
		connector_map_module = &s_connector_map[custom_mode][CHIP_G1_SOC][module];
	break;
	case CHIP_G1P_SOC:
		connector_map_module = &s_connector_map[custom_mode][CHIP_G1P_SOC][module];
	break;
	case CHIP_G0_SOC:
		connector_map_module = &s_connector_map[custom_mode][CHIP_G0_SOC][module];
	break;
	case CHIP_G0M_SOC:
		connector_map_module = &s_connector_map[custom_mode][CHIP_G0M_SOC][module];
	break;
	case CHIP_G3_SOC:
	case CHIP_G3_PAL:
	case CHIP_G3_NE:
		connector_map_module = &s_connector_map[custom_mode][CHIP_G1_SOC][module];
	break;
	default:
		connector_map_module = NULL;
		fh2m_innodpu_err(dev, "DPU does not currently support %d platform.\n", plat);
	break;
	};

	return connector_map_module;
}

bool innodpu_detect_is_valid_output(struct device *dev, enum connector_module module)
{
	struct connector_map_prop *connector_map_module = NULL;

	connector_map_module = innodpu_find_connector_map_module(dev, module);
	if (NULL == connector_map_module) {
		fh2m_innodpu_err(dev, "connector_map_prop is NULL!\n");
		return false;
	}

	if (!connector_map_module->possible_crtc)
		return false;
	else {
		conn_info(dev, "connector_mode:%d, max mode:%dx%d possible_crtc:%d\n",
				module, connector_map_module->max_width,
				connector_map_module->max_height,
				connector_map_module->possible_crtc);
		return true;
	}
}

int innodpu_get_display_mode(void)
{
	return custom_mode;
}

int innodpu_get_odm_info(struct device *device, char *odm_info, inno_dev *pcie_dev)
{
	int ret = 0;

	ret = fh2m_hal_get_odm_vendor(pcie_dev, odm_info);

	if (ret || (fh2m_inno_strlen(odm_info) == 0)) {
		fh2m_inno_memset(odm_info, '*', HW_ODM_VENDOR_LEN);
		odm_info[HW_ODM_VENDOR_LEN - 1] = '\0';
		fh2m_innodpu_warn(device, "get odm info err, use default odm info[%s]\n", odm_info);
		return ret;
	}

	conn_info(device, "get odm info[%s]\n", odm_info);
	return ret;
}

static int innodpu_get_mfc_name(unsigned char *edid, char mfc_name[MFC_NAME_LEN])
{
	int offset = 'A'-1;

	if (!edid || !mfc_name) {
		return -1;
	}

	mfc_name[0] = ((edid[0x08] >> 2) & 0x1f) + offset;
	mfc_name[1] = (((edid[0x08] << 3) | (edid[0x09] >> 5)) & 0x1f)+ offset;
	mfc_name[2] = (edid[0x09] & 0x1f) + offset;
	mfc_name[3] = '\0';

	return 0;
}

struct resolution_info *innodpu_resolution_match(const inno_drm_display_mode *mode, struct resolution_info *resolution, int count)
{
	int hdisplay = -1;
	int vdisplay = -1;
	int clock    = -1;
	int vrefresh    = -1;

	if (!mode || !resolution)
		goto out;

	hdisplay = fh2m_inno_drm_disp_get_member(hdisplay, mode);
	vdisplay = fh2m_inno_drm_disp_get_member(vdisplay, mode);
	clock    = fh2m_inno_drm_disp_get_member(clock, mode);
	vrefresh    = fh2m_inno_drm_mode_vrefresh(mode);

	while (count--) {
		/* when the value is "-1", it could match anything. or must equal.*/
		if (((resolution->hdisplay == -1) || (resolution->hdisplay == hdisplay)) && \
			((resolution->vdisplay == -1) || (resolution->vdisplay == vdisplay)) && \
			((resolution->vrefresh == -1) || (resolution->vrefresh == vrefresh)) && \
			((resolution->clock == -1) || (resolution->clock == clock)) ) {
			return resolution;
		}

		resolution ++;
	}

out:
	return NULL;
}

struct mfc_monitor_info *innodpu_mfc_monitor_match(unsigned char* edid, struct mfc_monitor_info *monitor_info, int count)
{
	char mfc_name[MFC_NAME_LEN] = {0};
	char monitor_name[MONITOR_NAME_LEN] = {0};

	if (!edid || !monitor_info)
		goto out;

	if (innodpu_get_mfc_name(edid, mfc_name) < 0)
		goto out;

	drm_edid_get_monitor_name((struct edid *)edid, monitor_name, sizeof(monitor_name));

	while (count--) {
		/* when the string is "*", it could match anything. or compare each character.*/
		if ((!strncmp("*", monitor_info->mfc_name, 1) || \
					!strncmp(mfc_name, monitor_info->mfc_name, MFC_NAME_LEN)) &&\
			(!strncmp("*", monitor_info->monitor_name, 1) || \
					!strncmp(monitor_name, monitor_info->monitor_name, MONITOR_NAME_LEN))) {
			return monitor_info;
		}

		monitor_info ++;
	}

out:
	return NULL;
}

struct hw_board_info *innodpu_odm_pcb_match(inno_dev *pcie_dev, struct hw_board_info *board_info, int count)
{
	int ret;
	char odm_vendor[HW_ODM_VENDOR_LEN] = {0};
	char pcb_version[HW_PCB_VERSION_LEN] = {0};

	if (!pcie_dev || !board_info) {
		goto out;
	}

	if (!board_info->odm_vendor || !board_info->pcb_version) {
		goto out;
	}

	ret = fh2m_hal_get_odm_vendor(pcie_dev, odm_vendor);
	if (-1 == ret)
		goto out;

	ret = fh2m_hal_get_pcb_version(pcie_dev, pcb_version);
	if (-1 == ret)
		goto out;

	while (count--) {
		/* when the string is "*", it could match anything. or compare each character.*/
		if ((!strncmp("*", board_info->odm_vendor, 1) || \
					!strncmp(odm_vendor, board_info->odm_vendor, HW_ODM_VENDOR_LEN)) &&\
			(!strncmp("*", board_info->pcb_version, 1) || \
					!strncmp(pcb_version, board_info->pcb_version, HW_PCB_VERSION_LEN))) {
			return board_info;
		}

		/* next */
		board_info ++;
	}

out:
	return NULL;
}

struct drm_display_mode *
innodpu_get_native_mode(struct drm_connector *connector,
		bool (*filter)(struct drm_connector *pconnector, struct drm_display_mode *pmode, bool scaling_filter))
{
	struct drm_display_mode *mode = NULL;
	struct drm_display_mode *native_mode = NULL;
	struct drm_display_mode *minimum_mode = NULL;
	struct drm_display_mode *preferred_mode = NULL;

	/*
	 * 1. choose Wmax as native mode (less than 2048x2048)
	 * 2. choose potential minimum mode (used as native mode when it is null)
	 * */
	list_for_each_entry(mode, &connector->probed_modes, head) {
		if (mode) {
			if (mode->type & DRM_MODE_TYPE_PREFERRED)
				preferred_mode = mode;
			else
				preferred_mode = NULL;
		} else {
			continue;
		}

		if (!minimum_mode || (minimum_mode->hdisplay > mode->hdisplay)) {
			if (!filter(connector, mode, false))
				minimum_mode = mode;
		}

		if (filter(connector, mode, true))
			continue;

		if (preferred_mode && preferred_mode->hdisplay < INNODPU_COMBINE_WIDTH &&
			preferred_mode->vdisplay < INNODPU_COMBINE_WIDTH) {
			native_mode = preferred_mode;
			break;
		}

		if (!native_mode || (native_mode->hdisplay < mode->hdisplay)) {
			native_mode = mode;
			continue;
		}

		if (native_mode->hdisplay > mode->hdisplay)
			continue;

		if ((native_mode->hdisplay == mode->hdisplay) &&
			(native_mode->vdisplay < mode->vdisplay))
			native_mode = mode;
	}

	if ((!native_mode) && minimum_mode) {
		bool mode_priv_support = filter(connector, minimum_mode, false);
		if (!mode_priv_support) {
			native_mode = minimum_mode;
		}
	}

	return native_mode;
}


static struct drm_display_mode *
innodpu_create_common_mode(struct drm_connector *connector,
		struct drm_display_mode *native_mode, const char *name, int hdisplay, int vdisplay)
{
	struct drm_device *drm_dev = connector->dev;
	struct drm_display_mode *mode = NULL;

	mode = drm_mode_duplicate(drm_dev, native_mode);
	if (mode == NULL)
		return NULL;

	mode->hdisplay = hdisplay;
	mode->vdisplay = vdisplay;
	mode->type &= ~DRM_MODE_TYPE_PREFERRED;
	strscpy(mode->name, name, DRM_DISPLAY_MODE_LEN);

	return mode;
}

static const struct mode_size {
		char name[DRM_DISPLAY_MODE_LEN];
		int w;
		int h;
		bool force;
	} common_modes[] = {
#if 0 // RK
		{  "640x480",  640,  480, false},
		{  "720x480",  720,  480, false},
		{  "800x600",  800,  600, false},
		{ "1024x768", 1024,  768, true},
		{ "1152x864", 1152,  864, true},
		{ "1280x600", 1280,  720, true},
		{ "1280x720", 1280,  720, true},
		{ "1280x768", 1280,  768, true},
		{ "1280x800", 1280,  800, true},
		{ "1280x960", 1280,  960, true},
		{"1280x1024", 1280, 1024, true},
		{ "1360x768", 1360,  768, true},
		{ "1366x768", 1366,  768, true},
		{ "1440x900", 1440,  900, false},
		{"1440x1050", 1440, 1050, false},
		{"1680x1050", 1680, 1050, false},
		{" 1600x900", 1600,  900, false},
		{"1600x1200", 1600, 1200, false},
		{"1680x1050", 1600, 1050, false},
		{"1920x1080", 1920, 1080, false},
		{"1920x1200", 1920, 1200, false}
#else
		{  "640x480",  640,  480, false},
		{  "720x480",  720,  480, false},
		{  "800x600",  800,  600, false},
		{ "1024x768", 1024,  768, true},
		{ "1280x720", 1280,  720, true},
		{ "1280x800", 1280,  800, true},
		{ "1280x960", 1280,  960, true},
		{"1280x1024", 1280, 1024, true},
		/* { "1366x768", 1366,  768, false}, */
		{ "1440x900", 1440,  900, false},
		{"1680x1050", 1680, 1050, false},
		{"1600x900",  1600,  900, false},
		{"1600x1200", 1600, 1200, false},
		{"1920x1080", 1920, 1080, false},
		{"1920x1200", 1920, 1200, false},
		{"2560x1440", 2560, 1440, false},
		{"2560x1600", 2560, 1600, false},
		{"2560x1920", 2560, 1920, false},
		{"2560x2048", 2560, 2048, false}
#endif
	};

int innodpu_connector_add_common_modes(struct drm_connector *connector,
		struct drm_display_mode *native_mode, bool support_force)
{
	struct drm_display_mode *mode = NULL;
	int i;
	int n;
	int num_modes = 0;

	n = INNO_ARRAY_SIZE(common_modes);

	for (i = 0; i < n; i++) {
		struct drm_display_mode *curmode = NULL;
		bool mode_existed = false;

		if (common_modes[i].w > native_mode->hdisplay ||
			common_modes[i].h > native_mode->vdisplay ||
			(common_modes[i].w == native_mode->hdisplay &&
			common_modes[i].h == native_mode->vdisplay))
			continue;

		if (common_modes[i].w < (native_mode->hdisplay >> 2) ||
			common_modes[i].h < (native_mode->vdisplay >> 2))
			continue;

		list_for_each_entry(curmode, &connector->probed_modes, head) {
			if (common_modes[i].w == curmode->hdisplay &&
				common_modes[i].h == curmode->vdisplay) {
				if (support_force && (common_modes[i].force == true)) {
					drm_mode_copy(curmode, native_mode);
					curmode->hdisplay = common_modes[i].w;
					curmode->vdisplay = common_modes[i].h;
					curmode->type &= ~DRM_MODE_TYPE_PREFERRED;
					strscpy(curmode->name, common_modes[i].name, DRM_DISPLAY_MODE_LEN);
				}
				mode_existed = true;
				break;
			}
		}

		if (mode_existed)
			continue;

		mode = innodpu_create_common_mode(connector, native_mode,
				common_modes[i].name, common_modes[i].w,
				common_modes[i].h);
		if (!mode)
			continue;

		drm_mode_probed_add(connector, mode);
		num_modes++;
	}
	return num_modes;
}

static struct connector_output_mode dp_output_mode[] = {
	{DRM_MODE_CONNECTOR_DisplayPort, "DP", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA, "CH7517", CONNECTOR_FLAGS_EN_PLUGIN, 0},
	{DRM_MODE_CONNECTOR_VGA, "CM3166", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA, "CS5212", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA, "RT2166", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA, "LT8711V", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_DisplayPort, "DP", CONNECTOR_FLAGS_NONE, 0},/*reserver id:6, as DP now*/
	{DRM_MODE_CONNECTOR_DisplayPort, "DP", CONNECTOR_FLAGS_NONE, 0},/*reserver id:7, as DP now*/
	{DRM_MODE_CONNECTOR_DisplayPort, "DP", CONNECTOR_FLAGS_NONE, 0},/*reserver id:8, as DP now*/
	{DRM_MODE_CONNECTOR_eDP, "eDP", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_LVDS,"LT7911D", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_LVDS,"CH7511", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_LVDS,"CS5211", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_LVDS,"CH7513A", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_HDMIA, "CS5363", CONNECTOR_FLAGS_NONE, 0},
};

static struct connector_output_mode hdmi_output_mode[] = {
	{DRM_MODE_CONNECTOR_HDMIA, "HDMI", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_DVII,  "DVI", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA,   "CS5213", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_DisplayPort, "CS5801", \
		CONNECTOR_FLAGS_SKIP_DDC | CONNECTOR_FLAGS_LONGER_HPD, 1600},
	{DRM_MODE_CONNECTOR_LVDS,  "LT6911C", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA,  "CS5210", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_VGA,  "LT8511A", CONNECTOR_FLAGS_NONE, 0},
	{DRM_MODE_CONNECTOR_DisplayPort, "LT6711A", \
		CONNECTOR_FLAGS_SKIP_DDC | CONNECTOR_FLAGS_LONGER_HPD, 2300},
};

static int chip_connector_id[CHIP_TYPE_MAX][DPU_CONNECT_NUM] = {
	[CHIP_G1_SOC] = {
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI0, 0),
		CONNECTOR_ID_ITEM(CONNECTOR_M_DP0,   1),
	},
	[CHIP_G0_SOC] = {
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI0, 0),
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI1, 1),
		CONNECTOR_ID_ITEM(CONNECTOR_M_DP0,   2),
		CONNECTOR_ID_ITEM(CONNECTOR_M_LVDS,  3),
	},
	[CHIP_G1P_SOC] = {
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI0, 0),
		CONNECTOR_ID_ITEM(CONNECTOR_M_DP0,   1),
		CONNECTOR_ID_ITEM(CONNECTOR_M_VGA,   2),
	},
	[CHIP_G0M_SOC] = {
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI0, 0),
		CONNECTOR_ID_ITEM(CONNECTOR_M_HDMI1, 1),
		CONNECTOR_ID_ITEM(CONNECTOR_M_DP0,   2),
		CONNECTOR_ID_ITEM(CONNECTOR_M_VGA,   3),
	},
};

bool is_output_type_hdmi(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_HDMIA) {
		return true;
	}

	/*others*/
	return false;
}

bool is_output_type_dvi(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_DVII) {
		return true;
	}

	/*others*/
	return false;
}

bool is_output_type_vga(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_VGA) {
		return true;
	}

	/*others*/
	return false;
}

bool is_output_type_dp(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_DisplayPort) {
		return true;
	}

	/*others*/
	return false;
}

bool is_output_type_edp(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_eDP) {
		return true;
	}

	/*others*/
	return false;
}

bool is_output_type_lvds(struct connector_output_mode *om)
{
	if (!om)
		return false;

	if (om->mode_connector_item == DRM_MODE_CONNECTOR_LVDS) {
		return true;
	}

	/*others*/
	return false;
}

bool is_flags_skip_ddc_detect(struct connector_output_mode *om)
{
	bool skip = false;

	if (!om)
		return skip;

	if (om->flags & CONNECTOR_FLAGS_SKIP_DDC) {
		skip = true;
	}

	return skip;
}

bool is_flags_wait_longer_hpd(struct connector_output_mode *om)
{
	bool wait = false;

	if (!om)
		return wait;

	if (om->flags & CONNECTOR_FLAGS_LONGER_HPD) {
		wait = true;
	}

	return wait;
}

bool is_flags_data_en_plugin(struct connector_output_mode *om)
{
	bool en = false;

	if (!om)
		return en;

	if (om->flags & CONNECTOR_FLAGS_EN_PLUGIN) {
		en = true;
	}

	return en;
}


static int innodpu_get_connector_id(inno_dev *dev, unsigned int reg_module)
{
	chip_type_e plat;
	int index = 0;

	if (!dev)
		return -EINVAL;

	plat = fh2m_hal_get_chiptype(dev);

	switch (reg_module) {
	case REG_M_HDMI:
		index = CONNECTOR_M_HDMI0;
	break;
	case REG_M_HDMI1:
		index = CONNECTOR_M_HDMI1;
	break;
	case REG_M_DP:
		index = CONNECTOR_M_DP0;
	break;
	case REG_M_VGA:
		index = CONNECTOR_M_VGA;
	break;
	case REG_M_LVDS:
		index = CONNECTOR_M_LVDS;
	break;
	default:
		index = DPU_CONNECT_NUM;
		fh2m_innodpu_err(dev, "unknown reg_module\n");
	break;
	};

	if (plat >= CHIP_TYPE_MAX ||
		index >= DPU_CONNECT_NUM) {
		fh2m_innodpu_err(dev, "unknown connector id\n");
		return -EFAULT;
	}

	return chip_connector_id[plat][index];
}

extern int s_dp_vga;
extern int s_hdmi_dvi;
static int innodpu_get_output_index(inno_dev *dev, unsigned int reg_module)
{
	inno_dev *pdev = NULL;
	unsigned char val = 0;
	int connector_id = 0, ret = 0, version = 0;

	if (!dev)
		return -EINVAL;

	pdev  = fh2m_inno_dev_get_parent(dev);
	if (!pdev)
		return -EFAULT;

	version = fh2m_hal_hwinfo_version(pdev);
	if (version < 0) {
		fh2m_innodpu_err(dev, "hwinfo version failed\n");
		return -EFAULT;
	}

	if (version >= 0x7) {
		connector_id = innodpu_get_connector_id(pdev, reg_module);
		if (connector_id < 0) {
			fh2m_innodpu_err(dev, "connector_id failed!!!\n");
			return -EINVAL;
		}

		ret = fh2m_hal_get_output_mode(pdev, connector_id, &val);
		if (ret < 0) {
			fh2m_innodpu_err(dev, "fh2m_hal_get_output_mode failed, Using 0\n");
			val = 0;
		}
	} else {
		if (reg_module == REG_M_HDMI) {
			if (s_hdmi_dvi) {
				ret = (s_hdmi_dvi & INNO_BIT(0)) ? 1 : 0;
			} else {
				ret = fh2m_hal_getflag_hdmi2dvi(pdev, 0);
			}
		} else if (reg_module == REG_M_HDMI1) {
			if (s_hdmi_dvi) {
				ret = (s_hdmi_dvi & INNO_BIT(1)) ? 1 : 0;
			} else {
				ret = fh2m_hal_getflag_hdmi2dvi(pdev, 1);
			}
		} else if (reg_module == REG_M_DP) {
			if (s_dp_vga == 1)
				ret = 1;
			else
				ret = fh2m_hal_getflag_dp2vga(pdev, 0);
		}

		if (ret > 0)
			val = 1;
		else
			val = 0;
	}

	conn_info(dev, "connector_id:%d output_id:%d\n", connector_id, val);

	return val;
}

struct connector_output_mode *innodpu_get_connector_output_mode(inno_dev *dev,
		unsigned int reg_module)
{
	int index = 0;
	struct connector_output_mode *output_mode = NULL;

	if (!dev)
		return NULL;

	index = innodpu_get_output_index(dev, reg_module);
	if (index < 0) {
		fh2m_innodpu_err(dev, "get_output_mode failed!!!, Using the default output:0\n");
		index = 0;
	}

	if (reg_module == REG_M_HDMI ||
		reg_module == REG_M_HDMI1) {
		if (index < INNO_ARRAY_SIZE(hdmi_output_mode))
			output_mode = &hdmi_output_mode[index];
		else
			output_mode = &hdmi_output_mode[0]; //default
	} else if (reg_module == REG_M_DP) {
		if (index < INNO_ARRAY_SIZE(dp_output_mode))
			output_mode = &dp_output_mode[index];
		else
			output_mode = &dp_output_mode[0];   //default
	} else {
		output_mode = NULL;
	}

	return output_mode;
}

int innodpu_get_connector_backlight_mode(inno_dev *dev, unsigned int reg_module)
{
	inno_dev *pdev = NULL;
	unsigned char mode = 0;
	int connector_id = 0, ret = 0, version = 0;

	if (!dev)
		return -EINVAL;

	pdev  = fh2m_inno_dev_get_parent(dev);
	if (!pdev)
		return -EFAULT;

	version = fh2m_hal_hwinfo_version(pdev);
	if (version < 0) {
		fh2m_innodpu_err(dev, "hwinfo version failed, Use default DDCCI\n");
		return CONNECTOR_BACKLIGHT_DDCCI;
	}

	if (version >= 0x7) {
		connector_id = innodpu_get_connector_id(pdev, reg_module);
		if (connector_id < 0) {
			fh2m_innodpu_err(dev, "connector_id failed, Use default DDCCI\n");
			return CONNECTOR_BACKLIGHT_DDCCI;
		}

		ret = fh2m_hal_get_backlight_mode(pdev, connector_id, &mode);
		if (ret < 0) {
			fh2m_innodpu_err(dev, "fh2m_hal_get_backlight_mode failed, Use default DDCCI\n");
			mode = CONNECTOR_BACKLIGHT_DDCCI;
		}
	} else {
		mode = CONNECTOR_BACKLIGHT_DDCCI;
	}

	conn_info(dev, "backlight_mode:%d\n", mode);

	return mode;
}

static int range_pixel_clock_khz(struct edid *edid, u8 *t)
{
	/* unspecified */
	if (t[9] == 0 || t[9] == 255)
		return 0;

	/* 1.4 with CVT support gives us real precision, yay */
	if (edid->revision >= 4 && t[10] == 0x04)
		return (t[9] * 10000) - ((t[12] >> 2) * 250);

	/* 1.3 is pathetic, so fuzz up a bit */
	return t[9] * 10000 + 5001;
}

#define EDID_DETAILED_TIMINGS 4
#define EDID_DETAIL_MONITOR_RANGE 0xfd
int innodpu_detailed_block_monitor_range_replace(u8 *raw_edid)
{
	u8 csum = 0;
	int i, clock;
	struct detailed_timing *timing = NULL;
	struct detailed_non_pixel *data = NULL;
	struct edid *edid = (struct edid *)raw_edid;
	struct detailed_data_monitor_range *range = NULL;

	if (edid == NULL)
		return -1;

	for (i = 0; i < EDID_DETAILED_TIMINGS; i++) {
		timing = &(edid->detailed_timings[i]);
		data = &timing->data.other_data;

		if (data->type != EDID_DETAIL_MONITOR_RANGE)
			continue;

		clock = range_pixel_clock_khz(edid, (u8*)timing);
		range = &data->data.range;
		if (clock > 533000 &&
			clock < 594000) {

			/* Replace Max Supported pixel clock rate in MHz/10 */
			if (edid->revision >= 4 && range->flags == 0x04) {
				range->pixel_clock_mhz = 60;
				range->formula.cvt.data1 = 0;
			} else {
				range->pixel_clock_mhz = 60;
			}

			fh2m_inno_printk("Max Supported pixel clock rate replace:old-%d khz, new-%d khz\n",
					clock, range_pixel_clock_khz(edid, (u8 *)timing));

			/* Adjust edid checksum to 0 */
			for (i = 0; i < EDID_LENGTH; i++)
				csum += raw_edid[i];

			if (csum) {
				if (csum <= edid->checksum)
					edid->checksum -= csum;
				else
					edid->checksum += 256 - csum;
			}
		}

		return 0;
	}

	return -1;
}

int innodpu_conn_get_monitor_max_clk(u8 *raw_edid)
{
	int i, clock;
	struct detailed_timing *timing = NULL;
	struct detailed_non_pixel *data = NULL;
	struct edid *edid = (struct edid *)raw_edid;

	if (edid == NULL)
		return -1;

	for (i = 0; i < EDID_DETAILED_TIMINGS; i++) {
		timing = &(edid->detailed_timings[i]);
		data = &timing->data.other_data;

		if (data->type != EDID_DETAIL_MONITOR_RANGE)
			continue;

		clock = range_pixel_clock_khz(edid, (u8*)timing);
		return clock;
	}

	return -1;
}
#undef EDID_DETAILED_TIMINGS
#undef EDID_DETAIL_MONITOR_RANGE

#ifdef CONFIG_DRM_INNO_AUDIO
void innodpu_connector_clear_eld(struct drm_connector *connector)
{
	memset(connector->eld, 0, sizeof(connector->eld));

	connector->latency_present[0] = false;
	connector->latency_present[1] = false;
	connector->video_latency[0] = 0;
	connector->audio_latency[0] = 0;
	connector->video_latency[1] = 0;
	connector->audio_latency[1] = 0;
}
#endif

int innodpu_connector_cnt_detect(struct drm_device *drm_dev)
{
	int count = 0;
	struct drm_connector *connector = NULL;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(drm_dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->status == inno_connector_status_connected)
			count++;
	}
	drm_connector_list_iter_end(&conn_iter);

	return count;
}
