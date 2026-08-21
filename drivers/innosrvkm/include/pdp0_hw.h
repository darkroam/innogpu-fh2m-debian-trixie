/*************************************************************************/ /*!
@File			pdp0_hw.h
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
#ifndef __PDP0_HW_H
#define __PDP0_HW_H

//#include "innodpu_common.h"
#include <linux/mutex.h>

#include "hal_interface.h"
#include "hal.h"
#include "inno_drm.h"
#include "inno_debug.h"
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
#include "inno_interrupt.h"
#include "pdp0_common.h"
#include "innodpu_connector.h"
#include "innodpu_compatibility.h"

#define HW_CURSOR 1
#define PDP0_COLORADJ_NUM_COEFFS	12
#define PDP0_COEFFTAB_NUM_COEFFS	64
#define PDP0_GAMMA_LUT_SIZE	256
#define PDP0_ROTATED_MASK	(DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)
#define PDP0_DEVICE_LV_HAS_3_STRIDES	INNO_BIT(0)
#define PDP0_INVALID_FORMAT_ID	0xff
#define PRIVATE_ROTATION

#define MAX_OUTPUT_CHANNELS	3


/* PDP Alpha table */
#define INNO_PDP_ALPHA_LUT 0xffaa5500

/* PDP Feature list */
#define INNO_PDP_COMBINE INNO_BIT(5)
#define INNO_PDP_DUALLINK INNO_BIT(6)
#define INNO_PDP_ASYNC INNO_BIT(7)
#define INNO_PDP_PVRIC INNO_BIT(8)
#define INNO_PDP_COMBINE_FIX INNO_BIT(9)
#define INNO_PDP_REG_NEW  INNO_BIT(10)
#define INNO_PDP_FUS_SCALER INNO_BIT(11)

enum {
	PDP0_FD_BLOCK = 0,
	PDP0_FS_BLOCK,
	PDP0_FC_BLOCK
};

#define PDP0_FCIRQ_FD	INNO_BIT(20)
#define PDP0_FCIRQ_FS	INNO_BIT(24)

struct pdp0_irq_map {
	u32 irq_mask;
	u32 vsync_irq;
	u32 err_mask;
};

enum pdp0_scaling_coeff_set {
	PDP0_UPSCALING_COEFFS = 1,
	PDP0_DOWNSCALING_1_5_COEFFS = 2,
	PDP0_DOWNSCALING_2_COEFFS = 3,
	PDP0_DOWNSCALING_2_75_COEFFS = 4,
	PDP0_DOWNSCALING_4_COEFFS = 5,
};

enum {
	PDP0_OW1 = INNO_BIT(0),
	PDP0_CURSOR = INNO_BIT(5),
};

struct pdp0_format_id {
	u32 format;					/* DRM fourcc */
	u8 layer;					/* bitmask of layers supporting it */
	u8 id;						/* used internally */
};

#define innodpu_upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define innodpu_lower_32_bits(n) ((u32)((n) & 0xffffffff))
#define innodpu_upper_8_address(n) (innodpu_upper_32_bits(n) & 0xff)


struct pdp0_crtc_config {
	bool bist_value;

	bool is_yuv_fmt;			//  普通回写格式选择：支持YUV和RGB。

	// BIT0-writeback， BIT1-inno writeback,  BIT2-pvric decompress dispaly, BIT3- pvric compress writeback
	uint64_t wb_start_flag;			//  普通回写使能，使能时pvric_value必须为0 注：pdp0回写和pvric同时只能二选一，开启一个。不使用

	// pvric回写-G1使用
	bool pvric_value;			//  pvric回写使能，使能时wb_start_flag必须为0 注：pdp0回写和pvric同时只能二选一，开启一个
	bool pvric_type_value;		//  0 是compress， 1 是decompress， PVRIC回写格式只能为NV12/NV21
	bool pvric_compress_value;
	uint32_t pvric_decomp_addr;

	uint64_t wb_save_frames;	//  回写存文件的帧个数
	int pvric_display_id;		//  PVRIC解压显示: 0 是显示在OW1, 1是显示在OW2， 2是显示在OW1+OW2
	int pvric_decomp_type;		//
	bool is_uv_revs;			//  PVRIC回写寄存器-0x174：BIT[10] 默认是NV12，如果为 1是NV21格式回写
	bool comp_tile_4x16;		//  PVRIC回写寄存器-0x174：BIT[13:12] 默认模式是2*32，为1是4*16
	int set_wm_fd;				//  PVRIC回写的dma-buf位置，后期改为Blob模式，可以一次设置多路
};

struct pdp0_hw_regmap {
	const u8 n_layers;
	const struct pdp0_layer *layers;

	const struct pdp0_irq_map fd_irq_map;
	const struct pdp0_irq_map fs_irq_map;
	const struct pdp0_irq_map fc_irq_map;

	const struct pdp0_format_id *pixel_formats;
	const u8 n_pixel_formats;
};


struct innodpu_pdp0_hw_device {
	unsigned int dpu_id;
	const struct pdp0_hw_regmap map;
	unsigned int modules;
	inno_dev *dev;

	/* plane图层支持的范围，plane check中检测图像大小 */
	u16 min_width;
	u16 max_width;
	u16 min_height;
	u16 max_height;

	bool init_qos;
	/*gamma coloradj 备份   复位后使用*/
	bool coladj_en;
	bool gamma_en;
	/*gamma coloradj 中断更新使用*/
	atomic_t gamma_changed;
	atomic_t coloradj_changed;

	bool pvric_en;

	u32 gamma_coeffs[PDP0_COEFFTAB_NUM_COEFFS];
	u32 coloradj_coeffs[PDP0_COLORADJ_NUM_COEFFS];

	/* 输出位深寄存器 */
	unsigned char  output_width[MAX_OUTPUT_CHANNELS];

	/* 功能支持寄存器，参考：PDP Feature list */
	unsigned int features;

	/*
	 * 如果开启combine，只需要操作父设备即可，否则每一路单独配置
	 */
	void (*enter_config_mode) (struct innodpu_pdp0_hw_device * hwdev);
	void (*leave_config_mode) (struct innodpu_pdp0_hw_device * hwdev);
	bool (*in_config_mode) (struct innodpu_pdp0_hw_device * hwdev, char mode);

	/*
	 * 当前如果在配置模式，禁止进行复位
	 * 当前如果在normal模式，需要同时配置 CFG_MODE | CRST | SRST
	 * 如果开启combine，那么需要连子设备一起reset，否则只reset当前设备
	 */
	void (*reset)(struct innodpu_pdp0_hw_device *hwdev, char mode);

	/*
	 * 初始化：
	 * 因为合并功能开关需要硬件复位，复位后是需要重新初始化，因此将所有硬件初始化代码放在这里,
	 * 作用：配置位深，配置图层alpha，配置图层YUV2RGB， 配置enh
	 * 如果开启combine，那么连子设备一起初始化，否则只初始化当前设备
	 *
	 * 去初始化：关闭中断即可
	 */
	void (*hardware_init)(struct innodpu_pdp0_hw_device *hwdev);
	void (*hardware_fini)(struct innodpu_pdp0_hw_device *hwdev);


	void (*set_config_valid) (struct innodpu_pdp0_hw_device * hwdev, u8 value);
	int (*set_and_wait_config_valid)(struct innodpu_pdp0_hw_device *hwdev);
	void (*modeset) (struct innodpu_pdp0_hw_device * hwdev, inno_videomode * m, bool bistmode);
	int (*rotmem_required) (struct innodpu_pdp0_hw_device * hwdev, u16 w, u16 h, u32 fmt);
	int (*fs_set_scaling_coeffs) (struct innodpu_pdp0_hw_device * hwdev,
								  struct pdp0_se_config * se_config,
								  struct pdp0_se_config * old_config);
	long (*fs_calc_mclk) (struct innodpu_pdp0_hw_device * hwdev,
						  struct pdp0_se_config * se_config, inno_videomode * vm);


	void (*atomic_update_combine)(inno_drm_crtc *crtc);
	void (*atomic_update_gamma)(inno_drm_crtc *crtc, inno_drm_crtc_state *old_state);
	void (*atomic_update_coloradj)(inno_drm_crtc *crtc, inno_drm_crtc_state *old_state);
	void (*atomic_se_config)(inno_drm_crtc *crtc, inno_drm_crtc_state *old_state);

	int (*reg_dump)(inno_seq_file *m, void *data);
	int (*base_info)(struct innodpu_pdp0_hw_device *pdp0_drm, u64 buf[]);
	int (*cur_info)(struct innodpu_pdp0_hw_device *pdp0_drm, u64 buf[]);

	int (*bisttest_show)(inno_seq_file *m, inno_drm_crtc *crtc);
	int (*bisttest_write)(inno_seq_file *m, inno_drm_crtc *crtc, const char __user *buf, size_t size, loff_t *ppos);

	int (*cursor_set2)(inno_drm_crtc *crtc, inno_drm_file *file_priv,
									uint32_t handle, uint32_t width, uint32_t height,
									int32_t hot_x, int32_t hot_y);
	int (*cursor_move)(inno_drm_crtc *crtc, int x, int y);
	int (*cursor_resume)(struct innodpu_pdp0_hw_device * hwdev);
	int (*cursor_is_disable)(struct innodpu_pdp0_hw_device * hwdev);

	void (*fd_plane_update)(inno_drm_plane *plane, inno_drm_plane_state *old_state);
	void (*fd_plane_disable)(inno_drm_plane *plane, inno_drm_plane_state *state);
	void (*fd_async_update)(inno_drm_plane *plane);
	void (*fd_async_enable)(inno_drm_plane *plane);
	void (*fd_async_disable)(inno_drm_plane *plane);
	void (*fd_pvric_disable)(struct innodpu_pdp0_hw_device *hwdev);
	void (*fd_decomp_pvric_init)(struct innodpu_pdp0_hw_device *hwdev, struct pdp0_crtc_config * priv_config,
		unsigned short format_id, inno_drm_framebuffer *fb, u32 width, u32 height, int plane_idx, u64 fb_addr);


	void (*enable_combine)(struct innodpu_pdp0_hw_device *hwdev, inno_videomode *mode);
	void (*start_combine)(struct innodpu_pdp0_hw_device *hwdev);
	void (*enable_dual_link)(struct innodpu_pdp0_hw_device *hwdev);
	void (*start_dual_link)(struct innodpu_pdp0_hw_device *hwdev);
	void (*disable_combine)(struct innodpu_pdp0_hw_device *hwdev);
	void (*disable_dual_link)(struct innodpu_pdp0_hw_device *hwdev);

	inno_drm_gem_object * (*get_gem)(void *inno_fb);
	struct innodpu_pdp0_hw_device * (*crtc_to_pdp0_hw)(inno_drm_crtc *crtc);
	void (*init_format_id)(void);

	void (*irq_handle)(struct innodpu_pdp0_hw_device *hwdev);
	void (*enable_irq)(struct innodpu_pdp0_hw_device *hwdev, u8 block, u32 irq);
	void (*disable_irq)(struct innodpu_pdp0_hw_device *hwdev, u8 block, u32 irq);

	void (*vga_point_enable)(struct innodpu_pdp0_hw_device *hwdev, u64 fb_addr,
			unsigned short width, unsigned short height, unsigned short x, unsigned short y);
	void (*vga_point_disable)(struct innodpu_pdp0_hw_device *hwdev);

	unsigned int bus_align;
	bool setqos;
	bool sethw_cursor;
	bool sethw_patch; // locations do not support x<0 and y<0, used ow2
	bool setvga;
	bool setvga_patch;
	u32 rotation_memory[2];

	// cursor
	bool cursor_enable;
	bool ow2_cursor_enable;
	unsigned long cursor_fb;
	u32 handle;
	int cursor_x;
	int cursor_y;
	int hot_x;
	int hot_y;
	int cursor_w;
	int cursor_h;
	int cursor_frame_w;
	int cursor_frame_h;
	unsigned int x_scaler;
	unsigned int y_scaler;

	bool combi;
	bool dual_link;
	bool is_nulldisp;
	bool is_normal_mode;

	inno_waitqueue_head *wq;
	atomic_t *config_valid;
	atomic_t *vblank_enable;

	inno_drm_crtc *crtc;
	inno_drm_device *drm_dev;
	struct pdp0_plane **plane;

	size_t *pframe_size;
	size_t *pframe_width;
	size_t *pframe_height;
	size_t *pframe_y_width_align;
	size_t *pframe_y_size_align;
	size_t *pframe_uv_width_align;
	size_t *pframe_uv_size_align;
	int *ppitch;
	u8 *pformat;
};

extern bool s_pdp0_debug;

static inline bool pdp0_hw_pitch_valid(struct innodpu_pdp0_hw_device *hwdev, unsigned int pitch)
{
	return !(pitch & (hwdev->bus_align - 1));
}

static inline unsigned char pdp0_hw_get_format_id(const struct pdp0_hw_regmap *map,
												  u8 layer_id, u32 format)
{
	unsigned int i;

	for (i = 0; i < map->n_pixel_formats; i++) {
		if (((map->pixel_formats[i].layer & layer_id) == layer_id) &&
			(map->pixel_formats[i].format == format))
			return map->pixel_formats[i].id;
	}

	return PDP0_INVALID_FORMAT_ID;
}

static inline int pdp0_plane_get_size(int start, unsigned length, unsigned last)
{
	int end = start + length;
	int size = 0;

	if (start <= 0) {
		if (end > 0)
			size = min_t(unsigned, end, last);
	} else if (start <= last) {
		size = min_t(unsigned, last - start, length);
	}

	return size;
}


#if 0
extern int pdp0_fd_irq_init(struct innodpu_pdp0_hw_device *hwdev);
extern void pdp0_fd_irq_fini(struct innodpu_pdp0_hw_device *hwdev);
extern int pdp0_fs_irq_init(struct innodpu_pdp0_hw_device *hwdev);
extern void pdp0_fs_irq_fini(struct innodpu_pdp0_hw_device *hwdev);
extern void pdp0_irq_handler(void *data);
#endif

extern struct innodpu_pdp0_hw_device pdp0_hw_device;

/* U16.16 */
#define FP_1_00000	0x00010000	/* 1.0 */
#define FP_0_66667	0x0000AAAA	/* 0.6667 = 1/1.5 */
#define FP_0_50000	0x00008000	/* 0.5 = 1/2 */
#define FP_0_36363	0x00005D17	/* 0.36363 = 1/2.75 */
#define FP_0_25000	0x00004000	/* 0.25 = 1/4 */

static inline enum pdp0_scaling_coeff_set pdp0_se_select_coeffs(u32 upscale_factor)
{
	return (upscale_factor >= FP_1_00000) ? PDP0_UPSCALING_COEFFS :
		(upscale_factor >= FP_0_66667) ? PDP0_DOWNSCALING_1_5_COEFFS :
		(upscale_factor >= FP_0_50000) ? PDP0_DOWNSCALING_2_COEFFS :
		(upscale_factor >= FP_0_36363) ? PDP0_DOWNSCALING_2_75_COEFFS : PDP0_DOWNSCALING_4_COEFFS;
}

#undef FP_0_25000
#undef FP_0_36363
#undef FP_0_50000
#undef FP_0_66667
#undef FP_1_00000


#define INNODP_BGND_COLOR_R		0x000
#define INNODP_BGND_COLOR_G		0x000
#define INNODP_BGND_COLOR_B		0x000
#define PDP0_FD_DEFAULT_PREFETCH_START		(0xb)

#define LAYER_ENABLE			(1UL << 31)
#define LAYER_FLOWCFG_MASK		7
#define LAYER_FLOWCFG(x)		(((x) & LAYER_FLOWCFG_MASK) << 28)
#define LAYER_FLOWCFG_SCALE_FS	3
#define LAYER_ROT_OFFSET		22
#define LAYER_H_FLIP			(1 << 21)
#define LAYER_V_FLIP			(1 << 20)
#define LAYER_ROT_MASK			(0xf << 20)
#define LAYER_COMP_MASK			(0x3 << 18)
#define LAYER_COMP_PIXEL		(0x3 << 18)
#define LAYER_COMP_LAYER		(0x2 << 18)
#define LAYER_COMP_PLANE		(0x2 << 18)

#define	LAYER_PMUL_ENABLE	(0x1 << 17)
#define	LAYER_ALPHA_OFFSET	(8)
#define	LAYER_ALPHA_COM_POS	(18)
#define	LAYER_ALPHA_MASK	(0xff)
#define	LAYER_ALPHA_VALUE(x)	(((x) & LAYER_ALPHA_MASK) << LAYER_ALPHA_OFFSET)
#define PDP0_LAYERS (1)

#define COMP_HEAD_SIZE			(0x100000)
#define TILE_SIZE				(0x100)
#define HEAD_ALIGN_SIZE			(0x100)
#define COMP_WIDTH_ALIGN		(0x80)
#define COMP_HEIGHT_ALIGN		(0x40)
#define COMP_TILE_FORMAT_2x32	(0x3)
#define COMP_TILE_FORMAT_4x16	(0x2)
#define TILE_Y_SIZE_2x32		(128)
#define TILE_UV_SIZE_2x32		(64)
#define TILE_Y_SIZE_4x16		(64)
#define TILE_UV_SIZE_4x16		(32)

#endif //__PDP0_HW_H
