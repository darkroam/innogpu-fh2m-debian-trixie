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

#ifndef __INNO_DRM_MODE___H__
#define __INNO_DRM_MODE___H__

//#include <linux/types.h>
//#include "inno_misc.h"

typedef void inno_drm_display_mode;
typedef unsigned int inno_drm_mode_status;
typedef unsigned int inno_hdmi_picture_aspect;

inno_drm_mode_status fh2m_INNO_MODE_NOCLOCK_FUNC(void);
inno_drm_mode_status fh2m_INNO_MODE_CLOCK_HIGH_FUNC(void);
inno_drm_mode_status fh2m_INNO_MODE_OK_FUNC(void);
inno_drm_mode_status fh2m_INNO_MODE_HSYNC_FUNC(void);
inno_drm_mode_status fh2m_INNO_MODE_VSYNC_FUNC(void);
inno_drm_mode_status fh2m_INNO_MODE_BAD_FUNC(void);
inno_hdmi_picture_aspect fh2m_INNO_HDMI_PICTURE_ASPECT_16_9_FUNC(void);
unsigned int fh2m_INNO_DRM_MODE_FLAG_PHSYNC_FUNC(void);
unsigned int fh2m_INNO_DRM_MODE_FLAG_PVSYNC_FUNC(void);
unsigned int  fh2m_INNO_DRM_MODE_FLAG_INTERLACE_FUNC(void);


#define INNO_MODE_NOCLOCK                       fh2m_INNO_MODE_NOCLOCK_FUNC()
#define INNO_MODE_CLOCK_HIGH                    fh2m_INNO_MODE_CLOCK_HIGH_FUNC()
#define INNO_MODE_OK                            fh2m_INNO_MODE_OK_FUNC()
#define INNO_MODE_HSYNC                         fh2m_INNO_MODE_HSYNC_FUNC()
#define INNO_MODE_VSYNC                         fh2m_INNO_MODE_VSYNC_FUNC()
#define INNO_MODE_BAD                           fh2m_INNO_MODE_BAD_FUNC()
#define INNO_HDMI_PICTURE_ASPECT_16_9           fh2m_INNO_HDMI_PICTURE_ASPECT_16_9_FUNC()
#define INNO_DRM_MODE_FLAG_PHSYNC               fh2m_INNO_DRM_MODE_FLAG_PHSYNC_FUNC()
#define INNO_DRM_MODE_FLAG_PVSYNC               fh2m_INNO_DRM_MODE_FLAG_PVSYNC_FUNC()
#define INNO_DRM_MODE_FLAG_INTERLACE            fh2m_INNO_DRM_MODE_FLAG_INTERLACE_FUNC()




int fh2m_inno_drm_mode_vrefresh(const inno_drm_display_mode *mode);
unsigned char fh2m_inno_drm_match_cea_mode(const inno_drm_display_mode *to_match);
bool fh2m_inno_drm_helper_hpd_irq_event(inno_drm_device *dev);

unsigned int fh2m_inno_drm_disp_get_htotal(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_hdisplay(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_vtotal(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_vdisplay(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_hdisplay(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_hsync_start(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_hsync_end(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_vsync_start(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_vsync_end(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_clock(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_flags(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_type(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_hskew(const inno_drm_display_mode *mode);
unsigned int fh2m_inno_drm_disp_get_vscan(const inno_drm_display_mode *mode);


unsigned int fh2m_inno_drm_disp_set_htotal(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_hdisplay(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_vtotal(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_vdisplay(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_hdisplay(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_hsync_start(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_hsync_end(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_vsync_start(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_vsync_end(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_clock(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_flags(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_type(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_hskew(inno_drm_display_mode *mode, unsigned int val);
unsigned int fh2m_inno_drm_disp_set_vscan(inno_drm_display_mode *mode, unsigned int val);

inno_drm_display_mode *fh2m_inno_drm_disp_mode_alloc(char *name,
                                    int clk, int hdisplay, int hsync_start,
                                    int hsync_end, int htotal, int hskew,
                                    int vdisplay, int vsync_start, int vsync_end,
                                    int vtotal, int vscan);

void fh2m_inno_drm_disp_mode_free(inno_drm_display_mode *mode);

#define fh2m_inno_drm_disp_get_member(member, mode) fh2m_inno_drm_disp_get_##member(mode)
#define fh2m_inno_drm_disp_set_member(member, val, mode) fh2m_inno_drm_disp_set_##member(mode, val)

#endif
