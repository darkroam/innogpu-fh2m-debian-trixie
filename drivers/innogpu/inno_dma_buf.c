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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
#include <drm/drm_prime.h>
#else
#include <drm/drmP.h>
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
#include <linux/iosys-map.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0))
#include <linux/dma-buf-map.h>
#endif
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>
#include "inno_misc.h"
#include "pvr_dma_resv.h"
#include "inno_dma_buf.h"
#include "kernel_compat.h"
#include "kernel_compatibility.h"


MODULE_IMPORT_NS(DMA_BUF);

int fh2m_inno_dma_buf_begin_cpu_access(inno_dma_buf *dmabuf, enum inno_dma_data_direction dir)
{
	enum dma_data_direction _dir = fh2m_inno_get_dma_direction(dir);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)) && \
	(!defined(CHROMIUMOS_KERNEL) || (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)))
	struct dma_buf *buf = (struct dma_buf *)dmabuf;
	return dma_buf_begin_cpu_access(buf, 0, buf->size, _dir);
#else
	return dma_buf_begin_cpu_access(dmabuf, _dir);
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_begin_cpu_access);

int fh2m_inno_dma_buf_end_cpu_access(inno_dma_buf *dmabuf, enum inno_dma_data_direction dir)
{
	enum dma_data_direction _dir = fh2m_inno_get_dma_direction(dir);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)) && \
	(!defined(CHROMIUMOS_KERNEL) || (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)))
	struct dma_buf *buf = (struct dma_buf *)dmabuf;
	dma_buf_end_cpu_access(buf, 0, buf->size, _dir);
	return 0;
#else
	return dma_buf_end_cpu_access(dmabuf, _dir);
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_end_cpu_access);

void *fh2m_inno_dma_buf_vmap(inno_dma_buf *dmabuf)
{
#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
	return dma_buf_vmap(dmabuf);
#else
#if (DRM_VERSION >= KERNEL_VERSION(5, 18, 0))
	struct iosys_map map;
#else
	struct dma_buf_map map;
#endif
	dma_buf_vmap(dmabuf, &map);
	return map.vaddr;
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_vmap);

void fh2m_inno_dma_buf_vunmap(inno_dma_buf *dmabuf, void *addr)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
	dma_buf_vunmap(dmabuf, addr);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
	struct iosys_map map;
	iosys_map_set_vaddr(&map, addr);
#else
	struct dma_buf_map map;
	dma_buf_map_set_vaddr(&map, addr);
#endif
	dma_buf_vunmap(dmabuf, &map);
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_vunmap);

void fh2m_inno_iosys_map_buf_map_set_vaddr_iomem(inno_iosys_map *map, void __iomem *vaddr_iomem)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
	printk(KERN_WARNING "%s %d: void function for kernel version lt 5.11.0\n", __func__, __LINE__);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	struct dma_buf_map *buf_map = (struct dma_buf_map *)map;
	dma_buf_map_set_vaddr_iomem(buf_map, vaddr_iomem);
#else
	struct iosys_map *buf_map = (struct iosys_map *)map;
	iosys_map_set_vaddr_iomem(buf_map, vaddr_iomem);
#endif
}
INNO_EXT_SYM(fh2m_inno_iosys_map_buf_map_set_vaddr_iomem);

void fh2m_inno_iosys_map_clear(inno_iosys_map *map)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
	printk(KERN_WARNING "%s %d: void function for kernel version lt 5.11.0\n", __func__, __LINE__);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	struct dma_buf_map *buf_map = (struct dma_buf_map *)map;
	dma_buf_map_clear(buf_map);
#else
	struct iosys_map *buf_map = (struct iosys_map *)map;
	iosys_map_clear(buf_map);
#endif
}
INNO_EXT_SYM(fh2m_inno_iosys_map_clear);

void *fh2m_inno_dma_buf_kmap(inno_dma_buf *dmabuf, unsigned long page_num)
{
#if (DRM_VERSION >= KERNEL_VERSION(5, 6, 0))
	return NULL;
#else
	return dma_buf_kmap(dmabuf, page_num);
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_kmap);

void fh2m_inno_dma_buf_kunmap(inno_dma_buf *dmabuf, unsigned long page_num, void *addr)
{
#if (DRM_VERSION >= KERNEL_VERSION(5, 6, 0))
	return;
#else
	dma_buf_kunmap(dmabuf, page_num, addr);
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_buf_kunmap);

int fh2m_inno_dma_buf_mmap(inno_dma_buf *dmabuf, inno_vm_area *vma, unsigned long pgoff)
{
	return dma_buf_mmap(dmabuf, vma, pgoff);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_mmap);

inno_dma_buf_attachment *fh2m_inno_dma_buf_attach(inno_dma_buf *dmabuf, void *dev)
{
	return dma_buf_attach(dmabuf, dev);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_attach);

inno_dev* fh2m_inno_dma_buf_attach_get_dev(inno_dma_buf_attachment *attach)
{
	return ((struct dma_buf_attachment *)attach)->dev;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_attach_get_dev);

char * fh2m_inno_dma_buf_attach_get_name(inno_dma_buf_attachment *attach)
{
	return ((struct dma_buf_attachment *)attach)->dmabuf->owner->name;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_attach_get_name);


void fh2m_inno_dma_buf_detach(inno_dma_buf *dmabuf, inno_dma_buf_attachment *attach)
{
	dma_buf_detach(dmabuf, attach);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_detach);

inno_sg_table *fh2m_inno_dma_buf_map_attachment(inno_dma_buf_attachment *attach, enum inno_dma_data_direction dir)
{
	enum dma_data_direction _dir = fh2m_inno_get_dma_direction(dir);
	return dma_buf_map_attachment(attach, _dir);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_map_attachment);

void fh2m_inno_dma_buf_unmap_attachment(inno_dma_buf_attachment *attach, inno_sg_table *sg_table, enum inno_dma_data_direction dir)
{
	enum dma_data_direction _dir = fh2m_inno_get_dma_direction(dir);
	dma_buf_unmap_attachment(attach, sg_table, _dir);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_unmap_attachment);

inno_dma_buf *fh2m_inno_dma_buf_get_from_attach(inno_dma_buf_attachment *attach)
{
	return ((struct dma_buf_attachment *)attach)->dmabuf;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_get_from_attach);

void fh2m_inno_get_dma_buf(inno_dma_buf *dmabuf)
{
	get_dma_buf((struct dma_buf *)dmabuf);
}
INNO_EXT_SYM(fh2m_inno_get_dma_buf);

int fh2m_inno_dma_buf_fd(inno_dma_buf *dmabuf, int flags)
{
	return dma_buf_fd(dmabuf, flags);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_fd);

inno_dma_buf *fh2m_inno_dma_buf_get(int fd)
{
	return dma_buf_get(fd);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_get);

void fh2m_inno_dma_buf_put(inno_dma_buf *dmabuf)
{
	dma_buf_put(dmabuf);
}
INNO_EXT_SYM(fh2m_inno_dma_buf_put);

uint64_t fh2m_inno_dma_buf_size(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->size;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_size);

void *fh2m_inno_dma_buf_get_priv(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->priv;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_get_priv);

void fh2m_inno_dma_buf_set_priv(inno_dma_buf *dmabuf, void* priv)
{
	((struct dma_buf *)dmabuf)->priv = priv;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_set_priv);

const char *fh2m_inno_dma_buf_get_exp_name(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->exp_name;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_get_exp_name);

inno_dma_resv *fh2m_inno_dma_buf_get_resv(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->resv;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_get_resv);

void fh2m_inno_dma_cache_sync(void *dev, void *vaddr, uint64_t size, enum inno_dma_data_direction dir)
{
#if defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
	enum dma_data_direction _dir = fh2m_inno_get_dma_direction(dir);
	dma_cache_sync((struct device *)dev, vaddr, size, _dir);
#endif
#endif
}
INNO_EXT_SYM(fh2m_inno_dma_cache_sync);

inno_dma_resv *fh2m_inno_dma_resv_alloc(void)
{
	struct dma_resv *resv =  (struct dma_resv *)kzalloc(sizeof(struct dma_resv), GFP_KERNEL);
	return resv;
}
INNO_EXT_SYM(fh2m_inno_dma_resv_alloc);

void fh2m_inno_dma_resv_free(inno_dma_resv *resv)
{
	kfree(resv);
}
INNO_EXT_SYM(fh2m_inno_dma_resv_free);

void fh2m_inno_dma_resv_init(inno_dma_resv *obj)
{
	dma_resv_init((struct dma_resv *)obj);
}
INNO_EXT_SYM(fh2m_inno_dma_resv_init);

void fh2m_inno_dma_resv_fini(inno_dma_resv *obj)
{
	dma_resv_fini((struct dma_resv *)obj);
}
INNO_EXT_SYM(fh2m_inno_dma_resv_fini);
