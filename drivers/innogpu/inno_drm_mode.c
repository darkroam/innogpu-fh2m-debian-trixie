/*
* Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
* Dual MIT/GPLv2
*
* The contents of this file are subject to the MIT license as set out below.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU General Public License Version 2 ("GPL") in which case the provisions
* of GPL are applicable instead of those above.
*
* If you wish to allow use of your version of this file only under the terms of
* GPL, and not to allow others to use your version of this file under the terms
* of the MIT license, indicate your decision by deleting the provisions above
* and replace them with the notice and other provisions required by GPL as set
* out in the file called "GPL-COPYING" included in this distribution. If you do
* not delete the provisions above, a recipient may use your version of this file
* under the terms of either the MIT license or GPL.
*
* This License is also included in this distribution in the file called
* "MIT-COPYING".
*
* EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
* PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "inno_drm_version.h"
#include <drm/drm.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_modes.h>
#include <drm/drm_crtc_helper.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 1, 0))
#include <drm/drm_probe_helper.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/drm_edid.h>
#endif

#include "inno_misc.h"
#include "inno_drm.h"
#include "inno_drm_mode.h"


inno_drm_mode_status fh2m_INNO_MODE_HSYNC_FUNC(void)
{
	return MODE_HSYNC;
}
INNO_EXT_SYM(fh2m_INNO_MODE_HSYNC_FUNC);

inno_drm_mode_status fh2m_INNO_MODE_NOCLOCK_FUNC(void)
{
	return MODE_NOCLOCK;
}
INNO_EXT_SYM(fh2m_INNO_MODE_NOCLOCK_FUNC);

inno_drm_mode_status fh2m_INNO_MODE_CLOCK_HIGH_FUNC(void)
{
	return MODE_CLOCK_HIGH;
}
INNO_EXT_SYM(fh2m_INNO_MODE_CLOCK_HIGH_FUNC);

inno_drm_mode_status fh2m_INNO_MODE_VSYNC_FUNC(void)
{
	return MODE_VSYNC;
}
INNO_EXT_SYM(fh2m_INNO_MODE_VSYNC_FUNC);

inno_drm_mode_status fh2m_INNO_MODE_BAD_FUNC(void)
{
	return MODE_BAD;
}
INNO_EXT_SYM(fh2m_INNO_MODE_BAD_FUNC);

inno_drm_mode_status fh2m_INNO_MODE_OK_FUNC(void)
{
	return MODE_OK;
}
INNO_EXT_SYM(fh2m_INNO_MODE_OK_FUNC);

inno_hdmi_picture_aspect fh2m_INNO_HDMI_PICTURE_ASPECT_16_9_FUNC(void)
{
	return HDMI_PICTURE_ASPECT_16_9;
}
INNO_EXT_SYM(fh2m_INNO_HDMI_PICTURE_ASPECT_16_9_FUNC);

unsigned int fh2m_INNO_DRM_MODE_FLAG_PHSYNC_FUNC(void)
{
	return DRM_MODE_FLAG_PHSYNC;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_FLAG_PHSYNC_FUNC);

unsigned int fh2m_INNO_DRM_MODE_FLAG_PVSYNC_FUNC(void)
{
	return DRM_MODE_FLAG_PVSYNC;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_FLAG_PVSYNC_FUNC);

unsigned int  fh2m_INNO_DRM_MODE_FLAG_INTERLACE_FUNC(void)
{
	return DRM_MODE_FLAG_INTERLACE;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_FLAG_INTERLACE_FUNC);

int fh2m_inno_drm_mode_vrefresh(const inno_drm_display_mode *mode)
{
    return drm_mode_vrefresh(mode);
}
INNO_EXT_SYM(fh2m_inno_drm_mode_vrefresh);

unsigned char fh2m_inno_drm_match_cea_mode(const inno_drm_display_mode *to_match)
{
	return drm_match_cea_mode(to_match);
}
INNO_EXT_SYM(fh2m_inno_drm_match_cea_mode);

bool fh2m_inno_drm_helper_hpd_irq_event(inno_drm_device *dev)
{
	struct drm_device *d = (struct drm_device *)dev;

	return drm_helper_hpd_irq_event(d);
}
INNO_EXT_SYM(fh2m_inno_drm_helper_hpd_irq_event);

void fh2m_inno_drm_disp_mode_free(inno_drm_display_mode *mode)
{
	kfree(mode);
}
INNO_EXT_SYM(fh2m_inno_drm_disp_mode_free);


inno_drm_display_mode *fh2m_inno_drm_disp_mode_alloc(char *name,
                                    int clk, int hdisplay, int hsync_start,
                                    int hsync_end, int htotal, int hskew,
                                    int vdisplay, int vsync_start, int vsync_end,
                                    int vtotal, int vscan)
{
	struct drm_display_mode *mode;
	struct drm_display_mode md = {DRM_MODE("default", DRM_MODE_TYPE_DRIVER, clk, hdisplay, hsync_start, hsync_end,
                                     htotal, hskew, vdisplay, vsync_start, vsync_end, vtotal, vscan,
                                     DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
                                     .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,};

	if (!name){
          	printk(KERN_ERR "fh2m_inno_drm_disp_mode_alloc: name is NULL");
		return NULL;
        }
	mode = (struct drm_display_mode *)
			kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	strncpy(md.name, name, sizeof(md.name));
	md.name[sizeof(md.name) - 1] = '\0';
	memcpy(mode, &md, sizeof(struct drm_display_mode));

	return mode;
}
INNO_EXT_SYM(fh2m_inno_drm_disp_mode_alloc);

#define DEF_DISP_MODE_GET_MEMBER_FUNC(MEMBER)  \
unsigned int fh2m_inno_drm_disp_get_##MEMBER(const inno_drm_display_mode *mode) \
{  \
    const struct drm_display_mode *md = (const struct drm_display_mode *)mode;  \
    return md->MEMBER;  \
}  \
INNO_EXT_SYM(fh2m_inno_drm_disp_get_##MEMBER);

#define DEF_DISP_MODE_SET_MEMBER_FUNC(MEMBER)  \
unsigned int fh2m_inno_drm_disp_set_##MEMBER(inno_drm_display_mode *mode, unsigned int val) \
{  \
    struct drm_display_mode *md = (struct drm_display_mode *)mode;  \
    return md->MEMBER = val;  \
}  \
INNO_EXT_SYM(fh2m_inno_drm_disp_set_##MEMBER);


DEF_DISP_MODE_GET_MEMBER_FUNC(htotal);
DEF_DISP_MODE_GET_MEMBER_FUNC(hdisplay);
DEF_DISP_MODE_GET_MEMBER_FUNC(vtotal);
DEF_DISP_MODE_GET_MEMBER_FUNC(vdisplay);
DEF_DISP_MODE_GET_MEMBER_FUNC(hsync_start);
DEF_DISP_MODE_GET_MEMBER_FUNC(hsync_end);
DEF_DISP_MODE_GET_MEMBER_FUNC(vsync_start);
DEF_DISP_MODE_GET_MEMBER_FUNC(vsync_end);
DEF_DISP_MODE_GET_MEMBER_FUNC(clock);
DEF_DISP_MODE_GET_MEMBER_FUNC(flags);
DEF_DISP_MODE_GET_MEMBER_FUNC(type);
DEF_DISP_MODE_GET_MEMBER_FUNC(hskew);
DEF_DISP_MODE_GET_MEMBER_FUNC(vscan);


DEF_DISP_MODE_SET_MEMBER_FUNC(htotal);
DEF_DISP_MODE_SET_MEMBER_FUNC(hdisplay);
DEF_DISP_MODE_SET_MEMBER_FUNC(vtotal);
DEF_DISP_MODE_SET_MEMBER_FUNC(vdisplay);
DEF_DISP_MODE_SET_MEMBER_FUNC(hsync_start);
DEF_DISP_MODE_SET_MEMBER_FUNC(hsync_end);
DEF_DISP_MODE_SET_MEMBER_FUNC(vsync_start);
DEF_DISP_MODE_SET_MEMBER_FUNC(vsync_end);
DEF_DISP_MODE_SET_MEMBER_FUNC(clock);
DEF_DISP_MODE_SET_MEMBER_FUNC(flags);
DEF_DISP_MODE_SET_MEMBER_FUNC(type);
DEF_DISP_MODE_SET_MEMBER_FUNC(hskew);
DEF_DISP_MODE_SET_MEMBER_FUNC(vscan);
