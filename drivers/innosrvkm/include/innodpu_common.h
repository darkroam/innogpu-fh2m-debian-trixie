/*************************************************************************/ /*!
@File			innodpu_common.h
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
#ifndef __INNODPU_COMMON_H
#define __INNODPU_COMMON_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include "inno_drm_version.h"
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/component.h>
#include <linux/kref.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/console.h>

#include <video/display_timing.h>
#include <video/videomode.h>
#include <drm/drm_fourcc.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 1, 0))
#include <drm/drm_probe_helper.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
#include <drm/drm_drv.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
#include <drm/drm_device.h>
#include <drm/drm_gem_framebuffer_helper.h>
#endif

#include <drm/drm_modes.h>

#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_debugfs.h>
#endif

#include <drm/drm_blend.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
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

#if (DRM_VERSION >= KERNEL_VERSION(4, 19, 0))
#include <drm/drm_writeback.h>
#endif

#include "pvrversion.h"
#include "img_defs.h"
#include "hal.h"
#include "hal_interface.h"
#include "innogpu.h"

#include "innodpu_drm_fb.h"
#include "innodpu_dpu.h"
#include "inno_debug.h"
#include "innodpu_module_param.h"
#include "innodpu_compatibility.h"
#include "inno_waitqueue.h"
#include "innodma.h"
#include "inno_timer.h"

#define DRM_SYSFS_LINK

extern uint32_t fh2m_inno_efuse_read_word(uint32_t word_area);

#if (DRM_VERSION < KERNEL_VERSION(4, 20, 0))
#define DRM_MODE_BLEND_PREMULTI	0
#define DRM_MODE_BLEND_COVERAGE	1
#define DRM_MODE_BLEND_PIXEL_NONE 2
#endif

extern bool s_pdp0_debug;

//#define DISPLAY_TEST


#define DRIVER_NAME "innogpu"
#define DRIVER_DATE "20210625"
#define DRIVER_DESC "Innosilicon Technologies Display Driver"
#define DRIVER_AUTHOR "Innosilicon Technologies Ltd. <support@innosilicon.com.cn>"

#define INNODPU_WIDTH_MIN	(0)
#define INNODPU_HEIGHT_MIN	(0)
#define INNODPU_CURSOR_WIDTH	(64)
#define INNODPU_CURSOR_HEIGHT	(64)

#define INNO_EDID_BUF_LEN		(256)
#define INNODP_EDID_BUF_LEN		(INNO_EDID_BUF_LEN)
#define INNOHDMI_EDID_BUF_LEN	(512)
#define INNOVGA_EDID_BUF_LEN	(INNO_EDID_BUF_LEN)
#define HAL_MAX_DPU_NUMS		(16)
#define INNODPU_COMBINE_WIDTH 	(2048)
#define INNODPU_COMBINE_HEIGHT 	(2048)

#define DRM_PM_SMART_BACKUP (1)
//#define DRM_PM_GEM_VERIFY_MEMBACK (1)

// BY_TBD： 这个宏用来标志官方pdp0和inno pdp0的commit tail修改，后期还原时方便
#define INNODPU_FORMAT_MOD_TILE  (0x9200000000000017)

#define INNODPU_FORMAT_MOD_AB24  (0x9200000000000017)
#define INNODPU_FORMAT_MOD_NV12  (0x9200000000000016)
#if !defined(PVR_ANDROID_USE_PDP_LEGACY)
#define PDP_USE_ATOMIC	1
#endif

#define EDID_AUTO_READ   (0x0)
#define EDID_STR_PUSH    (0x1)
#define EDID_USER_DEFINE (0x2)
#define EDID_ZOOM_ENABLE (0x3)

#define DDC_EDID_ADDR    (0x50)
#define DDC_CI_ADDR      (0x37)
#define DDC_SEGMENT_ADDR (0x30)

#define INNO_HW_SELF_TEST_EDID   (0x0)

#define PM_VT_SWITCH_DISABLE
#define LOGO_SWITCH_POWERUP			(0x0)
#define LOGO_SWITCH_POWERDOWN		(0x1)
#define LOGO_SWITCH_HIBERNATION		(0x2)
#define LOGO_SWITCH_COMPLETE		(0x3)

#define HW_ALIGN

// #define TEST_DRM_VIRT // test zoom_enable, default not define and used custom_info
#ifdef TEST_DRM_VIRT
extern unsigned int drm_reso_virt;
#endif
extern unsigned int s_logo_extime;

enum innodpu_crtc_property {
	INNODPU_CRTC_PROP_WB_START,
	INNODPU_CRTC_PROP_WB_SAVE,
	INNODPU_CRTC_PROP_PVR_ENABLE,
	INNODPU_CRTC_PROP_PVR_TYPE,
	INNODPU_CRTC_PROP_BIST,
	INNODPU_CRTC_PROP_ISYUV,
	INNODPU_CRTC_PROP_DISPLAY_ID,
	INNODPU_CRTC_PROP_UVREVS,
	INNODPU_CRTC_PROP_COMP_TILE_4x16,
	INNODPU_CRTC_PROP_WB_PROP,
	INNODPU_CRTC_PROP_Y_ADDR,
	INNODPU_CRTC_PROP_UV_ADDR,
	INNODPU_CRTC_PROP_OW_WIDTH,
	INNODPU_CRTC_PROP_OW_HEIGHT,
	INNODPU_CRTC_PROP_SET_WM_FD,
	INNODPU_CRTC_PROP_DECOMP_TYPE,
	INNODPU_CRTC_PROP_DECOMP_ADDR,

	INNODPU_CRTC_PROP_MAX
};
enum innodpu_plane_property {
	INNODPU_PLANE_PROP_COLOR,
	INNODPU_PLANE_PROP_PIXEL_ALPHA,
	INNODPU_PLANE_PROP_LAYER_ALPHA,
	INNODPU_PLANE_PROP_PRE_ALPHA,
	INNODPU_PLANE_PROP_WH_SCALER,
	INNODPU_PLANE_PROP_ROTATE,
	INNODPU_PLANE_PROP_MAX
};

typedef enum innodpu_mem_class_e {
	CONTINUOUS_VRAM = 0,
	NO_CONTINUOUS_VRAM,
	GTT
} innodpu_mem_class;

typedef enum innodpu_mem_manage_mode_e {
	MEM_LIST_MODE = 0,
	MEM_DRM_MM_MODE
} innodpu_mem_manage_mode;

typedef enum innodpu_mem_positon_e {
	VRAM_POSITION = 0,
	SHMEM_VRAM_POSITION,
	SYS_GTT_POSITION,
} innodpu_mem_positon;

typedef struct innodpu_vram_block_info_t {
	uint64_t size;
	uint64_t dev_paddr;
	uint64_t cpu_paddr;
	bool is_visible;
} innodpu_vram_block_info;

typedef struct innodpu_pdp_vga_gem_t {
	innodpu_vram_block_info info;
	unsigned int max_width;
	unsigned int max_height;
	void *buffer;
} innodpu_pdp_vga_gem;

typedef struct innodpu_zero_gem_t {
	innodpu_vram_block_info info;
	void *backup_buffer;
	struct work_struct gemclear_work;
	struct drm_device *drm_dev;
	bool is_ready;
} innodpu_zero_gem;

typedef struct innodpu_mem_manager_t {
	struct drm_device *drm_dev;

	/* used for un shemem, postion as visible/invisible/gtt */
	struct mutex mem_lock;
	struct list_head mem_list;

	/* used for shmem, position must SHGEM */
	struct drm_mm gem_mm;

	/* used for s3/s4 gem_obj backup */
	struct mutex pm_lock;
	struct list_head pm_list;

	struct role_target *role;
	innodpu_zero_gem *zero_gem;

	innodpu_mem_positon pos;
	innodpu_mem_manage_mode manage_mode;
	bool visible;

	/* fake value */
	unsigned long size;

	/* mem alloc */
	int (*mem_alloc)(void *mem_manager, void *innodpu_obj, size_t size);
	void (*mem_free)(void *mem_manager,void *innodpu_obj);

	/* mem s3/s4 backup or recover */
	void (*mem_backup)(void *mem_manager, void *innodpu_obj);
	void (*mem_recover)(void *mem_manager, void *innodpu_obj);

	/* mem vm ops */
	void (*vm_open)(struct vm_area_struct *vma);
	void (*vm_close)(struct vm_area_struct *vma);
	int (*vm_fault)(struct vm_area_struct *vma, struct vm_fault *vmf);

} innodpu_mem_manager;

typedef struct innodpu_share_mem_user_t {
	unsigned long user_id;
	innodpu_mem_manager *mem_manager;
} innodpu_shared_mem_user;

typedef struct innodpu_shared_mem_t {
	uint64_t dev_paddr;
	uint64_t cpu_paddr;
	unsigned long size;
	bool is_visible;

	spinlock_t user_idr_lock;
	struct idr uer_idr;
	innodpu_shared_mem_user *current_user;
} innodpu_shared_mem;

struct drm_pdp_user_add {
	unsigned long user_id;
	unsigned long flags;
};

struct drm_pdp_user_set {
	unsigned long user_id;
	unsigned long flags;
};

struct drm_pdp_user_del {
	unsigned long user_id;
	unsigned long flags;
};

struct drm_pdp_user_info {
	unsigned long user_id;
	unsigned long flags;
};


#define MAX_PLANE	6
#define COLORS (10)

struct edid_data {
	char szHeader[9];
	char szVendor[3];
	char szProductCode[3];
	char szProductSeq[5];
	char szProductCycle[2];
	char szProductDate[2];
	char szEDIDVersion[3];
	char szDisplayPara[6];
	char szColorTemp[11];
	char szFixedTiming[4];
	char szStandardTiming[17];
	char szDetailTiming[73];
	char szExpandFlag[2];
	char szCheckSum[2];
	char szExtraData[385];
};

struct inno_mode_data {
	int hdisplay;
	int vdisplay;
	int vrefresh;
	bool reduced_blanking;
	bool interlaced;
	bool margins;
};

struct inno_res_region {
	struct resource *res;
	void __iomem *vaddr;
	unsigned long baddr;
};

struct inno_pvric_comp {
	int plane_idx;
	unsigned short format_id;
	unsigned int height;
	int y_base_offset;
	int uv_base_offset;
	int comp_total_size;
	int width_align;
};
typedef void (*set_rgbdata_id) (unsigned int *vaddr, int db9000_id, u32 plane_index,
								uint32_t size, void *priv, u32 color_idx);
typedef void (*set_yuvdata_id) (unsigned char *vaddr_byte, struct drm_plane_state * plane_state,
								u32 y_size, u32 cb_cr_size, u32 color_idx, uint32_t color_bars);

#if (DRM_VERSION >= KERNEL_VERSION(5, 4, 0))
#define	PVR_DRIVER_PRIME 0
#else
#define	PVR_DRIVER_PRIME DRIVER_PRIME
#endif

struct innodpu_module_pm_cfg {
	unsigned int len;
	unsigned int data[0];		// reg, val
};


#define PDP0_LAYERS (1)

struct innodpu_pdp0_drm {
	struct innodpu_pdp0_hw_device *hwdev;
	struct device *dev;			/* current device */
	chip_type_e plat;
	struct drm_device *drm_dev;
	struct drm_crtc crtc;
	struct pdp0_plane *plane[PDP0_LAYERS];

	struct drm_pending_vblank_event *event;

	wait_queue_head_t wq;
	atomic_t config_valid;
	atomic_t vblank_enable;

	spinlock_t mw_lock;
	u32 dpu_id;
	struct hrtimer timer_hr;

	/* for vga auto adapt */
	struct notifier_block vga_adapt_nb;
	atomic_t vga_nb_registered;
	bool lg_layer_enabled;

	/* this use for get display active count */
	struct hrtimer timer_active;
	inno_waitqueue_head *active_wq;
	atomic64_t active_count;
};

#define crtc_to_pdp0_device(x) container_of(x, struct innodpu_pdp0_drm, crtc)
#define hrtimer_to_pdp0_device(x) container_of(x, struct innodpu_pdp0_drm, timer_hr)
#define notifier_to_pdp0_device(x) container_of(x, struct innodpu_pdp0_drm, vga_adapt_nb)
#define active_timer_to_pdp0_device(x) container_of(x, struct innodpu_pdp0_drm, timer_active)

struct innodpu_drm_private {
	struct device *dev;
	struct drm_device *drm_dev;
	struct dev_rsrc *pdev_rsrc;

	/* TBD: connector interface */
	struct hdmi_device *hdmi[HAL_MAX_HDMI_NUMS];
	struct dp_device *dp;
	struct vga_device *vga;
#if 0
	void __iomem *pmbus_ctrl_base[PMBUS_COUNT];
#endif
	bool display_enabled;
	struct mutex init_lock;

	/* TBD: db9000 interface */

	struct inno_res_region reg_space[HAL_MAX_DPU_NUMS];
	struct inno_res_region wb_space[HAL_MAX_DPU_NUMS];
	struct dma_buf *wb_dbuf[HAL_MAX_DPU_NUMS];
	struct inno_res_region def_wb_space[HAL_MAX_DPU_NUMS];
	size_t frame_size[HAL_MAX_DPU_NUMS];
	size_t frame_width[HAL_MAX_DPU_NUMS];
	size_t frame_height[HAL_MAX_DPU_NUMS];
	size_t frame_y_width_align[HAL_MAX_DPU_NUMS];
	size_t frame_y_size_align[HAL_MAX_DPU_NUMS];
	size_t frame_uv_width_align[HAL_MAX_DPU_NUMS];
	size_t frame_uv_size_align[HAL_MAX_DPU_NUMS];
	struct inno_pvric_comp inno_comp[HAL_MAX_DPU_NUMS];

	const char *dbname[HAL_MAX_DPU_NUMS];
	set_rgbdata_id db9000_data_rgbcb[COLORS];
	set_yuvdata_id db9000_data_yuvcb[COLORS];
	struct drm_crtc *crtc[HAL_MAX_DPU_NUMS];
	struct drm_plane *planes[HAL_MAX_DPU_NUMS][MAX_PLANE];
	uint32_t resolutions[HAL_MAX_DPU_NUMS][2];

	struct proc_dir_entry *db9000_proc_nodes[HAL_MAX_DPU_NUMS];
	uint32_t pre_scale_w[10], pre_scale_h[10];	// OVERLAY_MAX
	uint32_t post_scale_w[10], post_scale_h[10];	// OVERLAY_MAX

	bool new_frame[HAL_MAX_DPU_NUMS];
	bool db9000_en[HAL_MAX_DPU_NUMS];

	bool scaler;
	int wb_len;
	unsigned long frame_cnt[HAL_MAX_DPU_NUMS];	// 回写帧计数
	unsigned long frame_num[HAL_MAX_DPU_NUMS];	//
	int pitch[HAL_MAX_DPU_NUMS];
	u8 format[HAL_MAX_DPU_NUMS];

	void __iomem *sys_ctrl_base;	// sys_reg_base

	/* proc_fs : /proc/innodrm/dpu0~HAL_MAX_DPU_CHANS/ */
	struct proc_dir_entry *innodpu_proc_rootdir;
	struct proc_dir_entry *innodpu_proc_dpudir[HAL_MAX_DPU_NUMS];

	struct role_target role;
	innodpu_zero_gem *zero_gem;
	innodpu_pdp_vga_gem *pdp_vga_gem;

	innodpu_mem_manager *visible_mem_manager;
	innodpu_mem_manager *invisible_mem_manager;
	innodpu_mem_manager *gtt_mem_manager;
	innodpu_shared_mem shared_vram_info;
	bool has_inv_mem;
	bool has_shared_mem;
	bool has_gtt_mem;
	bool has_uncontinuous_mem;
	bool has_pdp_vga_mem;

	/* drm device */
	void *innodpu_drm[HAL_MAX_DPU_NUMS];

#if defined(CONFIG_DRM_FBDEV_EMULATION)
	struct inno_fbdev *fbdev;
#endif
#if (DRM_VERSION >= KERNEL_VERSION(4, 19, 0))
	struct drm_writeback_connector connector[HAL_MAX_DPU_NUMS];
#endif

	struct drm_property *plane_prop[INNODPU_PLANE_PROP_MAX];
	struct drm_property *crtc_prop[INNODPU_CRTC_PROP_MAX];
	/* connector property */

	struct drm_atomic_state *pm_state;

	/* ACPI kenrel notification chain status information */
	unsigned long pm_event;
	/* ACPI kernel notification chain */
	struct notifier_block pm_nb;

	bool drm_nulldisplay;
	bool irq_enabled;
	int role_id;

	inno_ktime logo_end_ktime;
	struct mutex logo_lock;
	int logo_execute_status;
#ifdef DRM_SYSFS_LINK
	bool sysfs_patch;
#endif

	struct disp_interface_info *interface_info;
};
#define to_inno_drm(_drm_dev, _dpuid)  \
	_drm_dev->dev_private ? \
	((struct innodpu_drm_private *)_drm_dev->dev_private)->innodpu_drm[_dpuid] : NULL;

static __always_inline int innodpu_get_dpuid_bycrtc(struct drm_crtc *crtc)
{
	unsigned int dpu_id = 0;

	if (!crtc)
		return -EFAULT;

	dpu_id = simple_strtoul(crtc->name + 3, NULL, 0);
	if (dpu_id > HAL_MAX_DPU_NUMS) {
		fh2m_innodpu_err(crtc->dev->dev, "Invaild dpu_id %s-%d\n", crtc->name, dpu_id);
		return -EINVAL;
	}
	return dpu_id;
}

// BY_TBD：目前plane只支持配置所属的crtc，不能交换使用，如果支持后，这里禁止使用
// 官方做法要求 plane所属crtc应该从state获取，而不是plane获取
// BY_TBD:
static __always_inline int innodpu_get_dpuid_byplane(struct drm_plane *plane)
{
	int dpu_id;
	int plane_idx = 0;

	if (strncmp(plane->name, "db9", 3) == 0) {
		sscanf(plane->name, "db9000:%d-plane:%d", &dpu_id, &plane_idx);
	} else {
		sscanf(plane->name, "dpu_id:%d-ow:%d", &dpu_id, &plane_idx);
	}

	return dpu_id;
}

static inline u32 inno_drm_fb_format(struct drm_framebuffer *fb)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
	return fb->format->format;
#else
	return fb->pixel_format;
#endif
}


/*
 * innodpu_dma_memcpy3: dma in (visible)vram -- host_memory exchange
 * enum dma_xfer_direction dir: GDDR2SYS, SYS2GDDR
 */
static inline int innodpu_dma_memcpy3(struct device *dev, void* src[],
		void* dst[], int len[], int cnt, enum dma_xfer_direction dir)
{
#ifdef SUPPORT_DMA_TRANSFER
	if (!module_is_live(dev->driver->owner)) {
		return -EBUSY;
	}
	return innodma_memcpy3(dev, src, dst, len, cnt, dir);
#else
	// fh2m_inno_dev_printk(KERN_ERR, dev, "innodpu_dma_memcpy3 not support gddr2gddr\n");
	return 0;
#endif
}

/*
 * innodpu_dma_memcpy: dma in vram -- vram exchange
 * enum dma_xfer_direction dir: GDDR2GDDR
 */
static inline int innodpu_dma_memcpy(struct device *dev, void* src[],
		void* dst[], int len[], int cnt, enum dma_xfer_direction dir)
{
#ifdef SUPPORT_DMA_TRANSFER
	if (!module_is_live(dev->driver->owner)) {
		return -EBUSY;
	}
	return fh2m_innodma_memcpy(dev, src, dst, len, cnt, dir);
#else
	// fh2m_inno_dev_printk(KERN_ERR, dev, "innodpu_dma_memcpy not support\n");
	return 0;
#endif
}

/*
 * innodpu_dma_memcpy_for_smallbar_sg: dma in (visible) vram -- host_memory exchange for kvmalloc
 * enum dma_xfer_direction dir: SYS2GDDR， GDDR2SYS for kvmalloc
 */
static inline int innodpu_dma_memcpy_for_smallbar_sg(struct device *dev, void *srcs[],
		void *dsts[], int lens[], int cnt, enum dma_xfer_direction dir)
{
#ifdef SUPPORT_DMA_TRANSFER
	if (!module_is_live(dev->driver->owner)) {
		return -EBUSY;
	}
	return fh2m_innodma_memcpy_for_smallbar_sg(dev, srcs, dsts, lens, cnt, dir);
#else
	// fh2m_inno_dev_printk(KERN_ERR, dev, "innodpu_dma_memcpy_for_smallbar_sg not support\n");
	return 0;
#endif
}

/*
 * innodpu_dma_memcpy_for_smallbar: dma in (visible) vram -- host_memory exchange for kzmalloc
 * enum dma_xfer_direction dir: SYS2GDDR， GDDR2SYS for kzmalloc
 */
static inline int innodpu_dma_memcpy_for_smallbar(struct device *dev, void *srcs[],
		void *dsts[], int lens[], int cnt, enum dma_xfer_direction dir)
{
#ifdef SUPPORT_DMA_TRANSFER
	if (!module_is_live(dev->driver->owner)) {
		return -EBUSY;
	}
	return innodma_memcpy_for_smallbar(dev, srcs, dsts, lens, cnt, dir);
#else
	// fh2m_inno_dev_printk(KERN_ERR, dev, "innodpu_dma_memcpy_for_smallbar not support\n");
	return 0;
#endif
}
#endif //__INNODPU_COMMON_H
