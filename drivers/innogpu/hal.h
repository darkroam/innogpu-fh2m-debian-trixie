/*************************************************************************/ /*!
@File           hal.h
@Title
@Copyright      Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License        Dual MIT/GPLv2

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

#ifndef __HAL_H__
#define __HAL_H__
#include <linux/errno.h>
#include "hal_interface.h"

#include "inno_interrupt.h"
#include "inno_dma.h"
#include "inno_lock.h"
#include "inno_pci.h"
#include "inno_task.h"
#include "inno_plat_dev.h"

#if defined(SUPPORT_ION)
#include "innogpu_ion.h"
#endif

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#define HAL_MAX_PMBUS_NUMS		(5)
#define HAL_MAX_VKMS_NUMS		(8)
#define HAL_MAX_HDMI_NUMS		(2)
#define HAL_MAX_VGA_NUMS		(1)
#define HAL_MAX_DP_NUMS			(1)
#define HAL_MAX_LVDS_NUMS		(1)
#define HAL_MAX_DPU_CHANS		(8)
#define HAL_MAX_DB9000_CHANS 	(2)
#define HAL_MAX_DRM_NUMS		(8)
#define HAL_MAX_AUDIO_NUMS		(1)
#define HAL_MAX_POWER_NUMS		(1)

#define HAL_MAX_DDR_CHIP	(4)
#define HAL_MAX_DDR_CHANNEL	(2)
#if defined(SUPPORT_PCIE_SRIOV)&&defined(SRIOV_VF_MODE)
#define HAL_MAX_VPU_CHANS      (8)
#else
#define HAL_MAX_VPU_CHANS	(16)
#endif
#define HAL_MAX_APU_CHANS	(2)
#define HAL_MAX_GPU_CORES	(4)
#define HAL_MAX_DMA_CHANS   (5)
#define HAL_MAX_DMA_NUMS	(5)
#define HAL_MAX_VRAM_UNITS	(4)
#define HAL_MAX_VPU_NUM		(6)
#define HAL_MAX_SR_NUM		(1)

#define HAL_DPU_PER_UNITS	(1)
#define HAL_VPU_PER_UNITS	(4)
#define HAL_GPU_PER_UNITS	(1)
#define HAL_DMA_PER_UNITS	(1)

#define HAL_MAX_VF_NUMS		(16)

#define HAL_MAX_ARRAY_NUM_IN_ZONE	(4)

#define HAL_ZONE_MC1		(4)
#define HAL_ZONE_MC2		(2)
#define HAL_ZONE_MC4		(1)

#define HAL_DPU_NUMS_IN_MC4	(2)
#define HAL_VPU_NUMS_IN_MC4	(2)

#define HAL_GPU_CORES_IN_MC4	(1)

#define HAL_DMA_ZONE_IDX	(0)

#define HAL_MAX_ALLOC_NUM	(100)

#define IRQ_TIMEOUT 	(5000)

// TODO
#define IRQ_NUMS		(121)

#define IRQ_REG_NUMS	(8)

#define IRQ_REG_IDX00	(0)
#define IRQ_REG_IDX01	(1)
#define IRQ_REG_IDX02	(2)
#define IRQ_REG_IDX03	(3)
#define IRQ_REG_IDX04	(4)
#define IRQ_REG_IDX05	(5)
#define IRQ_REG_IDX06	(6)
#define IRQ_REG_IDX07   (7)
/*
	Register REVISION
*/
#define HW_CORE_REVISION                       0x0004
#define HW_REVISION_MINOR_MASK                 0x0000FFFFU
#define HW_REVISION_MINOR_SHIFT                0
#define HW_REVISION_MINOR_SIGNED               0

#define HW_REVISION_MAJOR_MASK                 0xFFFF0000U
#define HW_REVISION_MAJOR_SHIFT                16
#define HW_REVISION_MAJOR_SIGNED               0
/*
	Register CHANGE_SET
*/
#define HW_CORE_CHANGE_SET                     0x0008
#define HW_CHANGE_SET_SET_MASK                 0xFFFFFFFFU
#define HW_CHANGE_SET_SET_SHIFT                0
#define HW_CHANGE_SET_SET_SIGNED               0
/*
	Register USER_ID
*/
#define HW_CORE_USER_ID                        0x000C
#define HW_USER_ID_ID_MASK                     0x0000000FU
#define HW_USER_ID_ID_SHIFT                    0
#define HW_USER_ID_ID_SIGNED                   0
/*
	Register USER_BUILD
*/
#define HW_CORE_USER_BUILD                     0x0010
#define HW_USER_BUILD_BUILD_MASK               0xFFFFFFFFU
#define HW_USER_BUILD_BUILD_SHIFT              0
#define HW_USER_BUILD_BUILD_SIGNED             0
#define HEX2DEC(v)                 ((((v) >> 4) * 10) + ((v) & 0x0F))
/*
	MCUFW status
*/
#define DBGINFO_MOD_VERSION                    (0xD031E0)
#define MCUFW_DDR_INIT_FAIL_COUNT              (0xD031FA)
#define MCUFW_DDR_RETRAIN_COUNT                (0xD031F8)
#define MCUFW_DDR_RETRAIN_COUNT_OFFSET         (MCUFW_DDR_RETRAIN_COUNT - DBGINFO_MOD_VERSION)
#define MCUFW_MEM_CHIP                         (0xD031C0)

/*
	g0 soc hwinfo
*/
#define HWINFO_KEY_LENGTH				(29)
#define HWINFO_KEY_ENTRY_CNT_LEN		(2)
#define HWINFO_KEY_CRC_CNT_LEN			(2)
#define HWINFO_KEY_DATA_OFFSET_LEN		(2)
#define HWINFO_STR_FLAG					(1U)
#define HWINFO_NUM_FLAG					(0U)
#define HWINFO_TWO_BYTE_FLAG			(2u)

#define HWINFO_LEN						(0x1020)		/* key allocated space */
#define HWINFO_MACIC_NUM_LEN			(4)
#define HWINFO_REVD_LEN					(24)
#define HWINFO_HW_PCB_ID_LEN			(30)
#define HWINFO_HW_PRODUCT_ID_LEN		(30)
#define HWINFO_HW_SN_LEN				(20)
#define HWINFO_HW_KEY_HEAD_LEN			(32U)
#define HWINFO_VERSION_ID_LEN			(2)
#define HWINFO_HW_OLD_VER				((char)1)
#define HWINFO_HW_NEW_VER				((char)0)
#define HWINFO_HW_DP2VGA_CHIP_LEN		(30)

#define HWINFO_CUSTOM_KEY_ADDR_OFFSET	(0x0UL)
#define HWINFO_CUSTOM_VAL_ADDR_OFFSET	(0x1000UL)
#define HWINFO_EXTRA_ADDR_OFFSET		(0x2000UL)
#define HWINFO_KEY_HEADER_ADDR_OFFSET  (0x3000UL)
#define HWINFO_VAL_ADDR_OFFSET  		(0x4000UL)
#define HWINFO_SN_ID_ADDR_OFFSET 		(0x20U)
#define HWINFO_SN_ID_LEN 				(16)

#define HWINFO_KEY_LEN_MAX				(0x1000)
#define HWINFO_INIT_FINISHED			((char)(1))
#define HWINFO_INIT_UNFINISHED			((char)(0))
#define INNO_POWERCHIP_NAME_SIZE		(15)
#define EYE_DIAGRAM_LEN                 (3)
#define EDID_DATA_LEN                   (128)

/**
 * DMA config
*/
#define HAL_MAX_DMA_CHAN_CNT            (16)    /* max channel count alloc for gpu */

enum INTERFACE_TYPE {
	INNO_DRM_MODE_CONNECTOR_Unknown = 0,
	INNO_DRM_MODE_CONNECTOR_VGA,
	INNO_DRM_MODE_CONNECTOR_DVII,
	INNO_DRM_MODE_CONNECTOR_DVID,
	INNO_DRM_MODE_CONNECTOR_DVIA,
	INNO_DRM_MODE_CONNECTOR_Composite,
	INNO_DRM_MODE_CONNECTOR_SVIDEO,
	INNO_DRM_MODE_CONNECTOR_LVDS,
	INNO_DRM_MODE_CONNECTOR_Component,
	INNO_DRM_MODE_CONNECTOR_9PinDIN,
	INNO_DRM_MODE_CONNECTOR_DisplayPort,
	INNO_DRM_MODE_CONNECTOR_HDMIA,
	INNO_DRM_MODE_CONNECTOR_HDMIB,
	INNO_DRM_MODE_CONNECTOR_TV,
	INNO_DRM_MODE_CONNECTOR_eDP,
	INNO_DRM_MODE_CONNECTOR_VIRTUAL,
	INNO_DRM_MODE_CONNECTOR_DSI,
	INNO_DRM_MODE_CONNECTOR_DPI,
	INNO_DRM_MODE_CONNECTOR_WRITEBACK,
};

enum HAL_DMA_TYPE {
	HAL_DMA_TYPE_AXI_DMA_LEFT = 0,
	HAL_DMA_TYPE_PCIE_DMA,
	HAL_DMA_TYPE_AXI_DMA_RIGHT,
	HAL_DMA_TYPE_MAX
};

enum G0_SOC_DP2VGA_CHIP_TYPE {
	DP2VGA_CH7517_CHIP = 1,
	DP2VGA_CM3166E_CHIP,
	DP2VGA_CS5212_CHIP,
	DP2VGA_RT2166_CHIP,
	DP2VGA_MAX_CHIP
};

enum HW_VOLCTRL_CHIP_TYPE{
	VOLCTRL_MP2979A,
	VOLCTRL_XDPE12284C,
	VOLCTRL_TDA38740,
	VOLCTRL_IR38064,
	VOLCTRL_UP1542SSU8,
	VOLCTRL_MAX
};

enum gpu_work_mode {
	HAL_GPU_WORK_MODE_MC1=1,
	HAL_GPU_WORK_MODE_MC2=2,
	HAL_GPU_WORK_MODE_MC3=3,
	HAL_GPU_WORK_MODE_MC4=4
};

enum system_work_mode {
	HAL_SYSTEM_WORK_MODE_HOST=0,
	HAL_SYSTEM_WORK_MODE_PF,
	HAL_SYSTEM_WORK_MODE_VF,
	HAL_SYSTEM_WORK_MODE_DOCK,
	HAL_SYSTEM_WORK_MODE_MAX,
};

enum HAL_THERMAL_TYPE {
	HAL_THML_TYPE_COMMERCIAL, /*C类*/
	HAL_THML_TYPE_INDUSTRIAL, /*I类*/
	HAL_THML_TYPE_MILITARY,   /*J类 || M类*/
	HAL_THML_TYPE_DEBUG,      /*D类*/
	HAL_THML_TYPE_MAX,
};

struct system_cfg_param
{
	enum system_work_mode mode;
};

struct gpu_cfg_param
{
	enum gpu_work_mode mode;
	int gpu_core_num;
};

/*标识一块内存区域，包含起始地址及大小*/
struct vram_region {
	uint64 base;
	uint64 size;
};

/*组件内存描述，包含可见区域、不可见区域、共享区域*/
struct component_vram {
	struct vram_region visible_region;
	struct vram_region invisible_region;
	struct vram_region shared_region;
};

struct vram_cb {
	int (*alloc)(void *private, uint32 role ,uint32 component_id,uint64 *dev_base,uint64 size, uint64_t flags);
	int (*free)(void *private, uint32 role ,uint32 component_id,uint64 base);
	int (*varm_dev_to_host)(void *private,uint64 dev_base,uint64 *host_base);
	int (*varm_host_to_dev)(void *private,uint64 host_base,uint64 *dev_base);
	int (*vram_stats)(void *private, uint32 role, uint32 component_id, struct vram_stats *stats);
	void *private;
};

struct gtt_cb {
	struct gtt_item* (*alloc)(void *private, uint64 size, uint64 flags);
	void (*free)(void *private, struct gtt_item *item);
	void *private;
};

struct pmr_cb {
	struct phys_mem_resource* (*alloc)(void *private, uint32_t role, uint32_t comp_id, uint64_t size, bool cpu_access, uint64_t flags);
	void (*free)(void *private, uint32_t role, uint32_t comp_id, bool cpu_access, struct phys_mem_resource *pmr);
	void *private;
};

/* 设备节点对应的内存区域*/
struct vram_node {
	struct component_vram dpu_vram_zone[HAL_MAX_DPU_CHANS]; /* 绑定关系决定最大访问数目*/
	struct component_vram vpu_vram_zone[HAL_MAX_ARRAY_NUM_IN_ZONE];
	struct component_vram apu_vram_zone;
	struct component_vram gpu_vram_zone;
	struct component_vram dma_vram_zone;
	int dpu_nums;
	int dpu_bind_id[HAL_MAX_DPU_CHANS];
	int vpu_nums;
	int vpu_bind_id[HAL_MAX_VPU_CHANS];
	bool is_share_vpu_to_dpu;			/* 可见显存，当VPU的heap需要限制在4G范围时，使VPU和DPU共用一个不超过4G的heap */
	bool is_inv_share_vpu_to_dpu;		/* 不可见显存，当VPU的heap需要限制在4G范围时，使VPU和DPU共用一个不超过4G的heap */
	struct vram_cb *pvisible_vram_cb; 	/* GPU接口回调函数 */
	struct vram_cb *invisible_vram_cb;
	struct gtt_cb  *gtt_cb;
	struct pmr_cb  *pmr_cb;
};


#define V_FREE 0 //空闲状态
#define V_BUSY 1 //已用状态
#define V_OK 1    //完成
#define V_ERROR 0 //出错
#define MAX_LINK_LIST_LEN	(10000)

typedef struct freearea//定义一个空闲区说明表结构
{
    uint64 size;   //分区大小
    uint64 address; //分区地址
    int state;   //状态
}v_area;

// 线性表的双向链表存储结构
typedef struct dulnode
{
    v_area data;
    struct dulnode *prior; //前趋指针
    struct dulnode *next;  //后继指针
}dul_node,*du_link_List;

/* 一块内存区域包含的组件*/
struct vram_zone {
	struct vram_node dev_node_vram;
	int vram_unit_nums_in_zone;
	int vram_unit_num[HAL_MAX_VRAM_UNITS];
	struct vram_region dpu_region_array[HAL_MAX_ALLOC_NUM]; /*内存分配管理数组*/
	struct vram_region vpu_region_array[HAL_MAX_ALLOC_NUM];
	struct vram_region gpu_region_array[HAL_MAX_ALLOC_NUM];
	struct vram_region dma_region_array[HAL_MAX_ALLOC_NUM];
	int dpu_refer_cnt;
	int vpu_refer_cnt;
	int gpu_refer_cnt;
	int dma_refer_cnt;
	dul_node dpu_blk_first;
	dul_node dpu_blk_last;
	dul_node vpu_blk_first;
	dul_node vpu_blk_last;
	dul_node apu_blk_first;
	dul_node apu_blk_last;
	dul_node gpu_blk_first;
	dul_node gpu_blk_last;
	dul_node dma_blk_first;
	dul_node dma_blk_last;
	inno_spinlock *dpu_lock;
	inno_spinlock *vpu_lock;
	inno_spinlock *apu_lock;
	inno_spinlock *gpu_lock;
	inno_spinlock *dma_lock;
};

/*内存配置项，确定内存划分，从组件和内存区域两个角度来描述内存*/
struct vram_cfg {
	struct vram_zone zones[HAL_MAX_VF_NUMS];  /* 内存按区域划分*/
	struct vram_zone zone_mc4;
	enum gpu_work_mode gpu_mode;
    int gpu_num;
	uint8_t zone_nums;
	uint64 vram_unit_size;
	int vram_unit_nums;
	uint64 vram_dev_base;
	uint64 vram_host_base;
	uint64 vram_pcie_base;
	inno_dev *dev;
	uint64 reserved_size;
	uint64 dma_total_size;
	uint64 dpu_total_size;
	uint64 vpu_total_size;
	uint64 apu_total_size;
	uint64 gpu_total_size;
	uint64 dpu_inv_total_size;
	uint64 vpu_inv_total_size;
	uint64 apu_inv_total_size;
	uint64 gpu_inv_total_size;
};

struct irq_handle {
	bool enabled;					// 对应irq是否使能回调函数
	void (*handler_func)(void *);	// irq 回调函数
	void *handler_data;				// 回调函数需要的数据
};

struct hal_irq_maskbit{
	uint32_t irq_reg_idx;
	uint32_t hal_mask_reg;
	uint32_t hal_bit_in_mask;
	uint32_t hal_bit_in_stat;
};

typedef struct rsrc_nums {
	uint8_t hdmi_nums;
	uint8_t dp_nums;
	uint8_t vga_nums;
	uint8_t dpu_nums;
	uint8_t db9000_nums;
	uint8_t pdp0_nums;
	uint8_t vkms_nums;
	uint8_t lvds_nums;
	uint8_t vpu_nums;
	uint8_t audio_nums;
	uint8_t dma_nums;
	uint8_t pmbus_nums;
	uint8_t gpu_nums;
	uint8_t apu_nums;
	uint8_t drm_nums;
	uint8_t sr_nums;
}rsrc_nums_t;

/* 格式参照 hwinfo_define_v3_20220928.xlsx  */
typedef struct hwinfo_key_flag_t{
	char key[HWINFO_KEY_LENGTH + 1];
	int flag;	//0:字符串 1:
}HWINFO_KEY_FLAG_T;
typedef struct hwinfo_key_head_t{
	char magic_num[HWINFO_MACIC_NUM_LEN];
	char key_length[HWINFO_KEY_ENTRY_CNT_LEN];
	char crc16[HWINFO_KEY_CRC_CNT_LEN];
	char revd[HWINFO_REVD_LEN];
}HWINFO_KEY_HEAD_T;

typedef struct hwinfo_key_t{
	unsigned char lens;
	char data_offset[HWINFO_KEY_DATA_OFFSET_LEN];
	char key[HWINFO_KEY_LENGTH];
}HWINFO_KEY_T;

struct hw_info_item {
	char *name;
	char val_type;
	unsigned int val_lens;
};
struct custom_display_t{
	char hdmi0_type;       /* 0:disable 1:hdmi 2:dvi */
	char hdmi1_type;       /* 0:disable 1:hdmi 2:dvi */
	char dp_type;          /* 0:disable 1:dp   2:vga */
	char lvds_type;        /* 0:disable 1:lvds       */
	char main_screen;      /* 1,2,3,4设置主屏 暂不支持 */
	char enable_4k;        /* 0:不支持4K 1:HDMI0 2:HDMI1 3:DP */
	char hdmi0_edid_mode;  /* 0:自动模式 1:强推模式 2:自定义EDID  */
	char hdmi1_edid_mode;
	char dp_edid_mode;
	char lvds_edid_mode;
	char zoom_mode;        /* 设置缩放 目前缺少参数     */
	char dual_link;        /* dual link 使能设置 */
	char hdmi0_eye_diagram[EYE_DIAGRAM_LEN];  /* 眼图功能配置 */
	char hdmi1_eye_diagram[EYE_DIAGRAM_LEN];
	char dp_eye_diagram[EYE_DIAGRAM_LEN];
	char hdmi0_edid1[EDID_DATA_LEN];   /*  edid数据 */
	char hdmi0_edid2[EDID_DATA_LEN];
	char hdmi1_edid1[EDID_DATA_LEN];
	char hdmi1_edid2[EDID_DATA_LEN];
	char dp_edid1[EDID_DATA_LEN];
	char dp_edid2[EDID_DATA_LEN];
	char lvds_edid1[EDID_DATA_LEN];
	char lvds_edid2[EDID_DATA_LEN];
};

#define ANALOG_VOLT_DEFAULT_OUTPUT 850

/* 电源管理 客户定制等字段 */
#define GPU_DEFAULT_WORK_FREQ            1000
#define GPU_FREQ_M2HZ_UNIT               (1000000UL)
#define GPU_VOL_UV2MV_UNIT               1000
#define INNO_POWER_WORKMODE_SUPPORT_MASK (0xf << 4)
#define INNO_POWER_DEFAULT_WORKMODE_MASK (0xf << 0)
#define DEFAULT_WORKMODE_NORMAL          (0x0)          /*正常模式*/
#define DEFAULT_WORKMODE_POWSAVE         (0x1)          /*低功耗模式*/
#define DEFAULT_WORKMODE_PERF            (0x2)          /*性能模式*/
#define DEFAULT_WORKMODE_DYN             (0x3)          /*动态调频*/

#define INNO_GPU_PRE_CLK_CHANGE		1
#define INNO_GPU_POST_CLK_CHANGE	0

#define DYNFREQ_ALGO_MIX 1			/*f(board_temp, chip_temp) ===> out:gpufreq, dbusfreq*/
#define DYNFREQ_ALGO_PID 2			/*f(chip_temp)             ===> out:gpufreq*/

/*update voltage scene*/
#define PWR_NOTIFY_FOR_S3_EXIT 1
#define PWR_NOTIFY_FOR_S4_EXIT 2
#define PWR_NOTIFY_FOR_NORMAL 3
#define PWR_NOTIFY_FOR_POWEROFF 4
#define PWR_NOTIFY_FOR_SHUTDOWN 5
#define PWR_NOTIFY_FOR_S3_ENTER 6
#define PWR_NOTIFY_FOR_S4_ENTER 7

#define PCIE_SPEED_GEN1 1
#define PCIE_SPEED_GEN2 2
#define PCIE_SPEED_MAX 0

typedef enum pcie_speed_cap
{
	PCIE_SPEED_CAP_GEN1 = 1,
	PCIE_SPEED_CAP_GEN2 = 2,
	PCIE_SPEED_CAP_GEN3 = 3,
	PCIE_SPEED_CAP_GEN4 = 4,
	PCIE_SPEED_CAP_GEN5 = 5,
	PCIE_SPEED_CAP_GEN6 = 6,
	PCIE_SPEED_CAP_MAX,
}pcie_speed_cap_type;

/*power debug level*/
#define PWRD_DEBUG_MODE (0x1 << 30)

#define PWRD_LVL_ERROR 0
#define PWRD_LVL_NOTICE 1
#define PWRD_LVL_WARN 2
#define PWRD_LVL_DBG 3
#define PWRD_LVL_INFO 4
#define PWRD_DBG_INPUT    (0xfd)
#define PWRD_DBG_IDLE_VOL (0xfe)
#define PWRD_DBG_REAL_VOL (0xff)

#define MCUFW_COMM_PROTOCOL_V1 1
#define MCUFW_COMM_PROTOCOL_V2 2

/* add ec backup field for notebook bus conflict between ec i2c and apb bus internally*/
#define MCUFW_COMM_PROTOCOL_V3 3

/* add debuginfo, no need to senf comm,directly query memory*/
#define MCUFW_COMM_PROTOCOL_V4 4

/* add memchip, no need to senf comm,directly query memory*/
#define MCUFW_COMM_PROTOCOL_V5 5

/* the version of vol cmd format */

/* V0: not support the communication protocol between kmd and mcufw
 *   : access voltage chip by kmd drv
 */
#define MCUFW_COMM_CMD_VOL_V0_LEGACY 0

/* V1: support update voltage by mcufw and mcufw has the following capabilities:
 *   : with sys_stat as command parameters
 */
#define MCUFW_COMM_CMD_VOL_V1 1

/* V2： Add additional commands to notify sys_stat */
#define MCUFW_COMM_CMD_VOL_V2 2

/* V3： Add GET_VOL_CAPACITY command */
#define MCUFW_COMM_CMD_VOL_V3 3

typedef struct power_custom_hwinfo_t{
	uint16 normal_mode_freq;         /* 正常工作模式下的工作频率 */
	uint16 powersave_mode_freq;      /* 低功耗模式下工作频率         */
	uint16 performance_mode_freq;    /* 性能工作模式下工作频率       */
	uint16 dyn_mode_mode_freq;       /* 动态工作模式下的初始工作频率 */
	uint16 dyn_lpc_dbus_freq;

	uint16 user_powsave_freq;
	uint16 user_mode_powsave;

	uint16 powersave_vdd_core_vol;
	uint16 powersave_databus_freq;
	uint16 powersave_ddr_speed;

	uint16 performence_mode_volt;

	uint16 dyn_freq_max;             /* 温度作为输入条件的最大可设置频率  */
	uint16 dyn_freq_min;             /* 温度作为输入条件的最小可设置频率  */

	unsigned char   pwr_ctrl_mode;            /* 电源管理模式                 */
	unsigned char   dyn_temp_start_trip;      /* 动态调频起始出发温度         */
	unsigned char   dyn_temp_step;            /* 温度增长补偿 */
	unsigned char   dyn_freq_step;
	unsigned char   dyn_update_vol_enable;
	unsigned char	dyn_lpc_version;
	unsigned char	dyn_lpc_gpu_utils;
	unsigned char   powersave_pcie_speed;

	unsigned char   over_temp_enable;
	unsigned char   over_temp_chip;
	unsigned char   over_temp_board;
	unsigned char   limit_freq_enable;
	unsigned char   limit_temp_chip;
	unsigned char   limit_temp_board;
	uint16          limit_freq_thd;
}POWER_CUSTOM_HWINFO_T;

#define PIDCTL_OUT_FREQ_NUM 3
/*temperature pid algo params*/
struct tpid_chip_parms {
	int P;              /*pid algo param: proportion factor*/
	int I;              /*pid algo param: integral factor*/
	int D;              /*pid algo param: differential factor*/
	int ref_temp;       /*pid algo param: target and reference value*/

	uint32_t outfreq[PIDCTL_OUT_FREQ_NUM]; /*max, middle, min gpufreq*/
	uint32_t real_target_freq;
};

/*temperature mix algo params*/
struct tmix_chip_parms {
	uint32_t gpu_drop_freq;
	uint32_t gpu_recover_freq;

	uint32_t dbus_drop_freq;
	uint32_t dbus_recover_freq;
};

/* 电源控制芯片相关信息 */
typedef struct volctrl_chip_hwinfo_t {
	/*power control chip infos*/
	unsigned char slave_addr;
	unsigned char chip_name[INNO_POWERCHIP_NAME_SIZE];
	unsigned pmbus_id;
}VOLCTRL_HWINFO_T;

/* dpu需要的相关硬件信息 */
typedef struct display_hwinfo_t{
	char display_hdmi_nums;             /* HDMI/miniHDMI数量 */
	char display_dp_nums;               /* DP/miniDP/eDP数量 */
	char display_vga_nums;
	char display_dvi_nums;
	char display_lvds_nums;
	char display_dp2vga_en;
	char display_hdmi2dvi_en;           /* bit'b[0]HDMI0转DVI使能 [1]HDMI1转DVI使能 ...        */
	char display_hdmi_dp_en;
	char display_dp2vga_chip[HWINFO_HW_DP2VGA_CHIP_LEN];  /* 标记dp2vga使用的哪个芯片 */
	char display_dp2vga_i2c_id;
	char display_dp2vga_i2c_addr;
	char display_vga_reset_pin;
	char display_vga_hard_reset;
	char hw_4k_en;
}DISPLAY_HWINFO_T;

typedef struct dev_display_type_t{
	unsigned int  disp_type;
	char disp_name[HWINFO_HW_DP2VGA_CHIP_LEN];
}DEV_DISPLAY_TYPE_T;

typedef struct dev_hwinfo_chip_type_t{
	unsigned int  en_chip_type;
	char chip_name[HWINFO_HW_DP2VGA_CHIP_LEN];
}DEV_HWINFO_CHIP_TYPE_T;
struct dev_hw_base_info_t{
	char hw_pcb_id[HWINFO_HW_PCB_ID_LEN];
	char version_id[HWINFO_VERSION_ID_LEN];
	char pcie_lanes;
	char mem_bar_size;
	char power_supply;
	char product_id[HWINFO_HW_PRODUCT_ID_LEN];
	char sn_id[HWINFO_HW_SN_LEN];
	char mem_type;
	char mem_nums;
	char mem_uint_size;
	char init_finished;
	char old_version_flag;
};
typedef struct dev_hwinfo_t{
	struct dev_hw_base_info_t hw_base_info;
	struct power_custom_hwinfo_t hw_power_info;
	struct display_hwinfo_t hw_display_info;
	struct volctrl_chip_hwinfo_t hw_volctl_info;
	struct inno_gpu_opp *opp_tbl;
	int opp_tbl_size;
}DEV_HWINFO_T;

typedef enum raw_intr
{
	RAW_VCODEC = 0,
	RAW_GPU = 1,
	RAW_PCIE = 2,
	RAW_MAX,
}raw_intr_type;

typedef enum raw_action
{
	RAW_R = 0,
	RAW_W = 1,
	RAW_RW = 2,
	RAW_ACT_MAX,
}raw_action_type;

struct chip_obj {
	uint64_t reserved_memsize;//供VBIOS和MCU FW使用
	uint64_t mixed_memsize;
	uint64_t dpu_memsize;
	uint64_t vpu_memsize;
	uint64_t apu_memsize;
	uint64_t hw_memsize; // 硬件真实的gddr大小
	uint64_t dpu_inv_memsize;
	uint64_t vpu_inv_memsize;
	uint64_t apu_inv_memsize;
	int sys_bar_num;
	int ddr_bar_num;
	int pmbus_nr[HAL_MAX_PMBUS_NUMS];
	rsrc_nums_t rsrc_nums;
	uint64_t sys_ctrl_offset;
	uint64_t sys_dev_base;
	bool quirk_gtt;
	int interrupt_count;
	bool has_inv_mem;
	bool has_resize;
	bool has_gtt_mem;
	bool has_limited_gtt_mem;
	int  gtt_window_num;
	uint64_t gtt_window_size;
	bool is_sharing_gpu_heap;
	bool is_gtt_need_falling_back;
	char * chip_name;
	char * chip_alias;
	/* gtt dev base addr */
	uint64_t gtt_dev_base_addr;
	bool is_support_dmar;
	bool is_support_resize;
	int  resize_ddr_index;

	/*misc control pm && freq && temperature*/
	bool is_low_power_enable;
	bool support_idle_switch;
	int dynfreq_algo;
	struct tpid_chip_parms algopid_parms;
	struct tmix_chip_parms algomix_parms;

	/* gpu feature for umd */
	struct hal_gpu_feature gpu_feature;

	struct plugin_miscdev *g_plugin_dev;
	struct dev_hwinfo_t hwinfo;

	uint64_t gtt_support;
	uint64_t smmu_support;
	unsigned int pci_cfg_reg[32];
	bool pci_cfg_reg_cache;
	bool fixup_pcie_access;

	uint64_t (*get_vram_pcie_base)(void *chip_ctx);
	int (*hw_init)(void *chip_ctx);
	int (*dev_quick_test)(void *chip_ctx);
	void (*mask_interrupt)(void *chip_ctx);
	inno_irqreturn_t (*irq_handler)(int irq, void *data);
	void (*enable_msi_mode)(void *chip_ctx);
	void (*irq_init)(void *chip_ctx);
	bool (*check_pcie_irq_response)(void *chip_ctx);
	int (*common_device_register)(void *chip_ctx);
	void (*common_device_unregister)(void *chip_ctx);
	void (*irq_free)(void *chip_ctx);
	//is_suspend: true is s3(resume)    false is s4(thaw/restore)
	void (*pm_resume)(void *chip_ctx, bool is_suspend);
	void (*gpu_init)(void *chip_ctx);
	uint32_t (*get_gpu_irq)(void *chip_ctx, uint32_t index);
	int (*find_chip_irq)(void *chip_ctx, int hal_irq);
	int (*pdp_sys_reset)(void *chip_ctx, enum reg_module reg_m);
	int (*pdp_video_set)(void *chip_ctx, int pdp_num, enum reg_module reg_m);
	int (*pdp_video_set_noclock)(void *chip_ctx, int pdp_num, enum reg_module reg_m);
	int (*subpdp_video_set)(void *chip_ctx, int pdp_num, enum reg_module reg_m);
	uint32_t (*get_pll)(void *chip_ctx, uint32_t module);
	void (*set_pll)(void *chip_ctx, uint32_t module, uint32_t pll);
	void (*set_audio_pll)(void *chip_ctx, enum reg_module reg_m, uint32_t reg1, uint32_t reg2);
	bool (*is_pll_maped)(void *chip_ctx, int hal_pll_mod);
	uint32_t (*init_total_mem_size)(void *chip_ctx);
	uint64_t (*get_bar2_size)(void *chip_ctx);
	uint64_t (*get_fw_size)(void *chip_ctx);
	uint64_t (*get_hw_size)(void *chip_ctx);
	int (*pcie_dma_is_enable)(void *chip_ctx);
	uint64_t (*get_gtt_dev_addr)(void *chip_ctx, uint64_t cpu_pa);
	uint64_t (*get_cpu_paddr_by_gtt_paddr)(void *chip_ctx, uint64_t cpu_pa);
	bool (*is_gtt_mem)(void *chip_ctx, uint64_t gtt_addr);
	bool (*is_left_vram)(void *chip_ctx, uint64_t dev_pa);

	/* display interface */
	int (*get_memsize)(void *chip_ctx);
	int (*get_hdmi_nums)(void *chip_ctx);
	int (*get_dp_nums)(void *chip_ctx);
	int (*get_vga_nums)(void *chip_ctx);
	int (*get_dvi_nums)(void *chip_ctx);
	int (*get_lvds_nums)(void *chip_ctx);
	int (*get_hdmi_dp_en_status)(void *chip_ctx);
	int (*get_dp2vga_chip_type)(void *chip_ctx);
	int (*get_dp2vga_i2c_id)(void *chip_ctx);
	int (*get_dp2vga_i2c_addr)(void *chip_ctx);
	int (*get_vga_reset_pin)(void *chip_ctx);
	int (*get_vga_hard_reset)(void *chip_ctx);
	int (*getflag_dp2vga)(void *chip_ctx, unsigned char index);
	int (*getflag_hdmi2dvi)(void *chip_ctx, unsigned char index);
	int (*get_lvds_vga_misc_en)(void *chip_ctx);
	int (*get_dual_link)(void *chip_ctx);
	int (*get_output_en)(void *chip_ctx, unsigned short *val);
	int (*get_output_mode)(void *chip_ctx, unsigned char index, unsigned char *val);
	int (*get_backlight_mode)(void *chip_ctx, unsigned char index, unsigned char *val);
	int (*pdp_restore_default_cfg)(inno_dev *dev);

	/*bmc*/
	int (*init_reserved_vram)(void *chip_ctx);
	int (*deinit_reserved_vram)(void *chip_ctx);
	int (*get_reserved_vram_val)(void *chip_ctx,unsigned long offset,char *buf,ssize_t count);
	int (*set_reserved_vram_val)(void *chip_ctx,unsigned long offset,char *buf,ssize_t count);
	int (*get_chip_static_info)(void *chip_ctx,char *buf,ssize_t *count);
	int (*get_chip_dyn_info)(void *chip_ctx,char *buf,ssize_t *count,bool develop_mode);
	unsigned int (*get_support_regulator)(void *chip_ctx);
	int (*get_driver_info)(void *chip_ctx,char *buf,ssize_t *count,bool develop_mode);
	int (*get_fw_env)(void *chip_ctx,char *buf,ssize_t *count,bool develop_mode);
	int (*trigger_mcu_intr)(void *chip_ctx,int intr_type);
	int (*get_ddr_info)(void *chip_ctx,char *buf,ssize_t *count,bool develop_mode);
	int (*get_axi_bandwidth_info)(void *chip_ctx,char *buf,ssize_t *count,int monitor_time,bool develop_mode);
	int (*get_axi_latency_info)(void *chip_ctx,char *buf,ssize_t *count, int monitor_time, bool develop_mode);
	int (*get_ddr_bandwidth_info)(void *chip_ctx,char *buf,ssize_t *count,int monitor_time,bool develop_mode);

	/* gpu info init*/
	int (*hwinfo_init)(void *chip_ctx);
	int (*hwinfo_deinit)(void *chip_ctx);
	int (*is_support_4k) (void *chip_ctx);

	/* custom display info  */
	int (*custom_hdmi_type) (void *chip_ctx, unsigned char index);
	int (*custom_dp_type) (void *chip_ctx, unsigned char index);
	int (*custom_lvds_type) (void *chip_ctx, unsigned char index);
	int (*custom_vga_type) (void *chip_ctx, unsigned char index);
	int (*custom_select_4k) (void *chip_ctx);
	int (*custom_hdmi_edid_mode) (void *chip_ctx, unsigned char index);
	int (*custom_dp_edid_mode) (void *chip_ctx, unsigned char index);
	int (*custom_lvds_edid_mode) (void *chip_ctx, unsigned char index);
	int (*custom_vga_edid_mode) (void *chip_ctx, unsigned char index);
	int (*custom_hdmi_edid_data) (void *chip_ctx, unsigned char index, char *buf);
	int (*custom_dp_edid_data) (void *chip_ctx, unsigned char index, char *buf);
	int (*custom_lvds_edid_data) (void *chip_ctx, unsigned char index, char *buf);
	int (*custom_vga_edid_data) (void *chip_ctx, unsigned char index, char *buf);
	int (*get_dpu_match) (void *chip_ctx);

	/*misc control pm && freq && temperature*/
	int (*ovheat_handler)(void *chip_ctx);
	int (*tempctl_algo_params_init)(void *chip_ctx);
	int (*set_voltage)(void *chip_ctx, unsigned int voltage, int sys_stat);
	int (*get_voltage)(void *chip_ctx);
	int (*pcie_speed_switch_to)(void *chip_ctx, int speed);
	int (*pcie_speed_max_cap)(void *chip_ctx, unsigned int *max_cap);
	int (*get_opp_tbl_size)(void *chip_ctx);
	unsigned long(*get_freq_offset)(void *chip_ctx);
	void *(*get_opp_tbl)(void *chip_ctx);
	int (*get_chip_temperature)(void *chip_ctx);
	int (*get_board_temperature)(void *chip_ctx);
	int (*mcufw_comm_irq_trigger)(void *chip_ctx);
	int (*init_mcufw_comm_vram)(void *chip_ctx);
	int (*deinit_mcufw_comm_vram)(void *chip_ctx);
	void (*old_custom_bin_compatible)(void *chip_ctx, void *powerinfo);
	bool (*prj_is_disable_update_vol)(void *chip_ctx);
	int (*get_gpu_freqinfo)(void *chip_ctx);

	ssize_t (*sys_dev_gpu_info_show)(inno_dev *dev, char *buf);
	struct gpu_info_hwinfo (*get_gpu_info_hwinfo)(void *chip_ctx);

	int (*custom_get_odm_vendor)(void *chip_ctx, char *buf);

	int (*custom_get_pcb_version)(void *chip_ctx, char *buf);

	int (*custom_zoom_is_enable) (void *chip_ctx, unsigned char dis_indnx);

	ssize_t (*show_pll)(void *chip_ctx, char *buf, ssize_t count);
	int (*set_pll_by_name)(void *chip_ctx, char *name, uint32_t pll);

        void (*vf_extern_gpu_heap)(void *data_pdev, uint64_t size, uint64_t base);

	/* raw function */
	void (*sync_raw_intr)(void *chip_ctx, raw_intr_type master, raw_action_type action);
	int (*set_gpu_raw_base)(void *chip_ctx, uint64_t dev_addr);
	int (*set_resize_control)(void *chip_ctx, int ddr_index);
	/* dma support */
	void (*dma_rsrc_init)(void *chip_ctx);


	/*========================== g3 add ================================*/
	uint64_t gtt_dev_total_size;
	int innolink_chip_id;
	bool innolink_dual_chip_en;

	bool (*dev_deep_test)(void *chip_ctx);
	int (*pcie_ddr_test_attr)(void *chip_ctx, int worknum, int worksize, unsigned int *ddr_test_over, unsigned long ddr_test_offset);
	int (*set_gtt_write_mask_statistics)(void *chip_ctx, int gtt_aw_count);
	int (*get_gtt_write_mask_statistics)(void *chip_ctx, char *buf, ssize_t *count);
	int (*raw_sync_init)(void *chip_ctx, uint64_t dev_addr);
#if defined(G3_RAW_INT_ENABLE)
	int (*irq_raw_enable)(void *chip_ctx);
#endif

	/* ppu power interfaces */
	bool (*module_power_on)(void *chip_ctx, uint32_t id);
	bool (*module_power_off)(void *chip_ctx, uint32_t id);
	bool (*module_power_reset)(void *chip_ctx, uint32_t id, bool auto_on);
	bool (*module_clk_off)(void *chip_ctx, uint32_t id);
	bool (*module_flr_reset)(void *chip_ctx, uint32_t id);

	void (*hw_deinit)(void *chip_ctx);
	void (*irq_innolink_enable)(void *chip_ctx);
};

struct mem_block {
	uint64_t dev_base_addr;
	void* __iomem virt_base_addr;
	uint64_t block_size;
	atomic_t left_size;

	dul_node blk_first;
	dul_node blk_last;
	inno_spinlock *blk_lock;
};

struct dev_dma_chan {
	int ref_cnt;
	inno_dma_chan *chan;
	inno_mutex *pLock;
};

struct dma_filter_param {
	char *dma_type;
	char *dir;
	inno_dev *dma_dev;
};

enum hal_dma_chan_type {
	HAL_PCIE_DMA_RD = 0,
	HAL_PCIE_DMA_WR,

	HAL_AXI_DMA_LEFT,
	HAL_AXI_DMA_RIGHT,

	HAL_DMA_CHAN_TYPE_MAX,
};

struct dma_chan_info {
	int  chan_cnt;
};

struct dma_obj {
	struct dma_chan_info chans_info[HAL_DMA_CHAN_TYPE_MAX];
	void *chan_pool;
};

#define INNO_PM_MODE_NUMS	4
struct power_userspace_data {
	bool valid;
	unsigned long userlevel;

	int tbl_size;
	unsigned long  *level2freq;

	unsigned int user_pm_mode;
};

struct temperature_freq {
	int temp;
	unsigned long freq;
};


#define INNO_FREQ_TABLE_SIZE	8
struct innothermal {
	inno_dev *posdev;	//from drm_device->dev
	inno_dev *ppci_bdev;//pci_base_device

	struct thermal_zone_device *thermal_dev;
	struct thermal_zone_device *thermal_bdev;
};

struct innopower_temp_pidctl {
	/*incremental pid algo: u(k) - u(k-1) = P*[e(k) - e(k-1)] + I*(k) + D*[e(k) - 2e(k-1) + e(k-2)] */
	int P;              /*pid algo param: proportion factor*/
	int I;              /*pid algo param: integral factor*/
	int D;              /*pid algo param: differential factor*/
	int ref_temp;       /*pid algo param: target and reference value*/

	/*the temperature range of pid algo work*/
	int recover_temp;   /*below recover_temp, exit pid algo and use default value*/
	int warnning_temp;  /*higher than warnning_temp, take mininum freq, default is 500M*/
	int over_temp;      /*greater than or equal to over_temp, host drv notify mcufw to power down for board*/

	/*correlation error used by pid algo*/
	int cur_temp;
	int pre_tmp_error;  /*e(k-1)*/
	int ppre_tmp_error; /*e(k-2)*/

	/*output freq of pid*/
	long pid_out_freq;

	/*current real work freq*/
	long real_target_freq;

	/*fix freqs taken by pid nearby*/
	long outfreq[PIDCTL_OUT_FREQ_NUM];
	long max_freq;
	long min_freq;

	long tolerance;

	/*algo work cycle period*/
	long timeout;
	int time;
};

struct innopower_temp_mixctl{
	long gpu_drop_freq;
	long gpu_recover_freq;
};

enum load_state {
	LOAD_STATE_ACTIVE,
	LOAD_STATE_IDLE,
	LOAD_STATE_MAX,
};

struct devfreq_inno_govdata {
	unsigned int upthreshold;
	unsigned int downdifferential;
	struct power_userspace_data userinfo;
	struct innothermal *thermal;
	inno_dev *ppci_bdev;//pci_base_device
	void* pvrsrv_dev_node;

	unsigned int enable;

	unsigned int user_mode_powsave;
	unsigned int user_powsave_freq;

	unsigned long curr_freq;
	unsigned long default_freq;
	unsigned long freq_offset;
	struct innopower_temp_pidctl pidc;
	struct innopower_temp_mixctl mixc;

	int keep_dropfreq_stat;
	inno_workqueue *devfreq_wkq;
	inno_dwork *devfreq_dwork;

	/* idle logic */
	int idle_exit_cnt;
	int idle_enter_cnt;
	bool input_kicked;
	inno_dwork *input_kick_dwork;
	bool support_idle_switch;
	enum load_state load_state;

	#define INNO_GOV_NAME 20
	char name[INNO_GOV_NAME];
	int id;
};

int fh2m_inno_governor_param_init(inno_dev *posdev, void *pvrsrv_dev_node, struct devfreq_inno_govdata *data);
int fh2m_inno_governor_param_deinit(inno_dev *posdev, struct devfreq_inno_govdata *data);
uint32_t hal_innopwr_cmd_ver_init(inno_dev *dev);
struct innothermal *fh2m_inno_thermal_register(inno_dev *posdev);
int fh2m_inno_thermal_unregister(struct innothermal *pinnothmal);
enum user_work_mode {
	HAL_GPU_WORK_MODE_NORMAL=0,
	HAL_GPU_WORK_MODE_POWERSAVE=1,
	HAL_GPU_WORK_MODE_PERFORMENCE=2,
	HAL_GPU_WORK_MODE_DYN=3
};

#define FANCTRL_REVERSE_DIR 0	/*-*/
#define FANCTRL_POSITIVE_DIR 1	/*+*/
#define FANCTRL_ENABLE 	1
struct inno_gpu_opp
{
	unsigned int volt;
	unsigned int freq;
};

struct mcufwc_static_data {
		char fw_rel_time[32];
};

typedef struct mcufw_ver_desc {
	uint32_t mcufw_major;
	uint32_t mcufw_minor;
	uint32_t mcufw_revision;
	uint32_t mcufw_build;
} mcufw_ver_desc_t;

#define VERSION_FORMAT_3BIT 3
#define VERSION_FORMAT_4BIT 4
typedef struct mcufw_ver_info {
	uint8_t mcufw_v_format;
	uint8_t internal_verison;
	uint8_t inited;
	mcufw_ver_desc_t mcufw_ver_desc;
} mcufw_ver_info_t;

#define MODULE_TAGNAME_MAX 32U
struct inno_module_etime {
	long long pcie_loadtime;
	struct module_item {
		long long etime;
		char tagname[MODULE_TAGNAME_MAX];
	} *module_ptr;
	int module_num;
	inno_mutex *etime_mutex;
};

typedef struct hal_bmc_desc {
	uint32_t is_valid;
	uint32_t crcdata;
	uint32_t status;
	uint32_t verison;
	uint32_t vram_start_addr;
	uint32_t vram_len;
	uint32_t reserved[3];
} hal_bmc_desc_t;

typedef struct hal_bmc {
	hal_bmc_desc_t bmc_desc;

	uint32_t verison;
	void __iomem* bmc_vram_base;
	int (*rd32)(inno_dev *dev, enum reg_module reg_m,
		enum reg_entity entity, unsigned int *return_val);
	int (*wr32)(inno_dev *dev, enum reg_module reg_m,
		enum reg_entity entity, unsigned int val);
} hal_bmc_t;

typedef struct efuse_desc {
	uint32_t is_valid;
	uint32_t crcdata;
	uint32_t status;
	uint32_t verison;
	uint32_t vram_start_addr;
	uint32_t vram_len;
	uint32_t reserved[3];
} efuse_desc_t;

typedef struct gpupll_screen_info {
	bool ft_is_valid;
	uint32_t ft_spec_lvl;

	bool slt_is_valid;
	uint32_t slt_spec_lvl;
} gpupll_screen_info_t;

typedef struct hal_efuse {
	efuse_desc_t efuse_desc;

	uint32_t verison;
	gpupll_screen_info_t gpupll_screen_info;

	uint32_t *data;
	void __iomem* efuse_vram_base;
} hal_efuse_t;

struct custom_verison {
	int major;
	int minor;
};

typedef struct gpu_freqinfo {
	int maxfreq;
	int minfreq;
} gpu_freqinfo_t;

struct vpuinfo_s {
	/* vpu add static vpu_workload */
	atomic64_t vpu_type[HAL_MAX_VPU_CHANS];
	atomic64_t vpu_usage[HAL_MAX_VPU_CHANS];
	atomic64_t vpu_mem_total;
	atomic64_t vpu_mem_used;
	atomic64_t vpu_mem_free;
};

#define RELEASE_VERSION false
#define DEBUG_VERSION true
struct dev_rsrc {
	chip_type_e chip_type;
	struct chip_obj chip;
	int pcie_func_idx;
	inno_pci_dev* pdev;			// 对应的pci设备
	inno_dev *dev;
	void *phwinfo;
	void *gpu_info_plugin;
	void *priv;
	void *dev_node;
	resource_size_t bar0_paddr;		// bar0的物理地址
	void __iomem* bar0_reg;			//bar0对应的寄存器起始地址
	void __iomem* sys_reg;			// 系统寄存器
	void __iomem *ddr_base;                 // 系统显存
	void __iomem* mcu_reserved_vram;		// 保留显存
	void __iomem* hwinfo_vram;				//gpuinfo的保留显存基于mcu空间分配;
	void __iomem* gddr[HAL_MAX_DDR_CHIP][HAL_MAX_DDR_CHANNEL]; // gddr信息
	struct vram_cfg vram_cfg;	// 显存配置项
	//struct vf_item sriov_pf;	// sriov pf设备
	//struct vf_item sriov_vfs[HAL_MAX_VF_NUMS];	// sriov vf设备
#if defined(SUPPORT_ION)
        struct ion_heap *ion_heaps[HAL_MAX_GPU_CORES];
#endif
	int mtrr;									// CPU访问内存时支持缓存
	inno_spinlock *irq_en_lock;						// irq使能变量锁
	struct irq_handle irq_handlers[IRQ_NUMS];
	atomic_t pcie_irq_responsed;
	int pcie_irq_mode;
	inno_completion *pcie_irq_completion;
	inno_dentry* debugfs_dir;		// debugfs目录
	inno_dentry* debugfs_hwdir;	// debugfs目录
	inno_dentry* debugfs_name;	// debugfs文件名
	inno_dentry* syspll_debugfs;	// debugfs syspll
	inno_dentry* test_temp_debugfs;	// debugfs test_temp_debugfs

	inno_platform_device* hdmi_dev[HAL_MAX_HDMI_NUMS];// hdmi平台设备
	inno_platform_device* dp_dev[HAL_MAX_DP_NUMS];	// dp平台设备
	inno_platform_device* vga_dev[HAL_MAX_VGA_NUMS];	// vga平台设备
	inno_platform_device* dpu_dev[HAL_MAX_DPU_CHANS];	// dpu平台设备
	inno_platform_device* vkms_dev[HAL_MAX_VKMS_NUMS];	// vkms平台设备
	inno_platform_device* lvds_dev[HAL_MAX_LVDS_NUMS];	// lvds平台设备
	inno_platform_device* vpu_dev[HAL_MAX_VPU_CHANS];	// vpu平台设备，之前有4个codec设备
	inno_platform_device* audio_dev[HAL_MAX_AUDIO_NUMS];	// audio平台设备
	inno_platform_device* dma_dev[HAL_MAX_DMA_NUMS];	// dma平台设备
	inno_platform_device* gpu_dev[HAL_MAX_GPU_CORES];			// gpu平台设备，之前是ext_dev
	inno_platform_device* apu_dev[HAL_MAX_APU_CHANS];			// apu平台设备
	inno_platform_device* drm_dev[HAL_MAX_DRM_NUMS];		// drm平台设备
	inno_platform_device* sr_dev[HAL_MAX_SR_NUM];			// sr平台设备
	inno_platform_device* sysdbg_dev;
	struct gpu_platform_data *gpu_pdata;
	inno_platform_device* pmbus_dev[HAL_MAX_PMBUS_NUMS];		// pmbus平台设备
	int cur_pmbus_idx;
	unsigned int pre_setvol;
	inno_i2c_adapter *adapter[HAL_MAX_PMBUS_NUMS];
	struct pmbus_funcs *pmbus_funcs[HAL_MAX_PMBUS_NUMS];
	inno_platform_device* power_dev[HAL_MAX_POWER_NUMS];		// 电源相关
	uint32_t gpu_init_freq;
	uint32_t efuse_gpu_freq;
	uint32_t custom_gpu_freq;
	bool efuse_is_valid;
	uint32_t efuse_spec_lvl;
	bool prj_is_legacy;
	char *prj_name;
	bool pwr_is_debug;

	//struct dpu_platform_data dpu_plat_data[HAL_MAX_DPU_CHANS];

#if defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	int bind_cpu_enable;
	unsigned int bind_cpu_num;
	unsigned int bind_cpu_offset;
#endif //END CONFIG_NUMA __INNO_CONTAINER__

	inno_dwork *mornitor_dwork;
	inno_workqueue *mornitor_wkq;
	inno_mutex *chiptemp_mutex;
	struct power_custom_hwinfo_t pwrinfo;

	bool vol_is_digital;
	bool htemp_monitor_support; /*high temperature monitor support*/
	bool ovheat;
	int normal_mode_dfreq;		/**normal mode default freq*/
	int normal_warn_dfreq;
	signed char reg_temp_min;
	signed char reg_temp_max;
	int test_temp;
	int ov_temp_thd;
	int warn_temp_chip;
	int preov_temp_thd;
	int sleep_vol;
	uint32_t sleep_freq;
	uint32_t vol_cmd_version; 		/*the vol module protocol between kmd and mcufw*/
	bool vol_cmd_inited;

	mcufw_ver_info_t mcufw_ver_info;
	void __iomem* mcufw_comm_vram;				//drv mcufw交互协议区域
	int mcufw_comm_v;
	hal_bmc_t bmc;
	hal_efuse_t efuse;
	struct custom_verison custom_ver;
	gpu_freqinfo_t gfreqinfo;

	struct mcufwc_static_data mcufwc_info;
	inno_mutex *mcufw_comm_mutexs[MCUFW_COMM_MODULE_MAX];
	unsigned int enable_dyn_freq;

	/**for dma support*/
	struct dma_obj dma;
	int dma_chan_cnt;
	struct dev_dma_chan *pcie_dma_rx_chans;
	struct dev_dma_chan *pcie_dma_tx_chans;
	struct dev_dma_chan *axi_dma_chans;

	inno_mutex *dma_mutex;

	void *dma_mem_pool;
	struct mem_block dma_mem_block;

	inno_spinlock *vs_lock;	// video_sel lock

	/* display interface information */
	int interface_nums;
	struct disp_interface_info *interface_info;

	struct hal_irq_maskbit mask_bit[IRQ_NUMS];
	uint32_t irq_status_reg[IRQ_REG_NUMS];
	uint32_t irq_mask_reg[IRQ_REG_NUMS];
	uint64_t ddr_bar_len;
	uint64_t sys_bar_len;
	uint32_t vfid;
	uint64_t host_memory_start_addr;
	uint64_t host_memory_len;
	/* drm_device: mutil card  */
	inno_drm *ddev[HAL_MAX_GPU_CORES];

	struct inno_module_etime etime;

	/** g3 add */
	inno_platform_device *smmu_dev; // smmu平台设备
	void* innosmmu_ops;

	struct vpuinfo_s vpuinfo;
};

struct apu_platform_data  {
	chip_type_e chip_type;
	int dev_idx;
	unsigned int reset_apu_left_bit;
	unsigned int reset_apu_right_bit;
	unsigned long pci_reset_offset;
	uint32_t cycles;
	int sys_ctrl_regs_bar;
	uint32_t apu_reg_size;
};

struct gpu_platform_data {
#if defined(SUPPORT_ION)
	struct ion_device *ion_device;
	int ion_heap_id;
#endif
	/* struct drm_device* ddev; */
	void * ddev;

	/* The testchip memory mode (LOCAL, HOST or HYBRID) */
	int mem_mode;

	/* gtt dev base addr and total size*/
	uint64_t gtt_dev_base_addr;
	uint64_t gtt_dev_total_size;

	/* The base address of the testchip memory (CPU physical address) -
	 * used to convert from CPU-Physical to device-physical addresses
	 */
	resource_size_t cpu_memory_base;

    /*
    *The base address of the testchip memory (Dev physical address)
    */
	resource_size_t dev_memory_base;
	uint64_t host_memory_start_addr;
	uint64_t host_memory_length;
	/* The following is used to setup the services heaps that map to the
	 * ion heaps
	 */
	resource_size_t pdp_heap_memory_base;
	resource_size_t pdp_heap_memory_size;
	resource_size_t rogue_heap_memory_base;
	resource_size_t rogue_heap_memory_size;
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	resource_size_t secure_heap_memory_base;
	resource_size_t secure_heap_memory_size;
#endif

	/* DMA channel names for RGX usage */
	char *inno_dma_tx_chan_name;
	char *inno_dma_rx_chan_name;

	struct vram_node *dev_node_vram;
	int dev_idx;
	void* psDevConfig;
};

struct gpu_stats{
	uint64_t gpu_total_size;
	uint64_t gpu_free_size;
};

struct hal_dma_slave_sg_data {
	inno_sg_table *psSg;
	inno_page     **ppages;
	int           num_pages;
	bool          is_user_addr;
};

struct hal_dma_mapped_buffer
{
	void *virt_addr;
	uint64_t cpu_pa;
	size_t size;
	bool is_contig_addr;
	bool is_gtt_addr;
	uint64_t offset;
	void *data;
};

int hal_vram_init(struct vram_cfg *pvram_cfg, enum system_work_mode sys_mode,
		enum gpu_work_mode gpu_mode, int gpu_core_num);
int hal_inv_vram_init(struct vram_cfg *cfg, enum system_work_mode sys_mode);
int fh2m_hal_get_vram_stats(inno_dev* dev, struct role_target *role, bool cpu_access, struct vram_stats *stats);
int hal_register_vram_alloc_callback(void *pfn_alloc,void *pfn_free,void *pfn_devpa_to_cpupa);

void hal_reset_gpu(struct dev_rsrc *pdev_rsrc);

/*
int hal_set_system_cfg_param(struct system_cfg_param *psys_param);
int hal_get_system_cfg_param(struct system_cfg_param *psys_param);
int hal_set_gpu_cfg_param(struct gpu_cfg_param *pgpu_param);
int hal_get_gpu_cfg_param(struct gpu_cfg_param *pgpu_param);
int hal_set_vram_cfg_param(struct system_cfg_param *psys_param);
int hal_get_vram_cfg_param(struct system_cfg_param *psys_param);
*/

void hal_check_reg_accessiable(struct dev_rsrc *pdev_rsrc);
int hal_gpu_init(struct dev_rsrc* pdev_rsrc);

//chips callback function
void hal_chip_init(struct dev_rsrc *pdev_rsrc);
void hal_chip_deinit(struct dev_rsrc *pdev_rsrc);
void g1_soc_chip_init(struct dev_rsrc* pdev_rsrc);
void g1_soc_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g0_pal_chip_init(struct dev_rsrc* pdev_rsrc);
void g0_soc_chip_init(struct dev_rsrc* pdev_rsrc);
void g0_soc_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g0m_soc_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g0_ne_chip_init(struct dev_rsrc* pdev_rsrc);
void g1p_soc_chip_init(struct dev_rsrc* pdev_rsrc);
void g1p_soc_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g0m_soc_chip_init(struct dev_rsrc* pdev_rsrc);
void g0m_pal_chip_init(struct dev_rsrc* pdev_rsrc);
//g1p virt
void g1p_soc_pf_chip_init(struct dev_rsrc* pdev_rsrc);
void g1p_soc_vf_chip_init(struct dev_rsrc* pdev_rsrc);
void g1p_request_init_data(struct dev_rsrc* pdev_rsrc);

struct power_custom_hwinfo_t *fh2m_hal_get_powerinfo(inno_dev *dev);
void hal_powercustom_init(struct dev_rsrc *pdev_rsrc);
void* fh2m_hal_power_get_opptbl(inno_dev *dev);
int fh2m_hal_power_get_opptbl_size(inno_dev *dev);
unsigned long fh2m_hal_power_get_freq_offset(inno_dev *dev);
ssize_t hal_gpu_info_show(inno_dev *dev, char *buf);
int fh2m_hal_get_dev_id(void *dev);
uint32_t fh2m_hal_get_gpu_core_nums(void);
uint32_t fh2m_hal_get_gpu_core_index(void);
int fh2m_hal_set_gpu_utils_ops(void *handler);
int fh2m_hal_get_gpu_utils(void *dev);
int fh2m_hal_set_gpudrv_clkchange_ops(void *handler);
int fh2m_hal_gpudrv_clkchange(void *dev, bool action);
bool fh2m_hal_gpuchip_is_ovheat(inno_dev *dev);
int fh2m_hal_fanctrl_direction(void *dev);
int fh2m_hal_fanctrl_enable(void *dev);
int fh2m_hal_get_dynfreq_algo(inno_dev *dev);
bool fh2m_hal_support_idle_feature(inno_dev *dev);
void* fh2m_hal_get_tempctl_chip_params(inno_dev *dev);
uint32_t fh2m_hal_get_gpu_drop_freq(inno_dev *dev);
uint32_t fh2m_hal_get_gpu_recover_freq(inno_dev *dev);
uint32_t fh2m_hal_get_dbus_drop_freq(inno_dev *dev);
uint32_t fh2m_hal_get_dbus_recover_freq(inno_dev *dev);
uint16_t fh2m_hal_get_dyn_lpc_gpu_utils(inno_dev *dev);
int fh2m_hal_set_dyn_lpc_pcie_speed(inno_dev *dev, uint32_t gpufreq);
int fh2m_hal_set_dyn_lpc_dbus_freq(inno_dev *dev, uint32_t gpufreq);
int get_test_temp(struct dev_rsrc* pdev_rsrc);
bool is_unlimit_freq(void);
bool fh2m_is_support_update_voltage(inno_dev *dev);
void fh2m_mod_update_voltage_disable(void);
bool support_adj_voltage_perstep(void);
bool fh2m_hal_vol_is_digital(inno_dev *dev);
unsigned int fh2m_hal_pcie_speed_max_cap(inno_dev *dev);
int fh2m_hal_power_init(struct dev_rsrc* pdev_rsrc);
int fh2m_hal_power_deinit(struct dev_rsrc* pdev_rsrc);
int hal_power_sleep(struct dev_rsrc* pdev_rsrc);
int hal_power_wakeup(struct dev_rsrc* pdev_rsrc);
ssize_t fh2m_hal_get_mcufw_release_time(void* chip_ctx, char *firmware_release_time, ssize_t size);
int get_pwrd_l(void);
int get_mod_pcie_drop_timeout(void);
int fh2m_hal_get_mod_pcie_drop_timeout(void);

int get_idle_voltage(void);
bool fh2m_is_mod_update_voltage_enable(void);
void *fh2m_hal_get_gpufreq_info(inno_dev *dev);

bool is_enable_dyn_freq(void);
int hal_pdp_restore_default_cfg(inno_dev *dev);
uint64_t hal_get_modparam_vram_reserved_size(void);
int fh2m_get_pwr_debug_lvl(void);

void hal_module_loadtime_init(inno_dev *dev, long long time);
void fh2m_hal_module_loadtime_register(inno_dev *dev, void *name);
void hal_module_loadtime_unregister_all(inno_dev *dev);
void fh2m_hal_module_loadtime_get(inno_dev *dev, inno_seq_file *m);
int mcufw_comm_module_get_offset(struct dev_rsrc *pdev_rsrc, int mcufw_comm_module);
unsigned int hal_mcufw_comm_crc32(unsigned int crc, unsigned char const *p, unsigned int len);
int hal_read_efuse_word(void *chip_ctx, unsigned int reg_idx, unsigned int *rdata);
#if defined(SRIOV_VF_MODE)
int fh2m_hal_set_vf_extern_vram_size(inno_dev *dev, void *set);
#endif

/* g3 */
void g3_ne_chip_init(struct dev_rsrc* pdev_rsrc);
void g3_ne_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g3_pal_chip_init(struct dev_rsrc* pdev_rsrc);
void g3_pal_chip_deinit(struct dev_rsrc* pdev_rsrc);
void g3_soc_chip_init(struct dev_rsrc *pdev_rsrc);
void g3_soc_chip_deinit(struct dev_rsrc *pdev_rsrc);
#endif // __HAL_H__


