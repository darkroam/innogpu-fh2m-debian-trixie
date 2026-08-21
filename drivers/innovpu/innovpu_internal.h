/*************************************************************************/ /*!
@File			innovpu.h
@Title			innovpu driver header
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu driver header
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

#ifndef __VPU_DRV_INTERNAL_H__
#define __VPU_DRV_INTERNAL_H__
#include "hal.h"
#include "hal_interface.h"
#include "inno_debug.h"
#include "inno_idr.h"
#include "innovpu_for_umd.h"
#include "inno_misc.h"
#include "inno_waitqueue.h"
#include "inno_fence.h"
extern uint64_t g_vpu_debug_level;

#define INNO_VPU_DEBUGFS_NAME "innovpu"

#if defined(linux) || defined(__linux) || defined(ANDROID)
#define SUPPORT_MULTI_INST_INTR
#define WAVE6_CODEC_CQ_ISR
#endif

/* if the driver want to use interrupt service from kernel ISR */
#ifdef SUPPORT_INTERRUPT
#define VPU_SUPPORT_ISR
#else
#endif

#ifdef VPU_SUPPORT_ISR
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
//#define VPU_IRQ_CONTROL
#endif

#ifdef VPU_SUPPORT_ISR
#define VPU_IRQ_NUM (23+32)
#endif

#define INNO_CMD_REG_END                0x00000200
#define VPU_RST_BLOCK_ALL               (0x3fffffff)
#define VPU_MAX_LOAD_SIZE (3840 * 2160 * 60)

#define MAX_VPU_NODE                    128
#define MAX_INST_HANDLE_SIZE            56              /* DO NOT CHANGE THIS VALUE */
#define VPU_MAX_NUM_INSTANCE            32 /* add prefix VPU_ to dodge sr same name definitions */
#define MAX_NUM_VPU_CORE                1
#define MAX_NUM_VCORE                   1
#define ATOMIC_SYNC_TIMEOUT             (60000)
#define BIT_CODE_LEN                    512
#define VPU_CMD_REG_LEN                 512
#define MAX_SW_UART_COUNT               6
#define MAX_SW_UART_INFO_LEN            512

#define VPU_FOUR_BYTES_ANDOPERATION     (0xffffffff)

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VPU_SUPPORT_PLATFORM_DRIVER_REGISTER

#define KBUILD_VPU_REG "inno_vpu_reg"
#define KBUILD_VPU_DBG "inno_vpu_dbg"
#define KBUILD_VPU_INFO "inno_vpu_info"
#define KBUILD_VPU_WARN  "inno_vpu_warn"
#define KBUILD_VPU_ERROR "inno_vpu_error"
#define pr_fmt_vpu_reg(fmt) "[%s][%s:%d]" fmt,KBUILD_VPU_REG,__func__,__LINE__
#define pr_fmt_vpu_dbg(fmt) "[%s][%s:%d]" fmt,KBUILD_VPU_DBG,__func__,__LINE__
#define pr_fmt_vpu_info(fmt) "[%s][%s:%d]" fmt,KBUILD_VPU_INFO,__func__,__LINE__
#define pr_fmt_vpu_warn(fmt) "[%s][%s:%d]" fmt,KBUILD_VPU_WARN,__func__,__LINE__
#define pr_fmt_vpu_error(fmt) "[%s][%s:%d]" fmt,KBUILD_VPU_ERROR,__func__,__LINE__

#define vpu_info(dev,fmt, ...) \
	if (g_vpu_debug_level >= 1) { \
		fh2m_inno_dev_printk(KERN_INFO,dev,pr_fmt_vpu_info(fmt), ##__VA_ARGS__); \
	}

#define TRACE_PRINTK(fmt, ...) \
	if (g_vpu_debug_level >= 2) { \
		fh2m_inno_trace_printk(fmt, ##__VA_ARGS__); \
	}

#define vpu_dbg(dev,fmt, ...) \
	if (g_vpu_debug_level >= 3) { \
		fh2m_inno_dev_printk(KERN_DEBUG,dev,pr_fmt_vpu_dbg(fmt), ##__VA_ARGS__); \
	}

#define vpu_register_dbg(dev,fmt, ...) \
	if (g_vpu_debug_level >= 4) { \
		fh2m_inno_dev_printk(KERN_DEBUG,dev,pr_fmt_vpu_reg(fmt), ##__VA_ARGS__); \
	}


#define vpu_warn(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_WARNING,dev,pr_fmt_vpu_warn(fmt), ##__VA_ARGS__)
#define vpu_error(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_ERR,dev,pr_fmt_vpu_error(fmt), ##__VA_ARGS__)

#define vpu_prinfo(fmt, ...) inno_info(pr_fmt_vpu_info(fmt), ##__VA_ARGS__)
#define vpu_prerr(fmt, ...) inno_error(pr_fmt_vpu_error(fmt), ##__VA_ARGS__)


#define VPU_WAKE_MODE           0
#define VPU_SLEEP_MODE          1
#define VPU_NAME_SIZE           10
#define VDI_NUM_LOCK_HANDLES    5
#define REG_SUCCESS             (0)

#define CLEAR_BIT(val, bit) (val &= ~(1<<bit))
#define SET_BIT(val, bit) (val |= (1<<bit))

#define SLC_IDX0 0
#define SLC_IDX1 1
#define SLC_IDX2 2
#define SLC_IDX3 3
#define USE_SLC_EN  1

#define VPU_SYSERR_VPU_STILL_RUNNING        (0x00001000)
#define ERR_RESULT_NOT_READY                (0x00000800)
#define RUN_CMD_STATUS_FAIL                 (0x0)
#define VPU_RESET_STATUS_RELEASED           (0x0)
#define VPU_BUSY_STATUS_IDLE                (0x0)

typedef struct{
	int vpu_id;
	uint64_t total_size;
	uint64_t free_size;
} vpu_vram_status;
#define VPU_CMD_BUSY_STATUS_REGISTER      0x70  //order preservation issues with pcie
#define VPU_CMD_BUSY_STATUS_REGISTER_CODA 0x160
#define MAX_INTERRUPT_QUEUE (16 * 32)

typedef struct vpu_bit_firmware_info_t {
	unsigned int size;						/* size of this structure*/
	unsigned int core_idx;
	u64 reg_base_offset;
	unsigned short bit_code[BIT_CODE_LEN];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_intr_info_t {
	int timeout;
	int intr_reason;
#ifdef SUPPORT_MULTI_INST_INTR
	int intr_inst_index;
	char reserved[PADDING_LOW];
#endif
} vpudrv_intr_info_t;

typedef void fasync_struct_vpu;
typedef struct vpu_context_t {
	fasync_struct_vpu *async_queue;
#ifdef SUPPORT_MULTI_INST_INTR
	u64 interrupt_reason[VPU_MAX_NUM_INSTANCE];
#else
	u64 interrupt_reason;
#endif
	u32 open_count;					 /*!<< device reference count. Not instance count */
} vpu_context_t;

typedef enum {
	VPU_STATUS_SUCCESS,
	VPU_STATUS_FAIL,
} VPU_STATUS;

typedef void  vpu_kfifo;
typedef struct vpu_instance_fence_msg {
	struct task_struct *vpu_instance_fence_thread;
	bool vpu_exec_fence_stop;
	inno_waitqueue_head *vpu_fence_thread_wq;
	vpu_kfifo *instance_fence_processed_q;
	inno_spinlock *instance_fence_processed_q_lock;
	atomic64_t  vpu_instance_fence_counter;
} vpu_instance_fence_msg_t;

typedef struct vpu_drv_ctx_common {
	vpudrv_buffer_t instance_pool;
	vpudrv_buffer_t common_memory;
	vpudrv_buffer_t pvric_memory;
	vpudrv_buffer_t vpu_register;
	vpu_context_t   vpu_context;
	vpudrv_chip_info_t chip_info;
	vpu_bit_firmware_info_t bit_firmware_info[MAX_NUM_VPU_CORE];
	u32 interrupt_flag[VPU_MAX_NUM_INSTANCE];
	u32 vpu_reg_store[MAX_NUM_VPU_CORE][VPU_CMD_REG_LEN];
	u32 inst_lock_wake_up_flag[VPU_MAX_NUM_INSTANCE];
	vpu_kfifo           *interrupt_pending_q[VPU_MAX_NUM_INSTANCE];
	inno_waitqueue_head *interrupt_wait_q[VPU_MAX_NUM_INSTANCE];
	inno_waitqueue_head *inst_lock_wait_q[VPU_MAX_NUM_INSTANCE];
	inno_spinlock        *inst_lock_info_lock[VPU_MAX_NUM_INSTANCE];
	u32 vpu_id;
	u32 vpu_ppu_id;
	u32 product_code;
	int irq_handler_num;
	u32 vpu_open_ref_count;
	bool is_sharing_gpu_heap;
	inno_dev *dev;
	inno_dev *parent;
	inno_dev *vpudev;
	inno_semaphore *vpu_sem;
	inno_spinlock *kfifo_lock;
	inno_semaphore *enc_sem;   // for enc not have cmd queue
	inno_spinlock *vpu_lock;
	inno_spinlock *inst_index_lock;  //control the selection of instance ID
	struct list_head inst_lock_info_head[VPU_MAX_NUM_INSTANCE];
	struct list_head vbp_head;
	struct list_head pm_buf_head;
	struct list_head inst_list_head;
	char vpuname[VPU_NAME_SIZE];
	atomic64_t invisible_mem;
	atomic64_t visible_mem;
	atomic64_t external_mem;
	atomic64_t statistic_load;
	inno_idr* hostmem_buffers;
	inno_spinlock *cmd_message_lock[VPU_MAX_NUM_INSTANCE];
	struct list_head cmd_message_head[VPU_MAX_NUM_INSTANCE];
	vpu_kfifo *async_cmd_processed_q;
	inno_spinlock *async_cmd_processed_q_lock;

	inno_task *vpu_exec_thread;
	bool vpu_exec_stop;
	inno_waitqueue_head  *vpu_exec_wq;
	atomic64_t vpu_exec_sync_cmd_counter;
	atomic64_t vpu_exec_async_cmd_counter;

	inno_task *vpu_dpc_thread;
	inno_waitqueue_head *vpu_dpc_wq;
	atomic64_t  vpu_exec_dpc_counter;

	inno_task *vpu_sw_uart_thread;
	bool vpu_sw_uart_run;

	inno_spinlock *input_buffer_lock[VPU_MAX_NUM_INSTANCE];
	struct list_head input_buffer_head[VPU_MAX_NUM_INSTANCE];
	vpu_instance_fence_msg_t vpu_fence_msg[VPU_MAX_NUM_INSTANCE];
	uint8_t *mirror_buffer; //used for internal construction of register buffer
	atomic64_t tick_start; // record last CMD_CQ_IN_TICK
	atomic64_t tick_end; // record last CMD_FW_DONE_TICK
	atomic64_t cumulative_tick;
	atomic64_t time_start; // record last CMD_CQ_IN_TICK
	atomic64_t time_end; // record last CMD_FW_DONE_TICK
	atomic64_t cumulative_time;
	atomic64_t timer_start;
	uint32_t  proc_num[VPU_MAX_NUM_INSTANCE]; // processed frame number
	VPU_STATUS status[VPU_MAX_NUM_INSTANCE];
	inno_spinlock *source_buffer_lock[VPU_MAX_NUM_INSTANCE];
	struct list_head source_buffer_head[VPU_MAX_NUM_INSTANCE]; //store buffer information and index
}vpu_drv_context;

typedef enum {
	BUF_TYPE_VPU_MALLOC,
	BUF_TYPE_VPU_EXPORT,
	BUF_TYPE_VPU_IMPORT,
	BUF_TYPE_VPU_MAX
} VPU_BUF_TYPE;

typedef struct vpu_fence_thread_internal {
	vpu_drv_context *vpu_drv_ctx;
	int instance_index;
} vpu_fence_thread_internal_t;

typedef struct vpu_sync_cmd_internal {
	vpu_sync_cmd_t sync_cmd;
	inno_semaphore *sem;
	bool use_kernel_interface;
} vpu_sync_cmd_internal_t;

typedef struct vpu_dma_buffer_info {
	vpu_buffer_info_t buffer_info;
	struct dma_buf *p_dma_buf;
	struct dma_fence *p_dma_fence;
} vpu_dma_buffer_info_t;

typedef struct vpu_buf_obj {
	vpudrv_buffer_t vb;
	vpu_drv_context *ctx;
	struct dma_buf_attachment *import_attach;
	struct sg_table *sgt;
	struct dma_buf *buf;
	VPU_BUF_TYPE buf_type;
	uint64_t map_addr;	/* kernel virtual address in use kernel  only for export and import dma_buf*/
	buffer_source_type buffer_type;
}vpu_buf_obj_t;

typedef struct vpu_async_cmd_internal {
	uint32_t *buffer;
	uint32_t buffer_size; /* in DWORD unit */
	vpu_dma_buffer_info_t dma_buf_info[DMA_BUF_FD_COUNT_MAX];
	struct dma_buf *p_result_buf;
	struct dma_fence *p_result_buf_fence;
	vpu_buf_obj_t output_vb;
	vpu_buf_obj_t result_vb;
	int32_t cmd_type; //CMD_TYPE
	inno_semaphore *sem; //sync dpc thread
	vpu_buf_obj_t time_cost_vb;
	struct dma_buf *p_time_cost;
	uint32_t result_cmd_size;
} vpu_async_cmd_internal_t;

typedef struct vpu_buffer_message {
	struct list_head list;
	vpu_dma_buffer_info_t dma_buf_info[DMA_BUF_FD_COUNT_MAX];
	int buffer_index;
} vpu_buffer_message_t;

typedef struct vpu_source_buffer_message {
	struct list_head list;
	vpu_source_buffer_info_t buffer_info;
} vpu_source_buffer_message_t;

typedef struct vpu_cmd_message {
	struct list_head list;
	bool is_sync_cmd;
	union cmd_internal
	{
		vpu_sync_cmd_internal_t sync_cmd;
		vpu_async_cmd_internal_t async_cmd;
	}cmd_internal_t;
	uint64_t time_stamp; //cmd generation time(ns) used for cmd priority sorting
	uint64_t time_stamp_start; //cmd execution time(ns)
	uint64_t time_stamp_end; //cmd execution end(ns)
	int instance_id;
	uint32_t proc_num; // processed frame number
} vpu_cmd_message_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
	struct list_head list;
	union {
		vpudrv_buffer_t vb;
		vpu_buf_obj_t obj;
	} buf;
	void *filp;
} vpudrv_buffer_pool_t;

/* To track the pm memory buffer */
typedef struct pm_buf_pool_t {
	struct list_head list;
	void* virt_addr;
	u64 phys_addr;
	u32 size;
	u32 domain;
} pm_buf_pool_t;

typedef enum {
	VPU_INST_STATUS_IDLE,
	VPU_INST_STATUS_CREATED,
	VPU_INST_STATUS_DISTROYED
} vpu_inst_status_e;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
	struct list_head list;
	int inst_idx;
	int core_idx;
	void *filp;
	vpu_instance_param_t inst_param;
	vpu_inst_status_e status;
} vpudrv_instanace_list_t;

typedef struct vpudrv_instance_pool_t {
	unsigned char   codecInstPool[VPU_MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
	vpudrv_buffer_t vpu_common_buffer;
	u32 vpu_instance_num;
	u32 instance_pool_inited;
	void* pendingInst;
	u32 pendingInstIdxPlus1;
	u32 doSwResetInstIdxPlus1;
} vpudrv_instance_pool_t;

typedef enum {
	VPUAPI_RET_SUCCESS,
	VPUAPI_RET_FAILURE, // an error reported by FW
	VPUAPI_RET_TIMEOUT,
	VPUAPI_RET_STILL_RUNNING,
	VPUAPI_RET_INVALID_PARAM,
	VPUAPI_RET_NOT_READY,
	VPUAPI_RET_MAX
} vpu_api_ret;

typedef enum {
	VPUDRV_MUTEX_VPU,
	VPUDRV_MUTEX_DISP_FALG,
	VPUDRV_MUTEX_RESET,
	VPUDRV_MUTEX_VMEM,
	VPUDRV_MUTEX_REV1,
	VPUDRV_MUTEX_MAX
} vpudrv_mutex_type;

enum {
	DEC_SEQ_INIT = 1,
	ENC_SEQ_INIT = 1,
	DEC_SEQ_END  = 2,
	ENC_SEQ_END  = 2,
	PIC_RUN      = 3,
	SET_FRAME_BUF = 4,
	ENCODE_HEADER = 5,
	ENC_PARA_SET  = 6,
	DEC_PARA_SET  = 7,
	DEC_BUF_FLUSH = 8,
	RC_CHANGE_PARAMETER = 9,
	VPU_SLEEP           = 10,
	VPU_WAKE            = 11,
	ENC_ROI_INIT        = 12,
	FIRMWARE_GET        = 0xf
};

typedef struct{
	u64 instIndex;
	u64 timestamp;
} vpu_lock_info_t;

typedef struct{
	struct list_head list;
	u64 instIndex;
	u64 timestamp;
} vpu_lock_info_list_t;

typedef enum {
	INT_VPU_INIT_VPU          = 0,
	INT_VPU_WAKEUP_VPU        = 1,
	INT_VPU_SLEEP_VPU         = 2,
	INT_VPU_CREATE_INSTANCE   = 3,
	INT_VPU_FLUSH_INSTANCE    = 4,
	INT_VPU_DESTROY_INSTANCE  = 5,
	INT_VPU_INIT_SEQ          = 6,
	INT_VPU_SET_FRAMEBUF      = 7,
	INT_VPU_DEC_PIC           = 8,
	INT_VPU_ENC_PIC           = 8,
	INT_VPU_ENC_SET_PARAM     = 9,
	INT_VPU_DEC_QUERY         = 14,
	INT_VPU_BSBUF_EMPTY       = 15,
	INT_VPU_BSBUF_FULL        = 15,
} VPU_interrupt_bit;

typedef enum {
	VPU_INIT_VPU        = 0x0001,
	VPU_WAKEUP_VPU      = 0x0002,
	VPU_SLEEP_VPU       = 0x0004,
	VPU_CREATE_INSTANCE = 0x0008,            /* queuing command */
	VPU_FLUSH_INSTANCE  = 0x0010,
	VPU_DESTROY_INSTANCE= 0x0020,            /* queuing command */
	VPU_INIT_SEQ        = 0x0040,            /* queuing command */
	VPU_SET_FB          = 0x0080,
	VPU_DEC_PIC         = 0x0100,            /* queuing command */
	VPU_ENC_PIC         = 0x0100,            /* queuing command */
	VPU_ENC_SET_PARAM   = 0x0200,            /* queuing command */
	VPU_QUERY           = 0x4000,
	VPU_UPDATE_BS       = 0x8000,
	VPU_RESET_VPU	   = 0x10000,
	VPU_MAX_VPU_COMD	= 0x10000,
} VPU_vpu_command;

typedef enum {
	GET_VPU_INFO        = 0,
	SET_WRITE_PROT      = 1,
	GET_RESULT          = 2,
	UPDATE_DISP_FLAG    = 3,
	GET_BW_REPORT       = 4,
	GET_BS_RD_PTR       = 5,    // for decoder
	GET_BS_WR_PTR       = 6,    // for encoder
	GET_SRC_BUF_FLAG    = 7,    // for encoder
	SET_BS_RD_PTR       = 8,    // for decoder
	GET_DEBUG_INFO      = 0x61,
} query_opt;

typedef struct{
	void **src_array;
	void **dst_array;
	int **len_array;
	int *num;
} vpu_transfer_discrete_msg;

#ifdef ANDROID
#define CORE_WAVE521_FIRMWARE_PATH "fw_w5_b.bin"   // for wave521
#define CORE_WAVE511_FIRMWARE_PATH "fw_w5_d.bin"   // for wave511
#define CORE_BODA955_FIRMWARE_PATH "fw_b9_d.bin"   // for boda955
#define CORE_WAVE627_FIRMWARE_PATH "fw_w6_e.bin"   // for wave627
#define CORE_WAVE637_FIRMWARE_PATH "fw_w6_b.bin"   // for wave637
#define CORE_WAVE517_FIRMWARE_PATH "fw_w5_d1.bin"  // for wave517
#else
#define CORE_WAVE521_FIRMWARE_PATH "innogpu/fw_w5_b.bin"  // for wave521
#define CORE_WAVE511_FIRMWARE_PATH "innogpu/fw_w5_d.bin"  // for wave511
#define CORE_BODA955_FIRMWARE_PATH "innogpu/fw_b9_d.bin"  // for boda955
#define CORE_WAVE627_FIRMWARE_PATH "innogpu/fw_w6_e.bin"  // for wave627
#define CORE_WAVE637_FIRMWARE_PATH "innogpu/fw_w6_b.bin"  // for wave637
#define CORE_WAVE517_FIRMWARE_PATH "innogpu/fw_w5_d1.bin" // for wave517
#endif

#define WAVE5_MAX_CODE_BUF_SIZE         (2 * 1024 * 1024)
#define WAVE5_TEMPBUF_SIZE              (1024 * 1024)
#define SIZE_COMMON                     (WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE)

#define INSTANCE_POOL_BUFFER_SIZE       (64 * 1024)

vpudrv_instance_pool_t *get_instance_pool_handle(u32 core, vpudrv_buffer_t *instance_pool);
#endif
