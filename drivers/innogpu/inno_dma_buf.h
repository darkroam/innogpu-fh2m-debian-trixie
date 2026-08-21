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
#ifndef __INNO_DMA_BUF_H__
#define __INNO_DMA_BUF_H__

#include <linux/types.h>

#include "inno_dma.h"

typedef void inno_dma_buf;
typedef void inno_dma_buf_attachment;
typedef void inno_dma_resv;
typedef void inno_iosys_map;

int fh2m_inno_dma_buf_begin_cpu_access(inno_dma_buf *dmabuf, enum inno_dma_data_direction dir);

int fh2m_inno_dma_buf_end_cpu_access(inno_dma_buf *dmabuf, enum inno_dma_data_direction dir);

void *fh2m_inno_dma_buf_vmap(inno_dma_buf *dmabuf);

void fh2m_inno_dma_buf_vunmap(inno_dma_buf *dmabuf, void *addr);

void fh2m_inno_iosys_map_buf_map_set_vaddr_iomem(inno_iosys_map *map, void __iomem *vaddr_iomem);

void fh2m_inno_iosys_map_clear(inno_iosys_map *map);

void *fh2m_inno_dma_buf_kmap(inno_dma_buf *dmabuf, unsigned long page_num);

void fh2m_inno_dma_buf_kunmap(inno_dma_buf *dmabuf, unsigned long page_num, void *addr);

int fh2m_inno_dma_buf_mmap(inno_dma_buf *dmabuf, inno_vm_area *vma, unsigned long pgoff);

inno_dma_buf_attachment *fh2m_inno_dma_buf_attach(inno_dma_buf *dmabuf, void *dev);

inno_dev* fh2m_inno_dma_buf_attach_get_dev(inno_dma_buf_attachment *attach);

void fh2m_inno_dma_buf_detach(inno_dma_buf *dmabuf, inno_dma_buf_attachment *attach);

inno_sg_table *fh2m_inno_dma_buf_map_attachment(inno_dma_buf_attachment *attach, enum inno_dma_data_direction dir);

void fh2m_inno_dma_buf_unmap_attachment(inno_dma_buf_attachment *attach, inno_sg_table *sg_table, enum inno_dma_data_direction dir);

inno_dma_buf *fh2m_inno_dma_buf_get_from_attach(inno_dma_buf_attachment *attach);

void fh2m_inno_get_dma_buf(inno_dma_buf *dmabuf);

int fh2m_inno_dma_buf_fd(inno_dma_buf *dmabuf, int flags);

inno_dma_buf *fh2m_inno_dma_buf_get(int fd);

void fh2m_inno_dma_buf_put(inno_dma_buf *dmabuf);

uint64_t fh2m_inno_dma_buf_size(inno_dma_buf *dmabuf);

void *fh2m_inno_dma_buf_get_priv(inno_dma_buf *dmabuf);

void fh2m_inno_dma_buf_set_priv(inno_dma_buf *dmabuf, void* priv);

const char *fh2m_inno_dma_buf_get_exp_name(inno_dma_buf *dmabuf);

inno_dma_resv *fh2m_inno_dma_buf_get_resv(inno_dma_buf *dmabuf);

void fh2m_inno_dma_cache_sync(void *dev, void *vaddr, uint64_t size, enum inno_dma_data_direction dir);

inno_dma_resv *fh2m_inno_dma_resv_alloc(void);

void fh2m_inno_dma_resv_free(inno_dma_resv *resv);

void fh2m_inno_dma_resv_init(inno_dma_resv *obj);

void fh2m_inno_dma_resv_fini(inno_dma_resv *obj);
char * fh2m_inno_dma_buf_attach_get_name(inno_dma_buf_attachment *attach);
#endif
