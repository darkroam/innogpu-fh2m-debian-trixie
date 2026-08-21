/*************************************************************************/ /*!
@File			g3_ne_hdmi.c
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
CONNECTION WITH THE SOFTWAcRE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#include "innodpu_hdmi.h"
#include "innodpu_hdmi_common.h"
#include "g3_ne_hdmi.h"
#include "gen_g3_ne_hdmi.h"
#include "innodpu_pmbus_fpga.h"

extern bool s_g3_hdmi_hdcp14;

static unsigned int g_pll_map[][11]={
	{0x01, 0xf0, 0x58, 0x10, 0x01, 0x44, 0x01, 0x14, 0x01, 594000, 0}, //594M(3840x2160/4096x2160@60HZ)
	{0x01, 0xf0, 0x63, 0x55, 0x41, 0x42, 0x01, 0x14, 0x01, 148500, 1}, //148.5M(1080P@60HZ)
	{0x01, 0xf0, 0x8a, 0x21, 0x64, 0x01, 0x01, 0x28, 0x03, 85000,  2}, //85M(1366x768@60HZ)
	{0x01, 0xf0, 0x8a, 0x21, 0x64, 0x01, 0x01, 0x28, 0x03, 75000,  3}, //75M(1024x768@70HZ)
	{0x01, 0xf0, 0x58, 0x5a, 0x41, 0x64, 0x01, 0x28, 0x03, 74250,  4}, //74.25M(720P@60HZ/1080i
	{0x01, 0xf0, 0x5c, 0xa5, 0x69, 0x42, 0x01, 0x28, 0x03, 65000,  5}, //65M(1024x768@60HZ)
	{0x01, 0xf0, 0x47, 0x6a, 0x4c, 0x42, 0x01, 0x28, 0x03, 40000,  6}, //40M(800x600@60HZ)
	{0x01, 0xf0, 0x24, 0x5a, 0x41, 0x64, 0x01, 0x28, 0x03, 27000,  7}, //27M(720x480@60HZ/1440x240
	{0x01, 0xf0, 0x58, 0x6f, 0x58, 0x42, 0x01, 0x28, 0x03, 25175,  8}, //25.175M(640x480@60HZ)
};

#if 0
static unsigned char g_datasheet_Akey[][7] = {
	{0x4d, 0xa4, 0x58, 0x8f, 0x13, 0x1e, 0x69}, //Akey[0]
	{0x1f, 0x82, 0x35, 0x58, 0xe6, 0x50, 0x09}, //Akey[1]
	{0x8a, 0x6a, 0x47, 0xab, 0xb9, 0x98, 0x0d}, //Akey[2]
	{0xf3, 0x18, 0x1b, 0x52, 0xcb, 0xc5, 0xca}, //Akey[3]
	{0xfb, 0x14, 0x7f, 0x68, 0x96, 0xd8, 0xb4}, //Akey[4]
	{0xe0, 0x8b, 0xc9, 0x78, 0x48, 0x8f, 0x81}, //Akey[5]
	{0xa0, 0xd0, 0x64, 0xc8, 0x11, 0x2c, 0x41}, //Akey[6]
	{0xb3, 0x9d, 0x5a, 0x28, 0x24, 0x20, 0x44}, //Akey[7]
	{0xb9, 0x28, 0xb2, 0xbd, 0xad, 0x56, 0x6b}, //Akey[8]
	{0x91, 0xa4, 0x7b, 0x4a, 0x6c, 0xe4, 0xf6}, //Akey[9]
	{0x56, 0x00, 0xf8, 0x20, 0x5e, 0x9d, 0x58}, //Akey[10]
	{0x8c, 0x7f, 0xb7, 0x06, 0xee, 0x3f, 0xa0}, //Akey[11]
	{0xc0, 0x2d, 0x8c, 0x9d, 0x7c, 0xbc, 0x28}, //Akey[12]
	{0x56, 0x12, 0x61, 0xe5, 0x4b, 0x9f, 0x05}, //Akey[13]
	{0x74, 0xf0, 0xde, 0x8c, 0xca, 0xc1, 0xcb}, //Akey[14]
	{0x3b, 0xb8, 0xf6, 0x0e, 0xfc, 0xdb, 0x6a}, //Akey[15]
	{0xa0, 0x2b, 0xbb, 0x16, 0xb2, 0x2f, 0xd7}, //Akey[16]
	{0x48, 0x2f, 0x8e, 0x46, 0x78, 0x54, 0x98}, //Akey[17]
	{0x66, 0xae, 0x25, 0x62, 0x27, 0x47, 0x38}, //Akey[18]
	{0x3d, 0x49, 0x52, 0xa3, 0x23, 0xdd, 0xf2}, //Akey[19]
	{0xe2, 0xd2, 0x31, 0x76, 0x7b, 0x3a, 0x54}, //Akey[20]
	{0x4d, 0x58, 0x1a, 0xed, 0xe6, 0x61, 0x25}, //Akey[21]
	{0x32, 0x60, 0x82, 0xbf, 0x7b, 0x22, 0xf7}, //Akey[22]
	{0xf6, 0x1b, 0x46, 0x35, 0x30, 0xce, 0x6b}, //Akey[23]
	{0x36, 0x04, 0x09, 0xf0, 0xd7, 0x97, 0x6b}, //Akey[24]
	{0xa1, 0xe1, 0x05, 0x61, 0x8d, 0x49, 0xf9}, //Akey[25]
	{0xc9, 0x8e, 0x9d, 0xd1, 0x05, 0x34, 0x06}, //Akey[26]
	{0x20, 0xc3, 0x67, 0x94, 0x42, 0x61, 0x90}, //Akey[27]
	{0x96, 0x44, 0x51, 0xce, 0xac, 0x4f, 0xc3}, //Akey[28]
	{0x3e, 0x90, 0x45, 0x04, 0xe1, 0x8c, 0x8a}, //Akey[29]
	{0x29, 0x00, 0x10, 0x57, 0x9c, 0x2d, 0xfc}, //Akey[30]
	{0xd7, 0x94, 0x3b, 0x69, 0xe5, 0xb1, 0x80}, //Akey[31]
	{0x54, 0xc7, 0xea, 0x5b, 0xdd, 0x7b, 0x43}, //Akey[32]
	{0x74, 0xfb, 0x58, 0x87, 0xc7, 0x90, 0xba}, //Akey[33]
	{0x93, 0x5c, 0xfa, 0x36, 0x4e, 0x1d, 0xe0}, //Akey[34]
	{0x03, 0x07, 0x5e, 0x15, 0x9a, 0x11, 0xae}, //Akey[35]
	{0x05, 0xd3, 0x40, 0x8a, 0x78, 0xfb, 0x01}, //Akey[36]
	{0x00, 0x59, 0xa5, 0xd7, 0xa0, 0x4d, 0xb3}, //Akey[37]
	{0x37, 0x3b, 0x63, 0x4a, 0x2c, 0x9e, 0x40}, //Akey[38]
	{0x25, 0x73, 0xbb, 0xb4, 0x56, 0x20, 0x41}, //Akey[39]
};

static unsigned char g_datasheet_Aksv[][5] = {
	{0xb7, 0x03, 0x61, 0xf7, 0x14}, //Aksv[0]
	{0xb7, 0x03, 0x61, 0xf7, 0x14}, //Aksv[1]
};
#endif

static unsigned char g_Akey[][7] = {
	{0x11, 0x79, 0x4b, 0x8b, 0x4b, 0x06, 0x45}, //Akey[0]
	{0xa0, 0xef, 0x39, 0x05, 0xd3, 0x51, 0xbd}, //Akey[1]
	{0xb1, 0x3f, 0x40, 0x4f, 0x25, 0xec, 0xf6}, //Akey[2]
	{0xe0, 0x83, 0xb0, 0xbf, 0x06, 0x04, 0x2d}, //Akey[3]
	{0x08, 0x45, 0x8a, 0x32, 0x1e, 0xef, 0xec}, //Akey[4]
	{0x0f, 0x26, 0x3f, 0x52, 0x7d, 0xb5, 0xf7}, //Akey[5]
	{0x99, 0xbe, 0x80, 0x2c, 0x4f, 0x0f, 0x0f}, //Akey[6]
	{0x42, 0x76, 0x8d, 0x87, 0x0b, 0x57, 0x2a}, //Akey[7]
	{0x06, 0x7e, 0x58, 0xcc, 0x82, 0x9f, 0x70}, //Akey[8]
	{0x0a, 0x9f, 0xff, 0xdc, 0x63, 0x58, 0x3f}, //Akey[9]
	{0xe5, 0x92, 0x07, 0x4e, 0x23, 0x80, 0x93}, //Akey[10]
	{0x00, 0xa2, 0x78, 0xd8, 0xf6, 0x10, 0xc4}, //Akey[11]
	{0x46, 0x16, 0xc2, 0xe6, 0x4d, 0x5b, 0xb8}, //Akey[12]
	{0x64, 0x5a, 0x7d, 0xdc, 0x63, 0x9c, 0x7a}, //Akey[13]
	{0x7e, 0xb2, 0x9b, 0x4b, 0x07, 0x05, 0xa4}, //Akey[14]
	{0x50, 0x31, 0x93, 0x08, 0x86, 0xb4, 0x20}, //Akey[15]
	{0xf7, 0x5d, 0xc9, 0xe1, 0x86, 0x34, 0x40}, //Akey[16]
	{0xcb, 0x0d, 0xe2, 0xca, 0xe7, 0x18, 0x36}, //Akey[17]
	{0x28, 0xb7, 0x9e, 0xcb, 0x27, 0x7d, 0x16}, //Akey[18]
	{0x91, 0xdf, 0xbf, 0xd8, 0xa5, 0x36, 0xb9}, //Akey[19]
	{0x7c, 0xec, 0x08, 0xaf, 0x23, 0x16, 0x3d}, //Akey[20]
	{0x18, 0xdd, 0xef, 0x63, 0x65, 0xa1, 0xef}, //Akey[21]
	{0x4d, 0x8a, 0x87, 0x64, 0x6a, 0x50, 0xce}, //Akey[22]
	{0x98, 0x25, 0xfa, 0x2b, 0x3c, 0x79, 0x78}, //Akey[23]
	{0xdc, 0xb2, 0x48, 0xd9, 0xf9, 0xfa, 0x9a}, //Akey[24]
	{0x35, 0xc2, 0xda, 0x66, 0x7e, 0xde, 0xc0}, //Akey[25]
	{0x8e, 0x37, 0xe6, 0x92, 0x1a, 0x10, 0xff}, //Akey[26]
	{0x4d, 0xed, 0xfe, 0x81, 0xb3, 0x8d, 0xee}, //Akey[27]
	{0xd4, 0xea, 0x70, 0x3c, 0xde, 0x04, 0xd1}, //Akey[28]
	{0xca, 0x1b, 0xe3, 0xe9, 0x61, 0x73, 0xa9}, //Akey[29]
	{0xb6, 0xbf, 0xdd, 0x2e, 0x51, 0xe8, 0xee}, //Akey[30]
	{0x67, 0xfd, 0x47, 0xcb, 0x84, 0x46, 0xdd}, //Akey[31]
	{0x49, 0x01, 0xd7, 0xd0, 0x42, 0x3d, 0xc4}, //Akey[32]
	{0x3c, 0x71, 0x8f, 0xb2, 0xb8, 0x16, 0xf0}, //Akey[33]
	{0x1e, 0x98, 0xe8, 0xc0, 0xd8, 0x97, 0xca}, //Akey[34]
	{0x70, 0xde, 0x9b, 0x78, 0xbd, 0x97, 0xc4}, //Akey[35]
	{0x71, 0x15, 0x08, 0x09, 0x0c, 0x81, 0xa5}, //Akey[36]
	{0xb1, 0xc1, 0x5f, 0x07, 0xd4, 0xdb, 0xa2}, //Akey[37]
	{0x74, 0x24, 0x17, 0xb9, 0x1e, 0x86, 0xb3}, //Akey[38]
	{0xba, 0xbc, 0x9d, 0x51, 0xb9, 0x12, 0xf0}, //Akey[39]
};

static unsigned char g_Aksv[][5] = {
	{0x9d, 0x6d, 0x8d, 0x45, 0xc4}, //Aksv[0]
	{0x9d, 0x6d, 0x8d, 0x45, 0xc4}, //Aksv[1]
};

static void innohdmi_i2c_write(u32 offset, u32 data, struct hdmi_chip_t *chip)
{
	fpga_innopmbus_send_data(chip->i2c_reg, 0x04, offset, data, 0x04);
}

static u32 innohdmi_i2c_read(u32 offset, struct hdmi_chip_t *chip)
{
	return fpga_innopmbus_hdmi_receive_data(chip->i2c_reg, offset, 0x04);
}

static int g3_ne_hdmi_write_reg(u32 entity, u32 value, struct hdmi_chip_t *chip)
{
	int retcode = fh2m_hal_reg_write32(chip->parent, chip->reg_module, entity, value);
	if (retcode) {
		fh2m_innodpu_err(chip->dev, "%s write reg failed:%#x write %#x\n",
			chip->name, entity, value);
	}
	return retcode;
}

static int g3_ne_hdmi_read_reg(u32 entity, struct hdmi_chip_t *chip)
{
	int retcode = 0;
	unsigned int val;


	retcode = fh2m_hal_reg_read32(chip->parent, chip->reg_module, entity, &val);
	if (retcode) {
		fh2m_innodpu_err(chip->dev, "%s read reg failed:%#x retcode %#x\n",
			chip->name, entity, retcode);
	}
	return val;
}

static const struct hdmi_chip_regops_t s_g3_ne_regops = {
	.write = g3_ne_hdmi_write_reg,
	.read  = g3_ne_hdmi_read_reg,
};

static void g3_ne_hdmi_load_akey(struct hdmi_chip_t *chip)
{
	int i, j;

	for (i = 0; i < 40; i++) {
		for (j = 0; j < 7; j++) {
			g3_ne_hdmi_write_reg(REG_ENTITY0152, g_Akey[i][j], chip); //0x98
			fh2m_inno_mdelay(2);
		}
	}
}

static void g3_ne_hdmi_load_aksv1(struct hdmi_chip_t *chip)
{
	int i;

	for (i = 0; i < 5; i++) {
		g3_ne_hdmi_write_reg(REG_ENTITY0152, g_Aksv[0][i], chip); //0x98
		fh2m_inno_mdelay(2);
	}
}

static void g3_ne_hdmi_load_aksv2(struct hdmi_chip_t *chip)
{
	int i;

	for (i = 0; i < 5; i++) {
		g3_ne_hdmi_write_reg(REG_ENTITY0152, g_Aksv[1][i], chip); //0x98
		fh2m_inno_mdelay(2);
	}
}

static void g3_ne_hdmi_check_akey_and_aksv(struct hdmi_chip_t *chip)
{
	u32 value = 0;
	int retry = 100;

	while (retry) {
		value = g3_ne_hdmi_read_reg(REG_ENTITY0084, chip); //0x54
		if (value == 0x1) {
			fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp Akey and Aksv load success\n");
			return;
		}

		fh2m_inno_mdelay(1);
		retry--;
	}

	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp Akey and Aksv load fail\n");
	return;
}

static void g3_ne_hdmi_start_authentication(struct hdmi_chip_t *chip)
{
	/* not skip bksv blacklist check */
	g3_ne_hdmi_write_reg(REG_ENTITY0083, 0x37, chip); //0x53

	/* mask hdcp interrupt */
	g3_ne_hdmi_write_reg(REG_ENTITY0194, 0xff, chip); //0xc2
	g3_ne_hdmi_write_reg(REG_ENTITY0196, 0xff, chip); //0xc4

	/* start hdcp auth */
	g3_ne_hdmi_write_reg(REG_ENTITY0082, 0x96, chip); //0x52
}

static void g3_ne_hdmi_reset(struct hdmi_chip_t *chip)
{
	u32 value = 0;

	fh2m_hal_reg_write32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0001, 0x4); //0x00 bit2-->reset
	fh2m_inno_mdelay(2);
	fh2m_hal_reg_read32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0005, &value); //0x10 读清
	fh2m_inno_mdelay(2);

	fh2m_hal_reg_write32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0001, 0x1); //0x00 bit1-->power on
	fh2m_inno_mdelay(2);
	fh2m_hal_reg_read32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0005, &value); //0x10 读清
	fh2m_inno_mdelay(2);
}

static void g3_ne_hdmi_reset_fixup(struct hdmi_chip_t *chip)
{
	u32 hpd_status = 0;

	fh2m_hal_dev_disable_irq(chip->parent, chip->hal_module);
	g3_ne_hdmi_reset(chip);

	hpd_status = g3_ne_hdmi_read_reg(REG_ENTITY0200, chip); //0xc8
	if (hpd_status & BIT(1)) {
		g3_ne_hdmi_write_reg(REG_ENTITY0200, hpd_status, chip); //write 1 to clear irq
	}

	fh2m_hal_dev_enable_irq(chip->parent, chip->hal_module);
}

static void g3_ne_hdmi_audio_packet_cfg(struct hdmi_chip_t *chip)
{
	g3_ne_hdmi_write_reg(REG_ENTITY0159, 0x08, chip); //9f packet type

	g3_ne_hdmi_write_reg(REG_ENTITY0160, 0x84, chip); //a0 HB0
	g3_ne_hdmi_write_reg(REG_ENTITY0161, 0x01, chip); //a1 HB1
	g3_ne_hdmi_write_reg(REG_ENTITY0162, 0x0a, chip); //a2 HB2
	g3_ne_hdmi_write_reg(REG_ENTITY0163, 0x00, chip); //a3 PB0
	g3_ne_hdmi_write_reg(REG_ENTITY0164, 0x01, chip); //a4 PB1
	g3_ne_hdmi_write_reg(REG_ENTITY0165, 0x0d, chip); //a5 PB2
	g3_ne_hdmi_write_reg(REG_ENTITY0166, 0x00, chip); //a6 PB3
	g3_ne_hdmi_write_reg(REG_ENTITY0167, 0x00, chip); //a7 PB4
	g3_ne_hdmi_write_reg(REG_ENTITY0168, 0x00, chip); //a8 PB5
}

static void g3_ne_hdmi_cfg_dmt_video_format(struct hdmi_chip_t *chip,
		const struct drm_display_mode *mode)
{
	u32 value = 0;

	value = mode->htotal;
	g3_ne_hdmi_write_reg(REG_ENTITY0009, value & 0xFF, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0010, (value >> 8) & 0x1F, chip);

	value = mode->htotal - mode->hdisplay;
	g3_ne_hdmi_write_reg(REG_ENTITY0011, value & 0xFF, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0012, (value >> 8) & 0x7F, chip);

	value = mode->htotal - mode->hsync_start;
	g3_ne_hdmi_write_reg(REG_ENTITY0013, value & 0xFF, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0014, (value >> 8) & 0x03, chip);

	value = mode->hsync_end - mode->hsync_start;
	g3_ne_hdmi_write_reg(REG_ENTITY0015, value & 0xFF, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0016, (value >> 8) & 0x03, chip);

	value = mode->vtotal;
	g3_ne_hdmi_write_reg(REG_ENTITY0017, value & 0xFF, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0018, (value >> 8) & 0x0f, chip);

	value = mode->vtotal - mode->vdisplay;
	g3_ne_hdmi_write_reg(REG_ENTITY0019, value & 0x7F, chip);

	value = mode->vtotal - mode->vsync_start;
	g3_ne_hdmi_write_reg(REG_ENTITY0020, value & 0x7F, chip);

	value = mode->vsync_end - mode->vsync_start;
	g3_ne_hdmi_write_reg(REG_ENTITY0021, value & 0x3F, chip);

	value = INNOHDMI_V_EXTERANL_VIDEO(1);
	value |= mode->flags & DRM_MODE_FLAG_PHSYNC ? INNOHDMI_V_HSYNC_POLARITY(1) :
		INNOHDMI_V_HSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_PVSYNC ? INNOHDMI_V_VSYNC_POLARITY(1) :
		INNOHDMI_V_VSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_INTERLACE ? INNOHDMI_V_INETLACE(1) :
		INNOHDMI_V_INETLACE(0);
	g3_ne_hdmi_write_reg(REG_ENTITY0008, value, chip);

	g3_ne_hdmi_write_reg(REG_ENTITY0048, 0x07, chip);
	g3_ne_hdmi_write_reg(REG_ENTITY0049, 0xff, chip);

	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "Resolution is [%dx%d]\n",
		mode->hdisplay, mode->vdisplay);
}

static void g3_ne_hdmi_audio_cfg(struct hdmi_chip_t *chip)
{
	g3_ne_hdmi_write_reg(REG_ENTITY0053, 0x03, chip); //0x35
	g3_ne_hdmi_write_reg(REG_ENTITY0056, 0x04, chip); //0x38
	g3_ne_hdmi_write_reg(REG_ENTITY0064, 0x18, chip); //0x40
	g3_ne_hdmi_write_reg(REG_ENTITY0065, 0x80, chip); //0x41
}

static int g3_ne_hdmi_setvideo(struct hdmi_chip_t *chip,
	bool test_mode, const struct drm_display_mode *mode)
{
	g3_ne_hdmi_write_reg(REG_ENTITY0000, 0x63, chip);

	g3_ne_hdmi_cfg_dmt_video_format(chip, mode);

	if (test_mode) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "bist mode\n");
		g3_ne_hdmi_write_reg(REG_ENTITY0201, 0x40, chip); //0xc9
	} else {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "normal mode\n");
		g3_ne_hdmi_write_reg(REG_ENTITY0201, 0x50, chip);
	}

	/* audio cfg */
	g3_ne_hdmi_audio_packet_cfg(chip);
	g3_ne_hdmi_audio_cfg(chip);

	g3_ne_hdmi_write_reg(REG_ENTITY0000, 0x61, chip); //0x00
	g3_ne_hdmi_write_reg(REG_ENTITY0206, 0x1, chip);  //0xce

	return 0;
}

static int g3_ne_hdmi_get_pclk_index(int clock)
{
	int index = -1;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(g_pll_map); i++) {
		if (clock == g_pll_map[i][9]) {
			index = g_pll_map[i][10];
			break;
		}
	}

	return index;
}

static void g3_ne_hdmi_disable_pll(struct hdmi_chip_t *chip)
{
	innohdmi_i2c_write(0x050014a0, 0x01, chip); //prepll  disable(0x1a0 bit1)
	innohdmi_i2c_write(0x050014aa, 0x01, chip); //postpll disable(0x1aa bit1)
}

static void g3_ne_hdmi_enable_pll(struct hdmi_chip_t *chip)
{
	innohdmi_i2c_write(0x050014aa, 0x0e, chip); //postpll eanble(0x1a0 bit1)
	innohdmi_i2c_write(0x050014a0, 0x00, chip); //prepll  enable(0x1aa bit1)
	fh2m_inno_mdelay(50);
}

static void g3_ne_set_pll_reg_value(struct hdmi_chip_t *chip, unsigned int index)
{
	/* pre pll */
	innohdmi_i2c_write(0x050014a1, g_pll_map[index][0], chip); //0x1a1
	innohdmi_i2c_write(0x050014a2, g_pll_map[index][1], chip); //0x1a2
	innohdmi_i2c_write(0x050014a3, g_pll_map[index][2], chip); //0x1a3
	innohdmi_i2c_write(0x050014a4, g_pll_map[index][3], chip); //0x1a4
	innohdmi_i2c_write(0x050014a5, g_pll_map[index][4], chip); //0x1a5
	innohdmi_i2c_write(0x050014a6, g_pll_map[index][5], chip); //0x1a6

	/* post pll */
	innohdmi_i2c_write(0x050014ab, g_pll_map[index][6], chip); //0x1ab
	innohdmi_i2c_write(0x050014ac, g_pll_map[index][7], chip); //0x1ac
	innohdmi_i2c_write(0x050014ad, g_pll_map[index][8], chip); //0x1ad
}

static int g3_ne_hdmi_setpll(struct hdmi_chip_t *chip, unsigned int clock)
{
	int pll_index = -1;

	pll_index = g3_ne_hdmi_get_pclk_index(clock);
	if (pll_index < 0) {
		fh2m_innodpu_err(chip->dev, "%s not support clock-%d\n", chip->name, clock);
		return -EINVAL;
	}

	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "pll_index: %d\n", pll_index);
	g3_ne_hdmi_disable_pll(chip);
	g3_ne_set_pll_reg_value(chip, pll_index);
	g3_ne_hdmi_enable_pll(chip);

	return 0;
}

static void g3_ne_hdmi_analog_cfg(struct hdmi_chip_t *chip)
{
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "analog config start\n");

	innohdmi_i2c_write(0x050014b0, 0x0c, chip); //0x1b0 bias
	innohdmi_i2c_write(0x050014cc, 0x0f, chip); //0x1cc rxsense

	/*
	 * termination resistance
	 * 0x87 disable auto calibration
	 * 0x14=20, 4000/20=200Ω
	 */
	innohdmi_i2c_write(0x050014c5, 0x87, chip); //0x1c5
	innohdmi_i2c_write(0x050014c8, 0x14, chip); //0x1c8
	innohdmi_i2c_write(0x050014c9, 0x14, chip); //0x1c9
	innohdmi_i2c_write(0x050014ca, 0x14, chip); //0x1ca
	innohdmi_i2c_write(0x050014cb, 0x14, chip); //0x1cb

	/* mainsel */
	innohdmi_i2c_write(0x050014b5, 0x1e, chip); //0x1b5
	innohdmi_i2c_write(0x050014b6, 0x1e, chip); //0x1b6
	innohdmi_i2c_write(0x050014b7, 0x1e, chip); //0x1b7
	innohdmi_i2c_write(0x050014b8, 0x1e, chip); //0x1b8

	/* isel */
	innohdmi_i2c_write(0x050014bf, 0x22, chip); //0x1bf
	innohdmi_i2c_write(0x050014c0, 0x22, chip); //0x1c0
}

static void g3_ne_hdmi_encoder_modeset(struct hdmi_chip_t *chip,
		int dpu_id, bool test_mode, inno_drm_display_mode *mode)
{
	int retcode = 0;
	u32 wdata = 0x68;
	u32 rdata = 0;
	u8 value = 0;

	innohdmi_i2c_write(0x50015cc, wdata, chip);
	rdata = innohdmi_i2c_read(0x50015cc, chip);
	fh2m_innodpu_err(chip->dev, "#### pmbus r/w test ####\n");
	fh2m_innodpu_err(chip->dev, "write:%#x, read:%#x.\n", wdata, rdata);

	value = 0xaa;
	gen_g3_ne_hdmi_scdc_write(chip, 0x02, &value, 1);
	value = 0;
	gen_g3_ne_hdmi_scdc_read(chip, 0x02, &value, 1);
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "#### scdc r/w test ####\n");
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "write:0xaa, read:%#x.\n", value);

	/* board phy reset */
	innohdmi_i2c_write(0x05001000, 0x03, chip);
	innohdmi_i2c_write(0x05001000, 0x63, chip);
	/* digital reset */
	g3_ne_hdmi_reset_fixup(chip);

	retcode = g3_ne_hdmi_setpll(chip, fh2m_inno_drm_disp_get_member(clock, mode));
	if (retcode ) {
		fh2m_innodpu_err(chip->dev, "hdmi set pll fail\n");
		return;
	}
	g3_ne_hdmi_analog_cfg(chip);

	innohdmi_i2c_write(0x050014b4, 0x07, chip); //0x1b4 ldo on
	innohdmi_i2c_write(0x050014be, 0x71, chip); //0x1be serialize on
	innohdmi_i2c_write(0x050014b2, 0x8f, chip); //0x1b2 tmds driver on

	innohdmi_i2c_write(0x050010c9, 0x54, chip); //board-->normal mode
	innohdmi_i2c_write(0x05001000, 0x61, chip); //board-->digital power on
}

static void g3_ne_hdmi_encoder_disable(struct hdmi_chip_t *chip)
{
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "start\n");

	g3_ne_hdmi_reset_fixup(chip);
	//g3_ne_hdmi_disable_pll(chip);
	chip->hdmi_cfg_hdcp14 = false;
	chip->bksv_pass = false;
}

static void g3_ne_hdmi_encoder_enable(struct hdmi_chip_t *chip, inno_drm_crtc *crtc)
{
	if (chip->adjusted_mode) {
		g3_ne_hdmi_setvideo(chip, chip->test_mode, chip->adjusted_mode);
	}

	if (s_g3_hdmi_hdcp14) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdmi2.0 hdcp14 test!!!\n");
		gen_g3_ne_hdmi_ddc_init(chip);
		fh2m_inno_mdelay(5000);

		g3_ne_hdmi_load_akey(chip);
		g3_ne_hdmi_load_aksv1(chip);
		g3_ne_hdmi_load_aksv2(chip);
		g3_ne_hdmi_check_akey_and_aksv(chip);
		g3_ne_hdmi_start_authentication(chip);
		chip->hdmi_cfg_hdcp14 = true;
	}
}

static inno_drm_mode_status g3_ne_hdmi_connector_mode_valid(struct hdmi_chip_t *chip,
								inno_drm_display_mode *mode)
{
	u16 hdisplay = fh2m_inno_drm_disp_get_member(hdisplay, mode);
	u16 vdisplay = fh2m_inno_drm_disp_get_member(vdisplay, mode);

	if (hdisplay > chip->max_width)
		return INNO_MODE_HSYNC;

	if (vdisplay > chip->max_height)
		return INNO_MODE_VSYNC;

	return INNO_MODE_OK;
}

static int g3_ne_hdmi_hpd_status_detect(struct hdmi_chip_t *chip)
{
	unsigned int hotplug_status = 0;

	hotplug_status = g3_ne_hdmi_read_reg(REG_ENTITY0053, chip); //0xc8

	return !!(hotplug_status & INNOHDMI_HOG_PLUG_VALUE);
}

static int g3_ne_hdmi_hotplug_update(struct hdmi_chip_t *chip)
{

	return inno_connector_status_connected;

	if (g3_ne_hdmi_hpd_status_detect(chip)) {
		hdmi_set_hpg_status(chip, 1);
		return inno_connector_status_connected;
	} else {
		hdmi_set_hpg_status(chip, 0);
		return inno_connector_status_connected;
	}
}

static int g3_ne_hdmi_connector_detect(struct hdmi_chip_t *chip)
{
	return !!hdmi_get_hpg_status(chip) ?  \
			inno_connector_status_connected : \
			inno_connector_status_disconnected;
}

static void g3_ne_hdmi_irq_enable(struct hdmi_chip_t *chip, unsigned int flag)
{
	unsigned int status;

	if (flag & HDMI_IRQ_HPD_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0200, chip); //0xc8
		status |= (BIT(5));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hpd status=0x%x(bit[5])\n", status&BIT(5));
		g3_ne_hdmi_write_reg(REG_ENTITY0200, status, chip);
	}

	if (flag & HDMI_IRQ_EDID_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0192, chip); //0xc0
		status |= (BIT(2));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid status=0x%x(bit[2])\n", status&BIT(2));
		g3_ne_hdmi_write_reg(REG_ENTITY0192, status, chip);
	}

	if (flag & HDMI_IRQ_SCDC_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0264, chip); //0x108
		status |= (BIT(0) | BIT(1));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "scdc status=0x%x(bit[1:0])\n", status&0x3);
		g3_ne_hdmi_write_reg(REG_ENTITY0264, status, chip);
	}

	return;
}

static void g3_ne_hdmi_irq_disable(struct hdmi_chip_t *chip, unsigned int flag)
{
	unsigned int status;

	if (flag & HDMI_IRQ_HPD_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0200, chip); //0xc8
		status &= ~(BIT(5));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hpd status=0x%x(bit[5])\n", status&BIT(5));
		g3_ne_hdmi_write_reg(REG_ENTITY0200, status, chip);
	}

	if (flag & HDMI_IRQ_EDID_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0192, chip); //0xc0
		status &= ~(BIT(2));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid status=0x%x(bit[2])\n", status&BIT(2));
		g3_ne_hdmi_write_reg(REG_ENTITY0192, status, chip);
	}

	if (flag & HDMI_IRQ_SCDC_MASK) {
		status = g3_ne_hdmi_read_reg(REG_ENTITY0264, chip); //0x108
		status &= ~(BIT(0) | BIT(1));
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "scdc status=0x%x(bit[1:0])\n", status&0x3);
		g3_ne_hdmi_write_reg(REG_ENTITY0264, status, chip);
	}

	return;
}

static unsigned int g3_ne_hdmi_irq_handle(struct hdmi_chip_t *chip)
{
	unsigned int hdmi_status = 0;
	unsigned int hpd_status = 0;
	unsigned int edid_status = 0;
	unsigned int scdc_status = 0;
	unsigned int hwi2c_status = 0;

	unsigned int hdcp_status = 0;
	unsigned int hdcp_status0 = 0;
	unsigned int hdcp_status1 = 0;

	u32 value = 0;

	hpd_status = g3_ne_hdmi_read_reg(REG_ENTITY0200, chip); //0xc8
	edid_status = g3_ne_hdmi_read_reg(REG_ENTITY0193, chip); //0xc1
	scdc_status = g3_ne_hdmi_read_reg(REG_ENTITY0265, chip); //0x109
	hwi2c_status = g3_ne_hdmi_read_reg(REG_ENTITY0333, chip); //0x14d

	if (hpd_status & BIT(1)) {
		g3_ne_hdmi_write_reg(REG_ENTITY0200, hpd_status, chip);
		g3_ne_hdmi_hotplug_update(chip);
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hot plug irq-%s\n",
			hpd_status & BIT(7) ? "connect" : "disconnect");
		hdmi_status |= HDMI_IRQ_HPD_MASK;
	}

	if (edid_status) {
		if (chip->chipi2c && chip->chipi2c->edid_inited) {
			chip->chipi2c->edid_status = edid_status;
			fh2m_inno_complete(chip->chipi2c->edid_comp);
		}
		g3_ne_hdmi_write_reg(REG_ENTITY0193, edid_status, chip);
	}

	if (scdc_status & 0x3) {
		if (chip->chipi2c && chip->chipi2c->scdc_inited) {
			chip->chipi2c->scdc_status = scdc_status;
			fh2m_inno_complete(chip->chipi2c->scdc_comp);
		}
		g3_ne_hdmi_write_reg(REG_ENTITY0265, scdc_status, chip);
	}

	if (hwi2c_status & 0x3) {
		if (chip->chipi2c && chip->chipi2c->hwi2c_inited) {
			chip->chipi2c->hwi2c_status = hwi2c_status;
			fh2m_inno_complete(chip->chipi2c->hwi2c_comp);
		}
		g3_ne_hdmi_write_reg(REG_ENTITY0333, ((hwi2c_status & 0x6c) | 0x3), chip);
	}


	if (chip->hdmi_cfg_hdcp14) {
		hdcp_status = g3_ne_hdmi_read_reg(REG_ENTITY0087, chip); //0x57
		hdcp_status0 = g3_ne_hdmi_read_reg(REG_ENTITY0195, chip); //0xc3
		hdcp_status1 = g3_ne_hdmi_read_reg(REG_ENTITY0197, chip); //0xc5
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp status:%#x\n", hdcp_status);
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp status0:%#x\n", hdcp_status0);
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp status1:%#x\n", hdcp_status1);

		if (((hdcp_status0 & 0x10) == 0x10) &&
			((hdcp_status1 & 0x65) == 0x65))
			fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp auth was successfully finished\n");

		if (hdcp_status & BIT(7))
			fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hdcp integrity success\n");

		if (hdcp_status0) {
			g3_ne_hdmi_write_reg(REG_ENTITY0195, hdcp_status0, chip); //0xc3

			if ((hdcp_status0 & BIT(5)) && !chip->bksv_pass) {
				fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "bksv update\n");
				value = g3_ne_hdmi_read_reg(REG_ENTITY0082, chip);
				value |= BIT(6); //bksv pass
				g3_ne_hdmi_write_reg(REG_ENTITY0082, value, chip); //0x52
				chip->bksv_pass = true;
			}
		}

		if (hdcp_status1) {
			g3_ne_hdmi_write_reg(REG_ENTITY0197, hdcp_status1, chip); //0xc5
		}
	}

	return hdmi_status;
}

static int g3_ne_hdmi_hw_init(struct hdmi_chip_t *chip)
{
	int retcode = 0;
	u32 value = 0;

	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "%s start.\n", chip->name);

	/* ensure hdmi0 power on */
	fh2m_hal_reg_read32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0002, &value); //0x04 hdmi status
	switch (value) {
	case 0x4:
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "State: Reset, need setting power on.\n");
		break;
	case 0x2:
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "State: Power Off, need setting power on.\n");
		break;
	case 0x1:
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "State: Power On.\n");
		goto out;
	default:
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "State: Unknown, need setting power on.\n");
		break;
	}

	fh2m_hal_reg_write32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0001, 0x1); //0x00 bit0-->power on
	fh2m_inno_mdelay(2);
	fh2m_hal_reg_read32(chip->parent, REG_M_PPU_HDMI0, REG_ENTITY0005, &value); //0x10 读清
	fh2m_inno_mdelay(2);

out:
	return retcode;
}

static void g3_ne_hdmi_hw_fini(struct hdmi_chip_t *chip)
{
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "%s start.\n", chip->name);

	return;
}

int g3_ne_hdmi_chip_init(struct hdmi_chip_t *chip,
		inno_dev *dev, unsigned int hdmi_id)
{
	int retcode = 0;

	if (!chip || (hdmi_id >= fh2m_hal_get_dev_nums(fh2m_inno_dev_get_parent(dev), DEV_HDMI)))
		return -EINVAL;

	chip->name = fh2m_inno_kasprintf(GFP_KERNEL, "g3-ne-innohdmi-%d", hdmi_id);
	if (chip->name == NULL) {
		fh2m_innodpu_err(dev,
			"Alloc g3-ne-innohdmi-%d name failed. Short of memory.\n", hdmi_id);
		return -ENOMEM;
	}

	chip->id = hdmi_id;
	chip->dev = fh2m_inno_get_device(dev);
	chip->parent = fh2m_inno_dev_get_parent(dev);
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "chip init start.\n");
	//chip->drm_dev = drm_dev;//TODO
	chip->max_width = 640;
	chip->max_height = 480;
	chip->replace_timing = false;
	chip->test_mode = false;

	if (gen_g3_ne_hdmi_i2c_init(chip))
		fh2m_innodpu_err(chip->dev, "i2c init failed.\n");

	gen_g3_ne_hdmi_regops_init(chip, &s_g3_ne_regops);

	if (hdmi_id == 0) {
		chip->reg_module = REG_M_HDMI;
		chip->hal_module = HAL_INTERRUPT_HDMI0;
		chip->i2c_reg = innopmbus_base(0);
	}
	chip->possible_crtc = 0xff;

	hdmi_set_hpg_status(chip, 0);

	chip->hw_init = g3_ne_hdmi_hw_init;
	chip->hw_fini = g3_ne_hdmi_hw_fini;
	chip->irq_handle = g3_ne_hdmi_irq_handle;
	chip->irq_enable = g3_ne_hdmi_irq_enable;
	chip->irq_disable = g3_ne_hdmi_irq_disable;

	chip->hpd_status_detect = g3_ne_hdmi_hpd_status_detect;

	chip->encoder_modeset = g3_ne_hdmi_encoder_modeset;
	chip->encoder_disable = g3_ne_hdmi_encoder_disable;
	chip->encoder_enable = g3_ne_hdmi_encoder_enable;

	chip->connector_detect = g3_ne_hdmi_connector_detect;
	chip->connector_mode_valid = g3_ne_hdmi_connector_mode_valid;

	chip->hdmi_edid_read  = gen_g3_ne_hdmi_edid_read;
	//chip->hdmi_edid_parse = g3_ne_hdmi_edid_parse;

	return retcode;
}

void g3_ne_hdmi_chip_fini(struct hdmi_chip_t *chip)
{
	if (!chip)
		return;

	gen_g3_ne_hdmi_i2c_fini(chip);
	gen_g3_ne_hdmi_regops_fini(chip);

	if (chip->name)
		fh2m_inno_kfree(chip->name);

	fh2m_inno_put_device(chip->dev);
}
