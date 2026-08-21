/*************************************************************************/ /*!
@File			innovpu_for_umd.h
@Title			innovpu driver for umd header
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

#ifndef __VPU_FOR_UMD_H__
#define __VPU_FOR_UMD_H__

#ifndef BUILD_WINDOWS_DRIVER
#ifdef __KERNEL__
#include <linux/types.h>  // for kernel
#else
// for umd
#include <stdint.h>
#endif
#endif

#ifndef BUILD_WINDOWS_DRIVER
#define VDI_IOCTL_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY	_IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY		_IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT			_IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE			_IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET						_IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL			_IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY			_IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE				_IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE			_IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM			_IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO			_IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_GET_FREE_MEM_SIZE			_IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_DMABUF_EXPORT				_IO(VDI_IOCTL_MAGIC, 14)
#define VDI_IOCTL_DMABUF_IMPORT				_IO(VDI_IOCTL_MAGIC, 15)
#define VDI_IOCTL_DMABUF_DESTROY			_IO(VDI_IOCTL_MAGIC, 16)
#define VDI_IOCTL_GET_PVRIC_MEMORY			_IO(VDI_IOCTL_MAGIC, 17)/**TODO: why*/
#define VDI_IOCTL_DMAFENCE_SIGNAL			_IO(VDI_IOCTL_MAGIC, 18)
#define VDI_IOCTL_DMAFENCE_CREATE			_IO(VDI_IOCTL_MAGIC, 19)
#define VDI_IOCTL_DMA_TRANSFER				_IO(VDI_IOCTL_MAGIC, 20)
#define VDI_IOCTL_GET_CHIP_INFO				_IO(VDI_IOCTL_MAGIC, 21)
#define VDI_IOCTL_LOCK_ENQUEUE				_IO(VDI_IOCTL_MAGIC, 22)
#define VDI_IOCTL_LOCK_DEQUEUE				_IO(VDI_IOCTL_MAGIC, 23)
#define VDI_IOCTL_TRY_LOCK					_IO(VDI_IOCTL_MAGIC, 24)
#define VDI_IOCTL_UNLOCK					_IO(VDI_IOCTL_MAGIC, 25)
#define VDI_IOCTL_CREATE_INSTANCE			_IO(VDI_IOCTL_MAGIC, 26)
#define VDI_IOCTL_DESTROY_INSTANCE			_IO(VDI_IOCTL_MAGIC, 27)
#define VDI_IOCTL_INCREASE_LOAD				_IO(VDI_IOCTL_MAGIC, 28)
#define VDI_IOCTL_GET_VRAM_INFO				_IO(VDI_IOCTL_MAGIC, 29)
#define VDI_IOCTL_DMA_MAP_HOSTMEM			_IO(VDI_IOCTL_MAGIC, 30)
#define VDI_IOCTL_DMA_UNMAP_HOSTMEM			_IO(VDI_IOCTL_MAGIC, 31)
#define VDI_IOCTL_DMA_TRANSFER_FAST			_IO(VDI_IOCTL_MAGIC, 32)
#define VDI_IOCTL_EXECUTE_SYNC_COMMAND		_IO(VDI_IOCTL_MAGIC, 33)
#define VDI_IOCTL_SEND_ASYNC_COMMAND		_IO(VDI_IOCTL_MAGIC, 34)
#define VDI_IOCTL_CV_OPEN					_IO(VDI_IOCTL_MAGIC, 35)
#define VDI_IOCTL_CV_CLOSE					_IO(VDI_IOCTL_MAGIC, 36)
#define VDI_IOCTL_GET_WORKLOAD				_IO(VDI_IOCTL_MAGIC, 37)
#define VDI_IOCTL_CONFIG_SOURCE_BUFFER		_IO(VDI_IOCTL_MAGIC, 38)
#define VDI_IOCTL_GET_MEMINFO				_IO(VDI_IOCTL_MAGIC, 39)
#define VDI_IOCTL_GET_BUSINFO				_IO(VDI_IOCTL_MAGIC, 40)

#else
#define VDI_IOCTL_OPEN_INSTANCE							9
#define VDI_IOCTL_CLOSE_INSTANCE						10
#define VDI_IOCTL_DMA_TRANSFER						  20
#define VDI_IOCTL_GET_CHIP_INFO							21
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY	34
#define VDI_IOCTL_FREE_PHYSICALMEMORY		    35
#define VDI_IOCTL_DMABUF_MAP								36
#define VDI_IOCTL_DMABUF_UNMAP							37
#define VDI_IOCTL_EXECUTE_SYNC_COMMAND		  38
#endif //BUILD_WINDOWS_DRIVER

#define INNO_VPU_GEM_DOMAIN_CPU       (1 << 0) //visible
#define INNO_VPU_GEM_DOMAIN_VRAM      (1 << 1) //invisible
#define INNO_VPU_GEM_DOMAIN_DISCRETE  (1 << 2) //discrete


#define INNO_GPU_BAR_SIZE_SMALL       (1 << 0)
#define INNO_GPU_BAR_SIZE_2G	      (1 << 1)
#define INNO_GPU_BAR_SIZE_4G	      (1 << 2)

#define PADDING_LOW   4

#define MIRROR_BUFFER_SIZE 0x500
#define WAVE511_REGISTER_MIRROR_BUFFER_SIZE 1024
#define WAVE511_COMMAND_BUFFER_SIZE         1024
#define WAVE517_REGISTER_MIRROR_BUFFER_SIZE 1024
#define WAVE517_COMMAND_BUFFER_SIZE         1024
#define WAVE627_COMMAND_BUFFER_SIZE         1024
#define WAVE627_REGISTER_MIRROR_BUFFER_SIZE 2048
#define BODA955_COMMAND_BUFFER_SIZE         1024
#define BODA955_REGISTER_MIRROR_BUFFER_SIZE 2048
#define WAVE677_COMMAND_BUFFER_SIZE         1024
#define WAVE677_REGISTER_MIRROR_BUFFER_SIZE 2048
/*
IMPORTANT:
All structures below are designed to be the same size when compiled for 32
and/or 64 bit architectures, i.e. there should be no compiler inserted
padding. This is achieved by sticking to the following rules:
1) only use fixed width types
2) always naturally align fields by arranging them appropriately and by using
padding fields when necessary
*/
typedef struct vpudrv_buffer_t {
	uint64_t phys_addr;
	uint64_t base;					/* kernel logical address in use kernel */
	volatile uint64_t virt_addr;				/* virtual user space address */
	uint64_t dev_addr;
	int size;
#ifndef BUILD_WINDOWS_DRIVER
	int fd;
#else
	uint64_t fd;       /* Windows allocation handle */
	uint64_t MDL;      /* Memory description list */
	uint64_t plat_mem; /* PPLAT_MEMORY */
#endif
	uint32_t domain;
	int id;
	// char reserved[PADDING_LOW];
} vpudrv_buffer_t;

typedef struct vpudrv_chip_info_t {
	uint64_t host_bar2_base_addr;
	uint64_t host_bar2_end_addr;
	uint64_t vram_dev_base_addr;
	uint64_t vram_dev_end_addr;
	uint32_t chip_type;
	uint32_t vpu_num;
	uint32_t bar_size;
	uint32_t vf_id;
#if defined(USE_REFACTOR_LOGIC) || defined(BUILD_WINDOWS_DRIVER)
	uint32_t vpu_id;
	uint32_t product_code;
#endif
	char reserved[PADDING_LOW];
} vpudrv_chip_info_t;
typedef struct{
	vpudrv_buffer_t src_vb;
	vpudrv_buffer_t dest_vb;
	int size;
	int direction;
} vpu_dma_info_t;

/*
	Async cmd have no output usually.
*/
#define DMA_BUF_FD_COUNT_MAX 4
typedef enum {
	BUFFER_TYPE_NONE,
	BUFFER_TYPE_INPUT,
	BUFFER_TYPE_OUTPUT,
	BUFFER_TYPE_OUTPUT_NEED_DECODE_COPY,
	BUFFER_TYPE_OUTPUT_NEED_DISPLAY_COPY,
	BUFFER_TYPE_MAX
} buffer_source_type;

typedef struct vpu_buffer_info {
	buffer_source_type buffer_type;
	int32_t buffer_index;
#ifndef BUILD_WINDOWS_DRIVER
	int32_t buffer_fd;
#else
	uint64_t buffer_fd; /* Windows allocation handle */
#endif
	bool skip_fence_track; //already fence wait and create
} vpu_buffer_info_t;

typedef struct vpu_async_cmd {
	uint64_t buffer;/*ptr virt*/
	uint32_t buffer_size; /* in DWORD unit */
	vpu_buffer_info_t fd[DMA_BUF_FD_COUNT_MAX]; /* wait these allocation's fences and create write fences before execute this cmd, signal write fence after. -1 indicate first invalid index here after */
	uint32_t result_cmd_size;
#ifndef BUILD_WINDOWS_DRIVER
	int32_t result_fd;
#else
	uint64_t  result_fd; /* Windows allocation handle */
	uint64_t  patch_buffer;
	uint32_t  patch_buffer_size;
	uint64_t  win_fd;
#endif
	int32_t cmd_type; //CMD_TYPE
	int32_t time_cost_fd;//statistics time cost
} vpu_async_cmd_t;

typedef struct vpu_sync_cmd {
	uint64_t buffer; /* input ptr virt*/
	uint64_t reg_mirror; /* output ptr virt*/
	uint32_t buffer_size; /* input, in DWORD unit*/
	uint32_t reg_mirror_size; /* output, in BYTE unit, because CNM original excel info is in BYTE unit. */
	int32_t  cmd_type;//CMD_TYPE
	uint32_t codec_type; //encoding type or decoding type. for example, W_HEVC_ENC W_AVC_DEC,etc
} vpu_sync_cmd_t;


typedef struct vpudrv_inst_info_t {
	int core_idx;
	int inst_idx;
	int inst_open_count;	/* for output only*/
	char reserved[PADDING_LOW];
} vpudrv_inst_info_t;

typedef struct{
	uint32_t width;
	uint32_t height;
	uint32_t frameRate;
    uint32_t codec_type;
	char reserved[PADDING_LOW];
} vpu_instance_param_t;

typedef struct sync_event{
	int fd;
	bool event;/*write：1， read：0*/
} sync_event_t;

typedef struct {
	uint32_t running_instance;
	uint32_t product_code;
	int workload;
} vpu_workload_info_t;

typedef struct {
	bool is_smallbar;
	uint64_t visiblemem_free;
	uint64_t invisiblemem_free;
} vpu_meminfo_t;

// this structure must be consistent with libva
/**
 * \brief Coded buffer segment.
 *
 * #VACodedBufferSegment is an element of a linked list describing
 * some information on the coded buffer. The coded buffer segment
 * could contain either a single NAL unit, or more than one NAL unit.
 * It is recommended (but not required) to return a single NAL unit
 * in a coded buffer segment, and the implementation should set the
 * VA_CODED_BUF_STATUS_SINGLE_NALU status flag if that is the case.
 */
/** Padding size in 4-bytes */
#define VA_PADDING_LOW 4
typedef struct _va_coded_buffer_segment_copy {
  /**
   * \brief Size of the data buffer in this segment (in bytes).
   */
  uint32_t size;
  /** \brief Bit offset into the data buffer where the video data starts. */
  uint32_t bit_offset;
  /** \brief Status set by the driver. See \c VA_CODED_BUF_STATUS_*. */
  uint32_t status;
  /** \brief Reserved for future use. */
  uint32_t reserved;  //current frame_type
  /** \brief Pointer to the start of the data buffer. */
  uint64_t buf;
  /**
   * \brief Pointer to the next #VACodedBufferSegment element,
   * or \c NULL if there is none.
   */
  uint64_t next;

  /** \brief Reserved bytes for future use, must be zero */
  uint32_t va_reserved[VA_PADDING_LOW];
} va_coded_buffer_segment_copy;

typedef struct {
	uint64_t frame_count;
	uint64_t time_start;
	uint64_t time_end;
} vpu_time_statistics;

typedef enum CMD_TYPE_E {
	WAVE517_CREATE_INST_DEC_ENUM,
	WAVE517_INIT_SEQ_ENUM,
	WAVE517_INIT_VPU_ENUM,
	WAVE517_WAKEUP_VPU_ENUM,
	WAVE517_SET_FB_UPDATE_FB_ENUM,
	WAVE517_SET_FB_DEC_ENUM,
	WAVE517_DEC_PIC_ENUM,
	WAVE517_QUERY_GET_VPU_INFO_ENUM,
	WAVE517_QUERY_GET_RESULT_DEC_ENUM,
	WAVE517_QUERY_UPDATE_DISP_IDC_ENUM,
	WAVE517_QUERY_GET_BW_RESULT_ENUM,
	WAVE517_QUERY_GET_PF_RESULT_ENUM,
	WAVE517_UPDATE_BS_DEC_ENUM,
	WAVE517_UPDATE_VLC_DEC_ENUM,  // unused
	WAVE517_QUERY_GET_BS_RD_PTR_ENUM,
	WAVE517_QUERY_SET_BS_RD_PTR_ENUM,
	WAVE517_DESTORY_INST_DEC_ENUM,
	WAVE517_FLUSH_INST_DEC_ENUM,
	WAVE517_SLEEP_VPU_DEC_ENUM,
	WAVE627_CREATE_INST_ENUM,
	WAVE627_ENC_SET_PARAM_ENUM,
	WAVE627_INIT_SEQ_ENUM,
	WAVE627_SET_FB_ENUM,
	WAVE627_SET_FB_UPDATE_ENUM,
	WAVE627_ENC_PIC_ENUM,
	WAVE627_QUERY_ENC_GET_RESULT_ENUM,
	WAVE627_QUERY_ENC_GET_FLUSH_CMD_INFO_ENUM,  // unused
	WAVE627_GET_VPU_INFO_ENUM,
	WAVE627_INIT_VPU_ENUM,
	WAVE627_DESTORY_INST_ENUM,
	IPCMODEL_CREATE_INSTANCE_ENUM,
	IPCMODEL_SET_PARAM_ENUM,
	IPCMODEL_QUERY_RESULT_SET_PARAM_ENC_ENUM,
	IPCMODEL_ENC_PIC_ENUM,
	BODA955_DEC_SEQ_INIT_ENUM,
	BODA955_DEC_SEQ_END_ENUM,
	BODA955_SET_FRAME_BUF_ENUM,
	BODA955_DEC_PARA_SET_ENUM,
	BODA955_DEC_BUF_FLUSH_ENUM,
	BODA955_FIRMWARE_GET_ENUM,
	BODA955_PIC_RUN_ENUM,
	BODA955_GET_RESULT_DEC_ENUM,
	BODA955_GET_DEC_SEQ_ENUM,
	BODA955_CREATE_INST_DEC_ENUM,
	BODA955_UPDATE_DISP_IDC_ENUM,
	WAVE511_CREATE_INST_DEC_ENUM,
	WAVE511_INIT_SEQ_ENUM,
	WAVE511_INIT_VPU_ENUM,
	WAVE511_WAKEUP_VPU_ENUM,
	WAVE511_SET_FB_DEC_ENUM,
	WAVE511_DEC_PIC_ENUM,
	WAVE511_QUERY_GET_VPU_INFO_ENUM,
	WAVE511_QUERY_GET_RESULT_DEC_ENUM,
	WAVE511_QUERY_UPDATE_DISP_IDC_ENUM,
	WAVE511_QUERY_GET_BS_RD_PTR_ENUM,
	WAVE511_UPDATE_BS_DEC_ENUM,
	WAVE511_DESTORY_INST_DEC_ENUM,
	WAVE511_FLUSH_INST_DEC_ENUM,
	WAVE511_SLEEP_VPU_DEC_ENUM,
	IPCMODEL_QUERY_RESULT_ENC_PIC_ENUM,
	WAVE677_CREATE_INST_ENUM,
	WAVE677_ENC_SET_PARAM_ENUM,
	WAVE677_INIT_SEQ_ENUM,
	WAVE677_SET_FB_ENUM,
	WAVE677_SET_FB_UPDATE_ENUM,
	WAVE677_SET_DISP_DEC_ENUM,
	WAVE677_QUERY_GET_BW_RESULT_ENUM,
	WAVE677_ENC_PIC_ENUM,
	WAVE677_DEC_PIC_ENUM,
	WAVE677_QUERY_ENC_GET_RESULT_ENUM,
	WAVE677_QUERY_DEC_GET_RESULT_ENUM,
	WAVE677_QUERY_GET_FB_UPDATE_STATUS_ENUM,
	WAVE677_QUERY_ENC_GET_FLUSH_CMD_INFO_ENUM,  // unused
	WAVE677_QUERY_DEC_GET_FLUSH_CMD_INFO_ENUM,  // unused
	WAVE677_DESTORY_INST_ENUM,
} CMD_TYPE;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                               \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | \
	 ((uint32_t)(uint8_t)(ch3) << 24))
#endif /* MAKEFOURCC */

typedef enum INSTRUCTIONS_E {
	WRITE_VALUE_TO_REGISTER = MAKEFOURCC('w', 'v', '2', 'r'),
	READ_REGISTER_TO_MIRROR = MAKEFOURCC('r', 'r', '2', 'm'),
	WAIT_REGISTER_TO_VALUE  = MAKEFOURCC('w', 'r', '2', 'v'),
} INSTRUCTIONS;

typedef struct vpu_source_buffer_info {
	int32_t buffer_index;
	vpudrv_buffer_t source_buffer;
} vpu_source_buffer_info_t;
#endif

