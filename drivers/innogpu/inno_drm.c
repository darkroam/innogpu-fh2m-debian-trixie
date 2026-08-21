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
#include <linux/file.h>
#include <drm/drm_modes.h>

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif

#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <video/videomode.h>
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
#include <drm/drm_drv.h>
#endif
#if (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
#include <drm/drm_device.h>
#include <drm/drm_gem_framebuffer_helper.h>
#endif
#include <drm/drm_mode.h>
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#include <drm/drm_file.h>
#include <drm/drm_debugfs.h>
#endif
#include <drm/drm_blend.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <drm/drm_edid.h>
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
#include <drm/drm_vblank.h>
#endif
#include <drm/drm_plane_helper.h>
#include <drm/drm_fourcc.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 1, 0))
#include <drm/drm_probe_helper.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_scdc_helper.h>
#elif(DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#include <drm/drm_scdc_helper.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/drm_framebuffer.h>
#endif

#include "inno_misc.h"
#include "inno_drm.h"
#include "inno_debug.h"
#include "inno_plat_dev.h"
#include "inno_drm_version.h"

#if (DRM_VERSION > KERNEL_VERSION(5, 5, 0))
#ifndef ioremap_nocache
#define ioremap_nocache ioremap
#endif

#ifndef devm_ioremap_nocache
#define devm_ioremap_nocache  devm_ioremap
#endif
#endif

void *fh2m_inno_get_drm_file_prvdata(inno_drm_file *file)
{
	return ((struct drm_file *)file)->driver_priv;
}
INNO_EXT_SYM(fh2m_inno_get_drm_file_prvdata);

void fh2m_inno_set_drm_file_prvdata(inno_drm_file *file, void *data)
{
	((struct drm_file *)file)->driver_priv = data;
}
INNO_EXT_SYM(fh2m_inno_set_drm_file_prvdata);

bool fh2m_inno_is_support_scdc(inno_drm_scdc *scdc)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
	return ((struct drm_scdc *)scdc)->supported;
#else
	return true;
#endif
}
INNO_EXT_SYM(fh2m_inno_is_support_scdc);

bool fh2m_inno_connector_is_support_scdc(inno_drm_connector *connector)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
	struct drm_connector *drm_connector = (struct drm_connector *)connector;
	struct drm_scdc *scdc = NULL;

	if (drm_connector)
	{
		scdc = &drm_connector->display_info.hdmi.scdc;
		if (scdc)
			return scdc->supported;
	}

	return false;
#else
	return true;
#endif
}
INNO_EXT_SYM(fh2m_inno_connector_is_support_scdc);

#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
/**
 * DOC: scdc helpers
 *
 * Status and Control Data Channel (SCDC) is a mechanism introduced by the
 * HDMI 2.0 specification. It is a point-to-point protocol that allows the
 * HDMI source and HDMI sink to exchange data. The same I2C interface that
 * is used to access EDID serves as the transport mechanism for SCDC.
 */

#define SCDC_I2C_SLAVE_ADDRESS 0x54

/**
 * drm_scdc_read - read a block of data from SCDC
 * @adapter: I2C controller
 * @offset: start offset of block to read
 * @buffer: return location for the block to read
 * @size: size of the block to read
 *
 * Reads a block of data from SCDC, starting at a given offset.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
ssize_t drm_scdc_read(struct i2c_adapter *adapter, u8 offset, void *buffer,
		      size_t size)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = SCDC_I2C_SLAVE_ADDRESS,
			.flags = 0,
			.len = 1,
			.buf = &offset,
		}, {
			.addr = SCDC_I2C_SLAVE_ADDRESS,
			.flags = I2C_M_RD,
			.len = size,
			.buf = buffer,
		}
	};

	ret = i2c_transfer(adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EPROTO;

	return 0;
}

/**
 * drm_scdc_write - write a block of data to SCDC
 * @adapter: I2C controller
 * @offset: start offset of block to write
 * @buffer: block of data to write
 * @size: size of the block to write
 *
 * Writes a block of data to SCDC, starting at a given offset.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
ssize_t drm_scdc_write(struct i2c_adapter *adapter, u8 offset,
		       const void *buffer, size_t size)
{
	struct i2c_msg msg = {
		.addr = SCDC_I2C_SLAVE_ADDRESS,
		.flags = 0,
		.len = 1 + size,
		.buf = NULL,
	};
	void *data;
	int err;

	data = kmalloc(1 + size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	msg.buf = data;

	memcpy(data, &offset, sizeof(offset));
	memcpy(data + 1, buffer, size);

	err = i2c_transfer(adapter, &msg, 1);

	kfree(data);

	if (err < 0)
		return err;
	if (err != 1)
		return -EPROTO;

	return 0;
}
#endif

ssize_t fh2m_inno_drm_scdc_read(void *adapter, u8 offset, void *buffer,
		      size_t size)
{
	return drm_scdc_read((struct i2c_adapter *)adapter, offset, buffer, size);
}
INNO_EXT_SYM(fh2m_inno_drm_scdc_read);

ssize_t fh2m_inno_drm_scdc_write(void *adapter, u8 offset,
		       const void *buffer, size_t size)
{
	return drm_scdc_write((struct i2c_adapter *)adapter, offset, buffer, size);
}
INNO_EXT_SYM(fh2m_inno_drm_scdc_write);

inno_edid *fh2m_inno_drm_do_get_edid(inno_drm_connector *connector,
	int (*get_edid_block)(void *data, u8 *buf, unsigned int block,
			      size_t len), void *data)
{
    return drm_do_get_edid((struct drm_connector *)connector, get_edid_block, data);
}
INNO_EXT_SYM(fh2m_inno_drm_do_get_edid);

int fh2m_inno_drm_edid_header_is_valid(const u8 *raw_edid)
{
	return drm_edid_header_is_valid(raw_edid);
}
INNO_EXT_SYM(fh2m_inno_drm_edid_header_is_valid);

int fh2m_inno_drm_edid_block_checksum(const u8 *raw_edid)
{
	int i;
	u8 csum = 0;
	for (i = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];

	return csum;
}
INNO_EXT_SYM(fh2m_inno_drm_edid_block_checksum);

int fh2m_inno_drm_edid_block_valid(u8 *_block, int block_num, bool print_bad_edid,
			  bool *edid_corrupt)
{
	return drm_edid_block_valid(_block, block_num, print_bad_edid, edid_corrupt);
}
INNO_EXT_SYM(fh2m_inno_drm_edid_block_valid);

#define DDC_SEGMENT_ADDR 0x30
int fh2m_inno_drm_do_probe_ddc_edid(void *data, u8 *buf, unsigned int block, size_t len)
{
	struct i2c_adapter *adapter = data;
	unsigned char start = block * EDID_LENGTH;
	unsigned char segment = block >> 1;
	unsigned char xfers = segment ? 3 : 2;
	int ret, retries = 5;

	/*
	 * The core I2C driver will automatically retry the transfer if the
	 * adapter reports EAGAIN. However, we find that bit-banging transfers
	 * are susceptible to errors under a heavily loaded machine and
	 * generate spurious NAKs and timeouts. Retrying the transfer
	 * of the individual block a few times seems to overcome this.
	 */
	do {
		struct i2c_msg msgs[] = {
			{
				.addr	= DDC_SEGMENT_ADDR,
				.flags	= 0,
				.len	= 1,
				.buf	= &segment,
			}, {
				.addr	= DDC_ADDR,
				.flags	= 0,
				.len	= 1,
				.buf	= &start,
			}, {
				.addr	= DDC_ADDR,
				.flags	= I2C_M_RD,
				.len	= len,
				.buf	= buf,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(adapter, &msgs[3 - xfers], xfers);

		if (ret == -ENXIO) {
			DRM_DEBUG_KMS("drm: skipping non-existent adapter %s\n",
					adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	return ret == xfers ? 0 : -1;
}
INNO_EXT_SYM(fh2m_inno_drm_do_probe_ddc_edid);

inno_dev *fh2m_inno_drm_dev_get_dev(inno_drm_device *drm_dev)
{
	return ((struct drm_device *)drm_dev)->dev;
}
INNO_EXT_SYM(fh2m_inno_drm_dev_get_dev);

void *fh2m_inno_drm_dev_get_prvdata(inno_drm_device *drm_dev)
{
	return ((struct drm_device *)drm_dev)->dev_private;
}
INNO_EXT_SYM(fh2m_inno_drm_dev_get_prvdata);

bool fh2m_inno_drm_irq_enabled(inno_drm_device *drm_dev)
{
#if (DRM_VERSION <= KERNEL_VERSION(5, 14, 21))
	return ((struct drm_device *)drm_dev)->irq_enabled;
#else
	/* TODO::: noted by liuman 20230426: Temporary method to adapt higher linux kernel version for compliation error, shall discuss with DPU team
	 * drm_device irq_enabled member is valid only when the kernel enable CONFIG_DRM_LEGACY  */
	dev_printk(KERN_ERR, ((struct drm_device *)drm_dev)->dev, "warning: the drm_device irq_enabled issue shall be fixed at the kernel version above 5.14.21\n");
	return false;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_irq_enabled);

bool fh2m_inno_drm_crtc_handle_vblank(inno_drm_crtc *crtc)
{
	return drm_crtc_handle_vblank(crtc);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_handle_vblank);

bool fh2m_inno_dp_drm_helper_hpd_irq_event(inno_drm_device *dev)
{
	return drm_helper_hpd_irq_event(dev);
}
INNO_EXT_SYM(fh2m_inno_dp_drm_helper_hpd_irq_event);

///////////////// drm_plane /////////////////
inno_drm_plane_state *fh2m_inno_drm_plane_get_state(inno_drm_plane *plane)
{
	return ((struct drm_plane *)plane)->state;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_get_state);

inno_drm_device *fh2m_inno_drm_plane_get_drmdev(inno_drm_plane *plane)
{
	return ((struct drm_plane *)plane)->dev;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_get_drmdev);

inno_dev *fh2m_inno_drm_plane_get_drmdev_dev(inno_drm_plane *plane)
{
	return ((struct drm_plane *)plane)->dev->dev;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_get_drmdev_dev);

char *fh2m_inno_drm_plane_get_name(inno_drm_plane *plane)
{
	return ((struct drm_plane *)plane)->name;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_get_name);

void *fh2m_inno_drm_plane_get_drmdev_private(inno_drm_plane *plane)
{
	return ((struct drm_plane *)plane)->dev->dev_private;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_get_drmdev_private);


///////////////// drm_plane_state /////////////////
unsigned int fh2m_inno_drm_plane_state_get_rotation(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->rotation;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_rotation);

unsigned int fh2m_inno_drm_plane_state_get_src_w(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->src_w;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_src_w);

unsigned int fh2m_inno_drm_plane_state_get_src_h(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->src_h;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_src_h);

unsigned int fh2m_inno_drm_plane_state_get_src_y(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->src_y;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_src_y);

unsigned int fh2m_inno_drm_plane_state_get_src_x(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->src_x;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_src_x);

unsigned int fh2m_inno_drm_plane_state_get_crtc_w(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->crtc_w;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_crtc_w);

unsigned int fh2m_inno_drm_plane_state_get_crtc_h(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->crtc_h;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_crtc_h);

int fh2m_inno_drm_plane_state_get_crtc_x(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->crtc_x;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_crtc_x);

int fh2m_inno_drm_plane_state_get_crtc_y(inno_drm_plane_state *state)
{
	return ((struct drm_plane_state *)state)->crtc_y;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_crtc_y);

void* fh2m_inno_drm_plane_state_get_fb(inno_drm_plane_state *state)
{
    return ((struct drm_plane_state *)state)->fb;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_fb);

void* fh2m_inno_drm_plane_state_get_crtc(inno_drm_plane_state *state)
{
    return ((struct drm_plane_state *)state)->crtc;
}
INNO_EXT_SYM(fh2m_inno_drm_plane_state_get_crtc);

///////////////// drm_framebuffer /////////////////
unsigned int *fh2m_inno_drm_fb_get_pitches(inno_drm_framebuffer *fb)
{
    return ((struct drm_framebuffer *)fb)->pitches;
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_pitches);

unsigned int *fh2m_inno_drm_fb_get_offsets(inno_drm_framebuffer *fb)
{
    return ((struct drm_framebuffer *)fb)->offsets;
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_offsets);

uint64_t fh2m_inno_drm_fb_get_modifier(inno_drm_framebuffer *fb)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
    return ((struct drm_framebuffer *)fb)->modifier;
#else
    return ((struct drm_framebuffer *)fb)->modifier[0];
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_modifier);

unsigned char fh2m_inno_drm_fb_get_cpp(inno_drm_framebuffer *fb, unsigned char index)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	const struct drm_format_info *format = ((struct drm_framebuffer *)fb)->format;
	if (index > 4) {
		inno_error("%s not support index %d > 4", __func__, index);
		return 0;
	}

	return format->cpp[index];
#else
	inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
	fh2m_inno_warn_on(1);
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_cpp);

unsigned int fh2m_inno_drm_framebuffer_get_format(inno_drm_framebuffer *fb)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 13, 0))
    return ((struct drm_framebuffer *)fb)->pixel_format;
#else
    inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
    fh2m_inno_warn_on(1);
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_framebuffer_get_format);

void fh2m_inno_drm_fb_get_bpp_depth(u32 format, u32 *depth, u32 *bpp)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 13, 0))
	drm_fb_get_bpp_depth(format, depth, bpp);
#else
	*depth = 0;
	*bpp   = 0;
    inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
    fh2m_inno_warn_on(1);
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_bpp_depth);

u32 fh2m_inno_ilog2(u32 n)
{
	return ilog2(n);
}
INNO_EXT_SYM(fh2m_inno_ilog2);

const inno_drm_format_info *fh2m_inno_drm_fb_get_format_info(inno_drm_framebuffer *fb)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
    return ((struct drm_framebuffer *)fb)->format;
#else
    inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
    fh2m_inno_warn_on(1);
    return NULL;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_fb_get_format_info);

///////////////// drm_format_info /////////////////
//TODO: 参照 inno_drm_fb_format() 做版本兼容
unsigned char *fh2m_inno_drm_format_info_get_cpp(const inno_drm_format_info *format)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
    return ((struct drm_format_info *)format)->cpp;
#else
    inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
    fh2m_inno_warn_on(1);
    return NULL;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_format_info_get_cpp);

unsigned int fh2m_inno_drm_format_info_get_format(const inno_drm_format_info *format)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
    return ((struct drm_format_info *)format)->format;
#else
    inno_error("%s is not supported at current kernel version %d\n", __func__, DRM_VERSION);
    fh2m_inno_warn_on(1);
    return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_format_info_get_format);

///////////////// drm_crtc /////////////////
void *fh2m_inno_drm_crtc_get_state(inno_drm_crtc *crtc)
{
    return ((struct drm_crtc *)crtc)->state;
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_get_state);

void *fh2m_inno_drm_crtc_get_name(inno_drm_crtc *crtc)
{
    return ((struct drm_crtc *)crtc)->name;
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_get_name);

void *fh2m_inno_drm_crtc_get_dev(inno_drm_crtc *crtc)
{
    return ((struct drm_crtc *)crtc)->dev;
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_get_dev);


///////////////// drm_crtc_state /////////////////
bool fh2m_inno_drm_crtc_state_get_enable(inno_drm_crtc_state *state)
{
	return (((struct drm_crtc_state *)state)->enable);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_enable);

bool fh2m_inno_drm_crtc_state_get_color_mgmt_changed(inno_drm_crtc_state *state)
{
    return (((struct drm_crtc_state *)state)->color_mgmt_changed);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_color_mgmt_changed);

void *fh2m_inno_drm_crtc_state_get_adjusted_mode(inno_drm_crtc_state *state)
{
    return &(((struct drm_crtc_state *)state)->adjusted_mode);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_adjusted_mode);

bool fh2m_inno_drm_crtc_state_get_active(inno_drm_crtc_state *state)
{
    return (((struct drm_crtc_state *)state)->active);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_active);

unsigned int fh2m_inno_drm_crtc_state_get_ctm_baseid(inno_drm_crtc_state *state)
{
    return (((struct drm_crtc_state *)state)->ctm->base.id);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_ctm_baseid);

unsigned int fh2m_inno_drm_crtc_state_get_gamma_baseid(inno_drm_crtc_state *state)
{
    return (((struct drm_crtc_state *)state)->gamma_lut->base.id);
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_gamma_baseid);

void *fh2m_inno_drm_crtc_state_get_ctm(inno_drm_crtc_state *state)
{
    return ((struct drm_crtc_state *)state)->ctm;
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_ctm);

void *fh2m_inno_drm_crtc_state_get_gamma_lut(inno_drm_crtc_state *state)
{
    return ((struct drm_crtc_state *)state)->gamma_lut;
}
INNO_EXT_SYM(fh2m_inno_drm_crtc_state_get_gamma_lut);

///////////////// videomode /////////////////
inno_videomode *fh2m_inno_drm_videomode_alloc(void)
{
	struct videomode *mode;

	mode = (struct videomode *)
			kzalloc(sizeof(struct videomode), GFP_KERNEL);
	if (!mode)
		return NULL;

	return mode;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_alloc);

void fh2m_inno_drm_videomode_free(inno_videomode *mode)
{
	kfree(mode);
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_free);

unsigned long fh2m_inno_drm_videomode_get_pixelclock(inno_videomode *mode)
{
    return ((struct videomode *)mode)->pixelclock;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_pixelclock);

unsigned int fh2m_inno_drm_videomode_get_htotal(inno_videomode *mode)
{
	struct videomode *tmp_mode = (struct videomode *)mode;

	unsigned int htotal = tmp_mode->hactive + tmp_mode->hfront_porch +
			tmp_mode->hback_porch + tmp_mode->hsync_len;
	return htotal;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_htotal);

unsigned int fh2m_inno_drm_videomode_get_hactive(inno_videomode *mode)
{
    return ((struct videomode *)mode)->hactive;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_hactive);

unsigned int fh2m_inno_drm_videomode_get_hfront_porch(inno_videomode *mode)
{
    return ((struct videomode *)mode)->hfront_porch;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_hfront_porch);

unsigned int fh2m_inno_drm_videomode_get_hback_porch(inno_videomode *mode)
{
    return ((struct videomode *)mode)->hback_porch;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_hback_porch);

unsigned int fh2m_inno_drm_videomode_get_hsync_len(inno_videomode *mode)
{
    return ((struct videomode *)mode)->hsync_len;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_hsync_len);

int fh2m_inno_drm_videomode_get_vactive(inno_videomode *mode)
{
    return ((struct videomode *)mode)->vactive;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_vactive);

unsigned int fh2m_inno_drm_videomode_get_vfront_porch(inno_videomode *mode)
{
    return ((struct videomode *)mode)->vfront_porch;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_vfront_porch);

unsigned int fh2m_inno_drm_videomode_get_vback_porch(inno_videomode *mode)
{
    return ((struct videomode *)mode)->vback_porch;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_vback_porch);

unsigned int fh2m_inno_drm_videomode_get_vsync_len(inno_videomode *mode)
{
    return ((struct videomode *)mode)->vsync_len;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_vsync_len);

unsigned int fh2m_inno_drm_videomode_get_flags(inno_videomode *mode)
{
    return ((struct videomode *)mode)->flags;
}
INNO_EXT_SYM(fh2m_inno_drm_videomode_get_flags);

////////////drm_mode_object////////////////
unsigned int fh2m_inno_drm_mode_object_get_id(inno_drm_mode_object *obj)
{
	 return ((struct drm_mode_object *)obj)->id;
}
INNO_EXT_SYM(fh2m_inno_drm_mode_object_get_id);

void fh2m_inno_drm_get_format_name(u32 format, struct inno_drm_format_name_buf format_name)
{
#if (DRM_VERSION <= KERNEL_VERSION(5, 13, 19))
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	drm_get_format_name(format, (struct drm_format_name_buf *)&format_name);
#else
	char *str_buf = NULL;
	str_buf = drm_get_format_name(format);
	strncpy(format_name.str, str_buf, strlen(str_buf));
	if (str_buf) {
		kfree(str_buf);
	}
#endif
#else
    snprintf(format_name.str, sizeof(format_name.str), "%p4cc", &format);
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_get_format_name);

inno_drm_gem_object *fh2m_inno_drm_gem_object_lookup(inno_drm_file * filp, uint32_t handle)
{
#if (DRM_VERSION < KERNEL_VERSION(4, 7, 0))
	return drm_gem_object_lookup((filp)->minor->dev, filp, handle);
#else
	return drm_gem_object_lookup(filp, handle);
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_lookup);

void fh2m_inno_drm_gem_object_put(inno_drm_gem_object *gem_obj)
{
	drm_gem_object_put((struct drm_gem_object *)gem_obj);
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_put);

u32 fh2m_INNO_DRM_FORMAT_ARGB2101010_FUNC(void)
{
	return DRM_FORMAT_ARGB2101010;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_ARGB2101010_FUNC);

u32 fh2m_INNO_DRM_FORMAT_ABGR2101010_FUNC(void)
{
	return DRM_FORMAT_ABGR2101010;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_ABGR2101010_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGBA1010102_FUNC(void)
{
	return DRM_FORMAT_RGBA1010102;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGBA1010102_FUNC);

u32 fh2m_INNO_DRM_FORMAT_BGRA1010102_FUNC(void)
{
	return DRM_FORMAT_BGRA1010102;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_BGRA1010102_FUNC);

u32 fh2m_INNO_DRM_FORMAT_ARGB8888_FUNC(void)
{
	return DRM_FORMAT_ARGB8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_ARGB8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_ABGR8888_FUNC(void)
{
	return DRM_FORMAT_ABGR8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_ABGR8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGBA8888_FUNC(void)
{
	return DRM_FORMAT_RGBA8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGBA8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_BGRA8888_FUNC(void)
{
	return DRM_FORMAT_BGRA8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_BGRA8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_XRGB8888_FUNC(void)
{
	return DRM_FORMAT_XRGB8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_XRGB8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_XBGR8888_FUNC(void)
{
	return DRM_FORMAT_XBGR8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_XBGR8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGBX8888_FUNC(void)
{
	return DRM_FORMAT_RGBX8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGBX8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_BGRX8888_FUNC(void)
{
	return DRM_FORMAT_BGRX8888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_BGRX8888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGB888_FUNC(void)
{
	return DRM_FORMAT_RGB888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGB888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_BGR888_FUNC(void)
{
	return DRM_FORMAT_BGR888;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_BGR888_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGBA5551_FUNC(void)
{
	return DRM_FORMAT_RGBA5551;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGBA5551_FUNC);

u32 fh2m_INNO_DRM_FORMAT_ABGR1555_FUNC(void)
{
	return DRM_FORMAT_ABGR1555;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_ABGR1555_FUNC);

u32 fh2m_INNO_DRM_FORMAT_RGB565_FUNC(void)
{
	return DRM_FORMAT_RGB565;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_RGB565_FUNC);

u32 fh2m_INNO_DRM_FORMAT_BGR565_FUNC(void)
{
	return DRM_FORMAT_BGR565;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_BGR565_FUNC);

u32 fh2m_INNO_DRM_FORMAT_UYVY_FUNC(void)
{
	return DRM_FORMAT_UYVY;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_UYVY_FUNC);

u32 fh2m_INNO_DRM_FORMAT_YUYV_FUNC(void)
{
	return DRM_FORMAT_YUYV;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_YUYV_FUNC);

u32 fh2m_INNO_DRM_FORMAT_NV12_FUNC(void)
{
	return DRM_FORMAT_NV12;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_NV12_FUNC);

u32 fh2m_INNO_DRM_FORMAT_YUV420_FUNC(void)
{
	return DRM_FORMAT_YUV420;
}
INNO_EXT_SYM(fh2m_INNO_DRM_FORMAT_YUV420_FUNC);

u32 fh2m_INNO_DISPLAY_FLAGS_HSYNC_HIGH_FUNC(void)
{
	return DISPLAY_FLAGS_HSYNC_HIGH;
}
INNO_EXT_SYM(fh2m_INNO_DISPLAY_FLAGS_HSYNC_HIGH_FUNC);

u32 fh2m_INNO_DISPLAY_FLAGS_VSYNC_HIGH_FUNC(void)
{
	return DISPLAY_FLAGS_VSYNC_HIGH;
}
INNO_EXT_SYM(fh2m_INNO_DISPLAY_FLAGS_VSYNC_HIGH_FUNC);

u32 fh2m_INNO_DISPLAY_FLAGS_INTERLACED_FUNC(void)
{
	return DISPLAY_FLAGS_INTERLACED;
}
INNO_EXT_SYM(fh2m_INNO_DISPLAY_FLAGS_INTERLACED_FUNC);

u32  fh2m_INNO_DRM_MODE_ROTATE_MASK_FUNC(void)
{
	return DRM_MODE_ROTATE_MASK;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_ROTATE_MASK_FUNC);

u32  fh2m_INNO_DRM_MODE_ROTATE_0_FUNC(void)
{
	return DRM_MODE_ROTATE_0;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_ROTATE_0_FUNC);

u32  fh2m_INNO_DRM_MODE_ROTATE_90_FUNC(void)
{
	return DRM_MODE_ROTATE_90;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_ROTATE_90_FUNC);

u32  fh2m_INNO_DRM_MODE_ROTATE_180_FUNC(void)
{
	return DRM_MODE_ROTATE_180;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_ROTATE_180_FUNC);

u32  fh2m_INNO_DRM_MODE_ROTATE_270_FUNC(void)
{
	return DRM_MODE_ROTATE_270;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_ROTATE_270_FUNC);

u32  fh2m_INNO_DRM_MODE_REFLECT_X_FUNC(void)
{
	return DRM_MODE_REFLECT_X;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_REFLECT_X_FUNC);

u32  fh2m_INNO_DRM_MODE_REFLECT_Y_FUNC(void)
{
	return DRM_MODE_REFLECT_Y;
}
INNO_EXT_SYM(fh2m_INNO_DRM_MODE_REFLECT_Y_FUNC);

u32 fh2m_connector_status_connected_func(void)
{
	return connector_status_connected;
}
INNO_EXT_SYM(fh2m_connector_status_connected_func);

u32 fh2m_connector_status_disconnected_func(void)
{
	return connector_status_disconnected;
}
INNO_EXT_SYM(fh2m_connector_status_disconnected_func);

u32 fh2m_connector_status_unknown_func(void)
{
	return connector_status_unknown;
}
INNO_EXT_SYM(fh2m_connector_status_unknown_func);

const char *fh2m_inno_drm_get_connector_status_name(int status)
{
    return drm_get_connector_status_name(status);
}
INNO_EXT_SYM(fh2m_inno_drm_get_connector_status_name);

u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_ACK_FUNC(void)
{
	return DP_AUX_NATIVE_REPLY_ACK;
}
INNO_EXT_SYM(fh2m_INNO_DP_AUX_NATIVE_REPLY_ACK_FUNC);

u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_NACK_FUNC(void)
{
	return DP_AUX_NATIVE_REPLY_NACK;
}
INNO_EXT_SYM(fh2m_INNO_DP_AUX_NATIVE_REPLY_NACK_FUNC);

u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_DEFER_FUNC(void)
{
	return DP_AUX_NATIVE_REPLY_DEFER;
}
INNO_EXT_SYM(fh2m_INNO_DP_AUX_NATIVE_REPLY_DEFER_FUNC);

u32 fh2m_INNO_DP_AUX_NATIVE_REPLY_MASK_FUNC(void)
{
	return DP_AUX_NATIVE_REPLY_MASK;
}
INNO_EXT_SYM(fh2m_INNO_DP_AUX_NATIVE_REPLY_MASK_FUNC);

void fh2m_inno_drm_gem_private_object_init(inno_drm_device *dev, inno_drm_gem_object *obj, size_t size)
{
	drm_gem_private_object_init((struct drm_device *)dev, (struct drm_gem_object *)obj, size);
}
INNO_EXT_SYM(fh2m_inno_drm_gem_private_object_init);

void fh2m_inno_drm_gem_object_release(inno_drm_gem_object *obj)
{
	drm_gem_object_release((struct drm_gem_object *)obj);
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_release);

int fh2m_inno_drm_gem_handle_create(inno_drm_file *file_priv, inno_drm_gem_object *obj, u32 *handlep)
{
	return drm_gem_handle_create((struct drm_file *)file_priv, (struct drm_gem_object *)obj, handlep);
}
INNO_EXT_SYM(fh2m_inno_drm_gem_handle_create);

#if (DRM_VERSION >= KERNEL_VERSION(6, 0, 0))

#include <linux/aperture.h>
static int drm_fb_helper_remove_conflicting_framebuffers(const char *name)
{
#if (DRM_VERSION >= KERNEL_VERSION(6, 5, 0))
	aperture_remove_all_conflicting_devices(name);
#else
	aperture_remove_all_conflicting_devices(false, name);
#endif
	return 0;
}

#elif (DRM_VERSION >= KERNEL_VERSION(5, 14, 0))

static int drm_fb_helper_remove_conflicting_framebuffers(struct apertures_struct *a,
		const char *name, bool primary)
{
#if IS_REACHABLE(CONFIG_FB)
	return remove_conflicting_framebuffers(a, name, primary);
#else
	return 0;
#endif
}

#endif

void fh2m_inno_drm_fb_kick_off_efifb(void)
{
#if (DRM_VERSION < KERNEL_VERSION(6, 0, 0))
	struct apertures_struct *a;

	a = alloc_apertures(1);
	if (!a) {
		inno_error("[%s][%d] Short of memory,innodrmfb will not take over fbcon !\n", __func__, __LINE__);
		return;
	}

	/* method 1: remove inno_efifb */
	//a->ranges[0].base = fh2m_hal_get_ddrbase(drm_dev->dev);
	//a->ranges[0].size = fh2m_hal_get_ddr_bar_len(drm_dev->dev);

	/* method 2: remove all fb_dev */
	a->ranges[0].base = 0;
	a->ranges[0].size = ~0;

	drm_fb_helper_remove_conflicting_framebuffers(a, "innodrmfb", true);

	kfree(a);
#else
	drm_fb_helper_remove_conflicting_framebuffers("innodrmfb");
#endif
}
INNO_EXT_SYM(fh2m_inno_drm_fb_kick_off_efifb);

inno_dev *fh2m_inno_drm_gem_object_get_device(inno_drm_gem_object *obj)
{
	struct drm_gem_object *gem_obj = (struct drm_gem_object *)obj;
	return gem_obj->dev->dev;
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_get_device);

const char *fh2m_inno_drm_get_render_kdev_name(inno_drm_device *drm_dev)
{
	return ((struct drm_device *)drm_dev)->render->kdev->kobj.name;
}
INNO_EXT_SYM(fh2m_inno_drm_get_render_kdev_name);
