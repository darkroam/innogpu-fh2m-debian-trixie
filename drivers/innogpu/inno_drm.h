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
#ifndef __INNO_DRM_H__
#define __INNO_DRM_H__

#include <linux/types.h>
#include "inno_plat_dev.h"

typedef void inno_drm_file;
typedef void inno_drm_device;
typedef void inno_drm_scdc;
typedef void inno_drm_connector;
typedef void inno_edid;
typedef void inno_drm_crtc;
typedef void inno_drm_plane;
typedef void inno_drm_plane_state;
typedef void inno_drm_framebuffer;
typedef void inno_drm_format_info;
typedef void inno_drm_crtc_state;
typedef void inno_drm_connector_state;
typedef void inno_videomode;
typedef void inno_drm_mode_object;
typedef void inno_drm_gem_object;
typedef void inno_drm_dp_aux;
typedef void inno_drm_dp_link;

struct inno_drm_format_name_buf{
	char str[32];
};



void *fh2m_inno_get_drm_file_prvdata(inno_drm_file *file);
void fh2m_inno_set_drm_file_prvdata(inno_drm_file *file, void *data);

bool fh2m_inno_is_support_scdc(inno_drm_scdc *scdc);
bool fh2m_inno_connector_is_support_scdc(inno_drm_connector *connector);

ssize_t fh2m_inno_drm_scdc_read(void *adapter, u8 offset, void *buffer,
		      size_t size);
ssize_t fh2m_inno_drm_scdc_write(void *adapter, u8 offset,
		       const void *buffer, size_t size);
inno_edid *fh2m_inno_drm_do_get_edid(inno_drm_connector *connector,
	int (*get_edid_block)(void *data, u8 *buf, unsigned int block,
			      size_t len), void *data);
int fh2m_inno_drm_edid_header_is_valid(const u8 *raw_edid);
int fh2m_inno_drm_edid_block_checksum(const u8 *raw_edid);
int fh2m_inno_drm_edid_block_valid(u8 *_block, int block_num, bool print_bad_edid,
			  bool *edid_corrupt);
int fh2m_inno_drm_do_probe_ddc_edid(void *data, u8 *buf, unsigned int block, size_t len);
bool fh2m_inno_drm_irq_enabled(inno_drm_device *drm_dev);
bool fh2m_inno_drm_crtc_handle_vblank(inno_drm_crtc *crtc);
void *fh2m_inno_drm_dev_get_dev(inno_drm_device *drm_dev);
void *fh2m_inno_drm_dev_get_prvdata(inno_drm_device *drm_dev);
bool fh2m_inno_dp_drm_helper_hpd_irq_event(inno_drm_device *dev);


inno_drm_device *fh2m_inno_drm_plane_get_drmdev_dev(inno_drm_plane *plane);
inno_drm_device *fh2m_inno_drm_plane_get_drmdev(inno_drm_plane *plane);
char *fh2m_inno_drm_plane_get_name(inno_drm_plane *plane);
void *fh2m_inno_drm_plane_get_drmdev_private(inno_drm_plane *plane);
inno_drm_plane_state *fh2m_inno_drm_plane_get_state(inno_drm_plane *plane);
#define fh2m_inno_drm_plane_get_member(member, plane) fh2m_inno_drm_plane_get_##member(plane)


unsigned int fh2m_inno_drm_plane_state_get_rotation(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_src_w(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_src_h(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_src_x(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_src_y(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_crtc_w(inno_drm_plane_state *state);
unsigned int fh2m_inno_drm_plane_state_get_crtc_h(inno_drm_plane_state *state);
int fh2m_inno_drm_plane_state_get_crtc_x(inno_drm_plane_state *state);
int fh2m_inno_drm_plane_state_get_crtc_y(inno_drm_plane_state *state);
void *fh2m_inno_drm_plane_state_get_fb(inno_drm_plane_state *state);
void *fh2m_inno_drm_plane_state_get_crtc(inno_drm_plane_state *state);
#define fh2m_inno_drm_plane_state_get_member(member, state) fh2m_inno_drm_plane_state_get_##member(state)



unsigned int *fh2m_inno_drm_fb_get_pitches(inno_drm_framebuffer *fb);
unsigned int *fh2m_inno_drm_fb_get_offsets(inno_drm_framebuffer *fb);
uint64_t fh2m_inno_drm_fb_get_modifier(inno_drm_framebuffer *fb);
const inno_drm_format_info *fh2m_inno_drm_fb_get_format_info(inno_drm_framebuffer *fb);
#define fh2m_inno_drm_fb_get_member(member, fb) fh2m_inno_drm_fb_get_##member(fb)



u32  fh2m_inno_drm_framebuffer_get_format(inno_drm_framebuffer *fb);
void fh2m_inno_drm_fb_get_bpp_depth(u32 format, u32 *depth, u32 *bpp);
u32  fh2m_inno_ilog2(u32 n);

unsigned int fh2m_inno_drm_format_info_get_format(const inno_drm_format_info *format);
unsigned char *fh2m_inno_drm_format_info_get_cpp(const inno_drm_format_info *format);
unsigned char fh2m_inno_drm_fb_get_cpp(inno_drm_framebuffer *fb, unsigned char index);

#define fh2m_inno_drm_format_info_get_member(member, format) fh2m_inno_drm_format_info_get_##member(format)



void *fh2m_inno_drm_crtc_get_state(inno_drm_crtc *crtc);
void *fh2m_inno_drm_crtc_get_name(inno_drm_crtc *crtc);
void *fh2m_inno_drm_crtc_get_dev(inno_drm_crtc *crtc);
#define fh2m_inno_drm_crtc_get_member(member, crtc) fh2m_inno_drm_crtc_get_##member(crtc)


void *fh2m_inno_drm_crtc_state_get_adjusted_mode(inno_drm_crtc_state *state);
void *fh2m_inno_drm_crtc_state_get_ctm(inno_drm_crtc_state *state);
void *fh2m_inno_drm_crtc_state_get_gamma_lut(inno_drm_crtc_state *state);
bool fh2m_inno_drm_crtc_state_get_color_mgmt_changed(inno_drm_crtc_state *state);
bool fh2m_inno_drm_crtc_state_get_enable(inno_drm_crtc_state *state);
unsigned int fh2m_inno_drm_crtc_state_get_ctm_baseid(inno_drm_crtc_state *state);
unsigned int fh2m_inno_drm_crtc_state_get_gamma_baseid(inno_drm_crtc_state *state);
bool fh2m_inno_drm_crtc_state_get_active(inno_drm_crtc_state *state);
#define fh2m_inno_drm_crtc_state_get_member(member, crtc) fh2m_inno_drm_crtc_state_get_##member(crtc)



inno_videomode *fh2m_inno_drm_videomode_alloc(void);
void fh2m_inno_drm_videomode_free(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_hactive(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_htotal(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_hfront_porch(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_hback_porch(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_hsync_len(inno_videomode *mode);
int fh2m_inno_drm_videomode_get_vactive(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_vfront_porch(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_vback_porch(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_vsync_len(inno_videomode *mode);
unsigned int fh2m_inno_drm_videomode_get_flags(inno_videomode *mode);
unsigned long fh2m_inno_drm_videomode_get_pixelclock(inno_videomode *mode);
#define fh2m_inno_drm_videomode_get_member(member, mode) fh2m_inno_drm_videomode_get_##member(mode)

void fh2m_inno_drm_get_format_name(u32 format, struct inno_drm_format_name_buf format_name);


u32 fh2m_INNO_DRM_FORMAT_ARGB2101010_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_ABGR2101010_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGBA1010102_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_BGRA1010102_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_ARGB8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_ABGR8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGBA8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_BGRA8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_XRGB8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_XBGR8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGBX8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_BGRX8888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGB888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_BGR888_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGBA5551_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_ABGR1555_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_RGB565_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_BGR565_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_UYVY_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_YUYV_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_NV12_FUNC(void);
u32 fh2m_INNO_DRM_FORMAT_YUV420_FUNC(void);



#define INNO_DRM_FORMAT_ARGB2101010    fh2m_INNO_DRM_FORMAT_ARGB2101010_FUNC()
#define INNO_DRM_FORMAT_ABGR2101010    fh2m_INNO_DRM_FORMAT_ABGR2101010_FUNC()
#define INNO_DRM_FORMAT_RGBA1010102    fh2m_INNO_DRM_FORMAT_RGBA1010102_FUNC()
#define INNO_DRM_FORMAT_BGRA1010102     fh2m_INNO_DRM_FORMAT_BGRA1010102_FUNC()
#define INNO_DRM_FORMAT_ARGB8888        fh2m_INNO_DRM_FORMAT_ARGB8888_FUNC()
#define INNO_DRM_FORMAT_ABGR8888        fh2m_INNO_DRM_FORMAT_ABGR8888_FUNC()
#define INNO_DRM_FORMAT_RGBA8888       fh2m_INNO_DRM_FORMAT_RGBA8888_FUNC()
#define INNO_DRM_FORMAT_BGRA8888       fh2m_INNO_DRM_FORMAT_BGRA8888_FUNC()
#define INNO_DRM_FORMAT_XRGB8888        fh2m_INNO_DRM_FORMAT_XRGB8888_FUNC()
#define INNO_DRM_FORMAT_XBGR8888        fh2m_INNO_DRM_FORMAT_XBGR8888_FUNC()
#define INNO_DRM_FORMAT_RGBX8888        fh2m_INNO_DRM_FORMAT_RGBX8888_FUNC()
#define INNO_DRM_FORMAT_BGRX8888       fh2m_INNO_DRM_FORMAT_BGRX8888_FUNC()
#define INNO_DRM_FORMAT_RGB888          fh2m_INNO_DRM_FORMAT_RGB888_FUNC()
#define INNO_DRM_FORMAT_BGR888         fh2m_INNO_DRM_FORMAT_BGR888_FUNC()
#define INNO_DRM_FORMAT_RGBA5551        fh2m_INNO_DRM_FORMAT_RGBA5551_FUNC()
#define INNO_DRM_FORMAT_ABGR1555        fh2m_INNO_DRM_FORMAT_ABGR1555_FUNC()
#define INNO_DRM_FORMAT_RGB565         fh2m_INNO_DRM_FORMAT_RGB565_FUNC()
#define INNO_DRM_FORMAT_BGR565          fh2m_INNO_DRM_FORMAT_BGR565_FUNC()
#define INNO_DRM_FORMAT_UYVY         fh2m_INNO_DRM_FORMAT_UYVY_FUNC()
#define INNO_DRM_FORMAT_YUYV          fh2m_INNO_DRM_FORMAT_YUYV_FUNC()
#define INNO_DRM_FORMAT_NV12         fh2m_INNO_DRM_FORMAT_NV12_FUNC()
#define INNO_DRM_FORMAT_YUV420          fh2m_INNO_DRM_FORMAT_YUV420_FUNC()



u32 fh2m_INNO_DISPLAY_FLAGS_HSYNC_HIGH_FUNC(void);
u32 fh2m_INNO_DISPLAY_FLAGS_VSYNC_HIGH_FUNC(void);
u32 fh2m_INNO_DISPLAY_FLAGS_INTERLACED_FUNC(void);


#define INNO_DISPLAY_FLAGS_HSYNC_HIGH   fh2m_INNO_DISPLAY_FLAGS_HSYNC_HIGH_FUNC()
#define INNO_DISPLAY_FLAGS_VSYNC_HIGH   fh2m_INNO_DISPLAY_FLAGS_VSYNC_HIGH_FUNC()
#define INNO_DISPLAY_FLAGS_INTERLACED   fh2m_INNO_DISPLAY_FLAGS_INTERLACED_FUNC()

u32 fh2m_connector_status_connected_func(void);
u32 fh2m_connector_status_disconnected_func(void);
u32 fh2m_connector_status_unknown_func(void);
#define inno_connector_status_unknown		fh2m_connector_status_unknown_func()
#define inno_connector_status_connected		fh2m_connector_status_connected_func()
#define inno_connector_status_disconnected	fh2m_connector_status_disconnected_func()
const char *fh2m_inno_drm_get_connector_status_name(int);

u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_ACK_FUNC(void);
u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_NACK_FUNC(void);
u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_DEFER_FUNC(void);
u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_MASK_FUNC(void);
#define INNO_DP_AUX_NATIVE_REPLY_ACK     fh2m_INNO_DP_AUX_NATIVE_REPLY_ACK_FUNC()
#define INNO_DP_AUX_NATIVE_REPLY_NACK    fh2m_INNO_DP_AUX_NATIVE_REPLY_NACK_FUNC()
#define INNO_DP_AUX_NATIVE_REPLY_DEFER   fh2m_INNO_DP_AUX_NATIVE_REPLY_DEFER_FUNC()
#define INNO_DP_AUX_NATIVE_REPLY_MASK    fh2m_INNO_DP_AUX_NATIVE_REPLY_MASK_FUNC()


u32  fh2m_INNO_DRM_MODE_ROTATE_MASK_FUNC(void);
u32  fh2m_INNO_DRM_MODE_ROTATE_0_FUNC(void);
u32  fh2m_INNO_DRM_MODE_ROTATE_90_FUNC(void);
u32  fh2m_INNO_DRM_MODE_ROTATE_180_FUNC(void);
u32  fh2m_INNO_DRM_MODE_ROTATE_270_FUNC(void);
#define INNO_DRM_MODE_ROTATE_MASK		fh2m_INNO_DRM_MODE_ROTATE_MASK_FUNC()
#define INNO_DRM_MODE_ROTATE_0			fh2m_INNO_DRM_MODE_ROTATE_0_FUNC()
#define INNO_DRM_MODE_ROTATE_90			fh2m_INNO_DRM_MODE_ROTATE_90_FUNC()
#define INNO_DRM_MODE_ROTATE_180		fh2m_INNO_DRM_MODE_ROTATE_180_FUNC()
#define INNO_DRM_MODE_ROTATE_270		fh2m_INNO_DRM_MODE_ROTATE_270_FUNC()

u32  fh2m_INNO_DRM_MODE_REFLECT_X_FUNC(void);
u32  fh2m_INNO_DRM_MODE_REFLECT_Y_FUNC(void);
#define INNO_DRM_MODE_REFLECT_X			fh2m_INNO_DRM_MODE_REFLECT_X_FUNC()
#define INNO_DRM_MODE_REFLECT_Y			fh2m_INNO_DRM_MODE_REFLECT_Y_FUNC()


inno_drm_gem_object *fh2m_inno_drm_gem_object_lookup(inno_drm_file * filp, uint32_t handle);
void fh2m_inno_drm_gem_object_put(void *gem_obj);
unsigned int fh2m_inno_drm_mode_object_get_id(inno_drm_mode_object *obj);

void fh2m_inno_drm_gem_private_object_init(inno_drm_device *dev, inno_drm_gem_object *obj, size_t size);
void fh2m_inno_drm_gem_object_release(inno_drm_gem_object *obj);
int fh2m_inno_drm_gem_handle_create(inno_drm_file *file_priv, inno_drm_gem_object *obj, u32 *handlep);
void fh2m_inno_drm_fb_kick_off_efifb(void);
inno_dev *fh2m_inno_drm_gem_object_get_device(inno_drm_gem_object *obj);
const char *fh2m_inno_drm_get_render_kdev_name(inno_drm_device *drm_dev);
#endif
