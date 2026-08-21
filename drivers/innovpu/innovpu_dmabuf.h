/*************************************************************************/ /*!
@File			innovpu_dmabuf.h
@Title			innovpu dmabuf driver header
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu dmabuf driver header
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

#ifndef __VPU_DMABUF_H__
#define __VPU_DMABUF_H__

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <linux/dma-buf.h>
#include "innovpu.h"
#define DMA_FENCE_SYNC_SUPPORT
#ifdef DMA_FENCE_SYNC_SUPPORT
#include "pvr_dma_resv.h"
#include "pvr_linux_fence.h"
#endif
/*
typedef struct vpu_buf_obj {
	vpudrv_buffer_t vb;
	vpu_drv_context *ctx; //struct device *dev;
	struct dma_buf_attachment *import_attach;
	struct sg_table *sgt;
}vpu_buf_obj_t;
*/

#define to_vpu_buf_obj(pvb)	container_of(pvb, struct vpu_buf_obj, vb)
int vpu_export_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj);
vpudrv_buffer_t *vpu_import_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj);
//void vpu_destroy_dmabuf(vpu_drv_context *ctx, int fd);
void vpu_destroy_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj);
int vpu_dmafd_to_vb (vpu_drv_ctxs *vpu_drv_ctx, int fd, vpudrv_buffer_t *vb);
#ifdef DMA_FENCE_SYNC_SUPPORT
int vpu_dma_fence_create(int fd, bool write, struct dma_buf *dma_buf_in, struct dma_fence **dma_fence_out);
int vpu_dma_fence_signal(int fd, bool write, struct dma_buf *dma_buf_in);
int vpu_dma_fence_wait(int fd, struct dma_buf *dma_buf_in, struct dma_fence *dma_fence_out);
#endif
#endif
