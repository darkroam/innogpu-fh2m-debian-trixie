/*************************************************************************/ /*!
@File			innovpu_common.h
@Title			innovpu common driver header
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu common driver header
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
#ifndef __INNOVPU_COMMON_H__
#define __INNOVPU_COMMON_H__
#include "hal_interface.h"
#include "innovpu_internal.h"
#define HOSTMEM_MIN_BUFFER 1
#define HOSTMEM_MAX_BUFFER 4096

u32 vpu_get_product_code(vpu_drv_context *vpu_drv_ctx);
#ifdef USE_CODEC_NAME//reserved todo...
void vpu_set_vpudev_name(inno_dev *vpudev, u32 product_code, char *name);
void vpu_reset_vpudev_num(inno_dev *vpudev, u32 product_code);
#endif
u64 vpu_get_free_mem_size(vpu_drv_context *vpu_drv_ctx);
void vpu_common_memory_addr_set(vpu_drv_context *vpu_drv_ctx);
int vpu_dma_transfer(vpu_drv_context *vpu_drv_ctx, vpu_dma_info_t info, void *filp);
int vpu_alloc_dma_buffer(vpudrv_buffer_t *vb, vpu_drv_context *vpu_drv_ctx);
void vpu_free_dma_buffer(vpudrv_buffer_t *vb, vpu_drv_context *vpu_drv_ctx);
u32 vpu_free_lock(vpu_drv_context *vpu_drv_ctx, void *filp);
u32 vpu_free_instances(vpu_drv_context *vpu_drv_ctx, void *filp);
u32 vpu_sleep_wake(u32 core, u32 mode, vpu_drv_context *vpu_drv_ctx);
u32 vpu_init_vpu(vpu_drv_context *vpu_drv_ctx);
void vpu_handle_irq(vpu_drv_context *vpu_drv_ctx);
int vpu_suspend(vpu_drv_context *vpu_drv_ctx);
int vpu_resume(vpu_drv_context *vpu_drv_ctx);
int vpu_get_type(uint32_t product_code);
int vpu_hw_reset(vpu_drv_context *vpu_drv_ctx);
int vpu_sw_reset(vpu_drv_context *vpu_drv_ctx);
int vpu_thaw(vpu_drv_context *vpu_drv_ctx);
int vpu_poweroff(vpu_drv_context *vpu_drv_ctx);
int vpu_lock_enqueue(vpu_drv_context *vpu_drv_ctx, vpu_lock_info_t *lock_info);
int vpu_lock_dequeue(vpu_drv_context *vpu_drv_ctx, u64 instIndex);
int vpu_try_lock(vpu_drv_context *vpu_drv_ctx, u64 instIndex);
int vpu_unlock(vpu_drv_context *vpu_drv_ctx, u64 instIndex);
int vpu_create_instance(vpu_drv_context *vpu_drv_ctx, u64 instIndex);
int vpu_destroy_instance(vpu_drv_context *vpu_drv_ctx, u64 instIndex);
int vpu_capture_inst_index(vpu_drv_context *vpu_drv_ctx, u32 core);//get instIndex from codecInstPool
int vpu_release_inst_index(vpu_drv_context *vpu_drv_ctx, u32 core, u64 instIndex);
int vpu_mutex_lock(u32 core, u32 inst, u32 type, vpu_drv_context *vpu_drv_ctx);
void vpu_mutex_unlock(u32 core, u32 inst, u32 type, vpu_drv_context *vpu_drv_ctx);
int vpu_hw_res_init(vpu_drv_context *vpu_drv_ctx);
u32 vpu_get_product_code_manual(vpu_drv_context * vpu_drv_ctx);
int vpu_dma_map_hostmem(vpu_drv_context *vpu_drv_ctx, vpudrv_buffer_t *vb);
void vpu_dma_unmap_hostmem(vpu_drv_context *vpu_drv_ctx, vpudrv_buffer_t *vb);
int vpu_idr_dma_unmap_hostmem(int id, void *ptr, void *data);
int vpu_dma_transfer_fast(vpu_drv_context *vpu_drv_ctx, vpu_dma_info_t info);
int vpu_start_exec_thread(vpu_drv_context *vpu_drv_ctx);
void vpu_exec_sync_cmd(vpu_drv_context *vpu_drv_ctx, vpu_sync_cmd_t *p_sync_cmd, int instance_id, bool use_kernel_interface);
bool vpu_load_firmware_and_INIT_VPU(u32 product_code);
const char* vpu_get_firmware_name(u32 product_code);
void vpu_special_handle_sync_command(vpu_sync_cmd_t sync_cmd, vpu_drv_context *vpu_drv_ctx, int instance_id);
void vpu_codeIndex_configure(vpu_drv_context *vpu_drv_ctx, int index, int instance_id, uint32_t codec_type);
void vpu_get_cmd_mirror_buffer_size(vpu_drv_context *ctx, uint32_t *cmd_buffer_size, uint32_t *mirror_buffer_size);
int vpu_is_creat_inst_cmd(int32_t cmd_type);
int vpu_is_decpic_or_encpic_cmd(int32_t cmd_type);
int vpu_register_request_firmware(vpu_drv_context *vpu_drv_ctx);
void vpu_dma_buf_signal(int fd, bool write, void *dma_fence_out);
void vpu_dma_buf_wait(int fd, void *dma_buf_in, void *dma_fence_out);
u32 vpu_get_semaphore_size(void);
void vpu_disable_sw_uart(vpu_drv_context *vpu_drv_ctx);
u32 vpu_get_kfifo_size(void);
void vpu_add_source_buffer(vpu_drv_context *vpu_drv_ctx, vpu_source_buffer_info_t *buffer_info, int instance_index);
void vpu_release_source_buffer(vpu_drv_context *vpu_drv_ctx, int instance_index);
void vpu_find_source_buffer(vpu_drv_context *vpu_drv_ctx, vpu_source_buffer_info_t *buffer_info, int instance_index);
void innovpu_workload_update(vpu_drv_context *ctx);
int vpu_get_ppu_id(vpu_drv_context* ctx);
void vpu_sync_dpc_thread(vpu_drv_context *vpu_drv_ctx, int instance_id);
#endif
