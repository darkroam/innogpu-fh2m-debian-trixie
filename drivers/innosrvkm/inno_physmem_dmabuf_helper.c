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
#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
#include <drm/drm_prime.h>
#else
#include <drm/drmP.h>
#endif
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 18, 0))
#include <linux/iosys-map.h>
#elif (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
#include <linux/dma-buf-map.h>
#endif

#include "inno_misc.h"
#include "inno_srvkm.h"
#include "physmem_dmabuf.h"
#include "inno_physmem_dmabuf_helper.h"
#include "kernel_compatibility.h"
#include "physmem_dmabuf_internal.h"

MODULE_IMPORT_NS(DMA_BUF);

/**************************************************************************************************
 * dma_buf_ops (non-GEM)
 *
 * Implementation of below callbacks adds the ability to export dma_bufs to other
 * drivers.
 */
static int inno_pmr_dma_buf_ops_attach(struct dma_buf *buf,
#if ((DRM_VERSION < KERNEL_VERSION(4, 19, 0)) && \
	!((DRM_VERSION >= KERNEL_VERSION(4, 14, 0)) && (defined(CHROMIUMOS_KERNEL))))
							  struct device *dev,
#endif
							  struct dma_buf_attachment *attach)
{
	return PVRDmaBufOpsAttach(buf, attach);
}

static struct sg_table *inno_pmr_dma_buf_ops_map(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	return (struct sg_table *)PVRDmaBufOpsMap(attach, (enum inno_dma_data_direction)dir);
}

static void inno_pmr_dma_buf_ops_unmap(struct dma_buf_attachment *attach,
	struct sg_table *table, enum dma_data_direction dir)
{
	PVRDmaBufOpsUnmap(attach, table, (enum inno_dma_data_direction)dir);
}

static void inno_pmr_dma_buf_ops_release(struct dma_buf *dma_buf)
{
	PVRDmaBufOpsRelease(dma_buf);
}

static int inno_pmr_dma_buf_ops_begin_cpu_access(struct dma_buf *dma_buf,
#if (!defined(CHROMIUMOS_KERNEL) && (DRM_VERSION < KERNEL_VERSION(4, 6, 0)))
                                      size_t uiStart,
                                      size_t uiLength,
#endif
                                      enum dma_data_direction dir)
{
	return PVRDmaBufOpsBeginCpuAccess(dma_buf, (enum inno_dma_data_direction)dir);
}

#if (!defined(CHROMIUMOS_KERNEL) && (DRM_VERSION < KERNEL_VERSION(4, 6, 0)))
static void inno_pmr_dma_buf_ops_end_cpu_access(struct dma_buf *dma_buf,
                                     size_t uiStart,
                                     size_t uiLength,
                                     enum dma_data_direction dir)
#else
static int inno_pmr_dma_buf_ops_end_cpu_access(struct dma_buf *dma_buf,
                                    enum dma_data_direction dir)
#endif
{
	int iErr;

	iErr = PVRDmaBufOpsEndCpuAccess(dma_buf, (enum inno_dma_data_direction)dir);

#if (defined(CHROMIUMOS_KERNEL) || (DRM_VERSION >= KERNEL_VERSION(4, 6, 0)))
	return iErr;
#endif
}

#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
static void *inno_pmr_dma_buf_ops_kmap(struct dma_buf *dma_buf, unsigned long page_num)
{
	return PVRDmaBufOpsKMap(dma_buf, page_num);
}

static void inno_pmr_dma_buf_ops_kunmap(struct dma_buf *dma_buf, unsigned long page_num, void *mem)
{
	return PVRDmaBufOpsKUnMap(dma_buf, page_num, mem);
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
static void *inno_pmr_dma_buf_ops_vmap(struct dma_buf *dma_buf)
{
	return PVRDmaBufOpsVMap_lt_5_11_0(dma_buf);
}
#else /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */
#if (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static int inno_pmr_dma_buf_ops_vmap(struct dma_buf *dma_buf, struct dma_buf_map *map)
#else /* DRM_VERSION < KERNEL_VERSION(5, 18, 0) */
static int inno_pmr_dma_buf_ops_vmap(struct dma_buf *dma_buf, struct iosys_map *map)
#endif
{
	return PVRDmaBufOpsVMap_ge_5_11_0(dma_buf, map);
}
#endif /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
static void inno_pmr_dma_buf_ops_vunmap(struct dma_buf *dma_buf, void *vaddr)
{
	return PVRDmaBufOpsVUnMap_lt_5_11_0(dma_buf, vaddr);
}
#else /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */
#if (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static void inno_pmr_dma_buf_ops_vunmap(struct dma_buf *dma_buf, struct dma_buf_map *map)
#else /* DRM_VERSION < KERNEL_VERSION(5, 18, 0) */
static void inno_pmr_dma_buf_ops_vunmap(struct dma_buf *dma_buf, struct iosys_map *map)
#endif
{
	return PVRDmaBufOpsVUnMap_ge_5_11_0(dma_buf, map);
}
#endif /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */

static int inno_pmr_dma_buf_ops_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	return PVRDmaBufOpsMMap(dma_buf, vma);
}

static const struct dma_buf_ops inno_pmr_dma_buf_ops = {
	.attach        = inno_pmr_dma_buf_ops_attach,
	.map_dma_buf   = inno_pmr_dma_buf_ops_map,
	.unmap_dma_buf = inno_pmr_dma_buf_ops_unmap,
	.release       = inno_pmr_dma_buf_ops_release,
	.begin_cpu_access = inno_pmr_dma_buf_ops_begin_cpu_access,
	.end_cpu_access   = inno_pmr_dma_buf_ops_end_cpu_access,
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#if ((DRM_VERSION < KERNEL_VERSION(4, 19, 0)) && \
	!((DRM_VERSION >= KERNEL_VERSION(4, 14, 0)) && (defined(CHROMIUMOS_KERNEL))))
	.map_atomic    = inno_pmr_dma_buf_ops_kmap,
#endif
#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
	.map           = inno_pmr_dma_buf_ops_kmap,
	.unmap         = inno_pmr_dma_buf_ops_kunmap,
#endif
#else /* (DRM_VERSION >= KERNEL_VERSION(4, 12, 0)) */
	.kmap_atomic   = inno_pmr_dma_buf_ops_kmap,
	.kmap          = inno_pmr_dma_buf_ops_kmap,
	.kunmap        = inno_pmr_dma_buf_ops_kunmap,
#endif /* (DRM_VERSION >= KERNEL_VERSION(4, 12, 0)) */
	.mmap          = inno_pmr_dma_buf_ops_mmap,

	.vmap          = inno_pmr_dma_buf_ops_vmap,
	.vunmap        = inno_pmr_dma_buf_ops_vunmap,
};
/* end of dma_buf_ops (non-GEM) */

/**************************************************************************************************
 * dma_buf_ops (GEM)
 *
 * Implementation of below callbacks adds the ability to export dma_bufs to other
 * drivers.
 */
static int inno_pmr_dma_buf_ops_attach_gem(struct dma_buf *buf,
#if ((DRM_VERSION < KERNEL_VERSION(4, 19, 0)) && \
	!((DRM_VERSION >= KERNEL_VERSION(4, 14, 0)) && (defined(CHROMIUMOS_KERNEL))))
							  struct device *dev,
#endif
							  struct dma_buf_attachment *attach)
{
	return PVRDmaBufOpsAttachGEM(buf, attach);
}

static struct sg_table *inno_pmr_dma_buf_ops_map_gem(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	return (struct sg_table *)PVRDmaBufOpsMapGEM(attach, (enum inno_dma_data_direction)dir);
}

static void inno_pmr_dma_buf_ops_unmap_gem(struct dma_buf_attachment *attach,
	struct sg_table *table, enum dma_data_direction dir)
{
	PVRDmaBufOpsUnmapGEM(attach, table, (enum inno_dma_data_direction)dir);
}

// static void inno_pmr_dma_buf_ops_releaseGEM(struct dma_buf *dma_buf)
// {
// 	PVRDmaBufOpsReleaseGEM(dma_buf);
// }

static int inno_pmr_dma_buf_ops_begin_cpu_access_gem(struct dma_buf *dma_buf,
#if (!defined(CHROMIUMOS_KERNEL) && (DRM_VERSION < KERNEL_VERSION(4, 6, 0)))
                                      size_t uiStart,
                                      size_t uiLength,
#endif
                                      enum dma_data_direction dir)
{
	return PVRDmaBufOpsBeginCpuAccessGEM(dma_buf, (enum inno_dma_data_direction)dir);
}

#if (!defined(CHROMIUMOS_KERNEL) && (DRM_VERSION < KERNEL_VERSION(4, 6, 0)))
static void inno_pmr_dma_buf_ops_end_cpu_access_gem(struct dma_buf *dma_buf,
                                     size_t uiStart,
                                     size_t uiLength,
                                     enum dma_data_direction dir)
#else
static int inno_pmr_dma_buf_ops_end_cpu_access_gem(struct dma_buf *dma_buf,
                                    enum dma_data_direction dir)
#endif
{
	int iErr;

	iErr = PVRDmaBufOpsEndCpuAccessGEM(dma_buf, (enum inno_dma_data_direction)dir);

#if (defined(CHROMIUMOS_KERNEL) || (DRM_VERSION >= KERNEL_VERSION(4, 6, 0)))
	return iErr;
#endif
}

#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
static void *inno_pmr_dma_buf_ops_kmap_gem(struct dma_buf *dma_buf, unsigned long page_num)
{
	return PVRDmaBufOpsKMapGEM(dma_buf, page_num);
}

static void inno_pmr_dma_buf_ops_kunmap_gem(struct dma_buf *dma_buf, unsigned long page_num, void *mem)
{
	return PVRDmaBufOpsKUnMapGEM(dma_buf, page_num, mem);
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
static void *inno_pmr_dma_buf_ops_vmap_gem(struct dma_buf *dma_buf)
{
	return PVRDmaBufOpsVMapGEM_lt_5_11_0(dma_buf);
}
#else /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */
#if (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static int inno_pmr_dma_buf_ops_vmap_gem(struct dma_buf *dma_buf, struct dma_buf_map *map)
#else /* DRM_VERSION < KERNEL_VERSION(5, 18, 0) */
static int inno_pmr_dma_buf_ops_vmap_gem(struct dma_buf *dma_buf, struct iosys_map *map)
#endif
{
	return PVRDmaBufOpsVMapGEM_ge_5_11_0(dma_buf, map);
}
#endif /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
static void inno_pmr_dma_buf_ops_vunmap_gem(struct dma_buf *dma_buf, void *vaddr)
{
	return PVRDmaBufOpsVUnMapGEM_lt_5_11_0(dma_buf, vaddr);
}
#else /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */
#if (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static void inno_pmr_dma_buf_ops_vunmap_gem(struct dma_buf *dma_buf, struct dma_buf_map *map)
#else /* DRM_VERSION < KERNEL_VERSION(5, 18, 0) */
static void inno_pmr_dma_buf_ops_vunmap_gem(struct dma_buf *dma_buf, struct iosys_map *map)
#endif
{
	return PVRDmaBufOpsVUnMapGEM_ge_5_11_0(dma_buf, map);
}
#endif /* DRM_VERSION < KERNEL_VERSION(5, 11, 0) */

static int inno_pmr_dma_buf_ops_mmap_gem(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	return PVRDmaBufOpsMMapGEM(dma_buf, vma);
}

static const struct dma_buf_ops inno_pmr_dma_buf_ops_gem =
{
	.attach        = inno_pmr_dma_buf_ops_attach_gem,
	.map_dma_buf   = inno_pmr_dma_buf_ops_map_gem,
	.unmap_dma_buf = inno_pmr_dma_buf_ops_unmap_gem,
	.release       = drm_gem_dmabuf_release,
	.begin_cpu_access = inno_pmr_dma_buf_ops_begin_cpu_access_gem,
	.end_cpu_access   = inno_pmr_dma_buf_ops_end_cpu_access_gem,
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#if ((DRM_VERSION < KERNEL_VERSION(4, 19, 0)) && \
	!((DRM_VERSION >= KERNEL_VERSION(4, 14, 0)) && (defined(CHROMIUMOS_KERNEL))))
	.map_atomic    = inno_pmr_dma_buf_ops_kmap_gem,
#endif
#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
	.map           = inno_pmr_dma_buf_ops_kmap_gem,
	.unmap         = inno_pmr_dma_buf_ops_kunmap_gem,
#endif
#else /* (DRM_VERSION >= KERNEL_VERSION(4, 12, 0)) */
	.kmap_atomic   = inno_pmr_dma_buf_ops_kmap_gem,
	.kmap          = inno_pmr_dma_buf_ops_kmap_gem,
	.kunmap        = inno_pmr_dma_buf_ops_kunmap_gem,
#endif /* (DRM_VERSION >= KERNEL_VERSION(4, 12, 0)) */
	.mmap          = inno_pmr_dma_buf_ops_mmap_gem,

	.vmap          = inno_pmr_dma_buf_ops_vmap_gem,
	.vunmap        = inno_pmr_dma_buf_ops_vunmap_gem,
};
/* end of dma_buf_ops (GEM) */

struct dma_buf * inno_physmem_gem_prime_export(
#if (DRM_VERSION < KERNEL_VERSION(5, 4, 0))
			struct drm_device *drm_dev,
#endif
			struct drm_gem_object *obj,
			int flags)
{
	struct drm_device *p_dev = NULL;
#if (DRM_VERSION < KERNEL_VERSION(5, 4, 0))
			p_dev = drm_dev;
#endif
	return PhysmemGEMPrimeExport(p_dev, obj, flags);
}

void inno_physmem_gem_obj_free(struct drm_gem_object *obj)
{
	PhysmemGEMObjectFree(obj);
}

#if (DRM_VERSION >= KERNEL_VERSION(5, 9, 0))
static const struct drm_gem_object_funcs physmem_gem_obj_funcs= {
	.export = inno_physmem_gem_prime_export,
	.free = inno_physmem_gem_obj_free,
};
#endif

void inno_drm_gem_object_register_funcs(inno_drm_gem_object *obj)
{
#if (DRM_VERSION >= KERNEL_VERSION(5, 9, 0))
	struct drm_gem_object *gem_obj = (struct drm_gem_object *)obj;
	gem_obj->funcs = &physmem_gem_obj_funcs;
#endif
}

struct inno_drm_gem_object_plus_addr{
	uint64_t addr;
	struct drm_gem_object gem_obj;
};

inno_drm_gem_object *fh2m_inno_drm_gem_object_alloc_plus_addrhold(void)
{
	struct inno_drm_gem_object_plus_addr *inno_gem_obj = NULL;
	inno_gem_obj = (struct inno_drm_gem_object_plus_addr *)kzalloc(sizeof(struct inno_drm_gem_object_plus_addr), GFP_KERNEL);

	if(!inno_gem_obj)
		return NULL;
	return &inno_gem_obj->gem_obj;
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_alloc_plus_addrhold);

void fh2m_inno_drm_gem_object_free_plus_addrhold(inno_drm_gem_object *obj)
{
	struct inno_drm_gem_object_plus_addr *inno_gem_obj = container_of(obj, struct inno_drm_gem_object_plus_addr, gem_obj);
	kfree(inno_gem_obj);
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_free_plus_addrhold);

void fh2m_inno_drm_gem_object_set_plus_addrval(inno_drm_gem_object *obj, uint64_t addr)
{
	struct inno_drm_gem_object_plus_addr *inno_gem_obj = container_of(obj, struct inno_drm_gem_object_plus_addr, gem_obj);
	inno_gem_obj->addr = addr;
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_set_plus_addrval);

uint64_t fh2m_inno_drm_gem_object_get_plus_addrval(inno_drm_gem_object *obj)
{
	struct inno_drm_gem_object_plus_addr *inno_gem_obj = container_of(obj, struct inno_drm_gem_object_plus_addr, gem_obj);
	return inno_gem_obj->addr;
}
INNO_EXT_SYM(fh2m_inno_drm_gem_object_get_plus_addrval);

bool fh2m_inno_dma_buf_is_pmr_ops(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->ops == &inno_pmr_dma_buf_ops;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_is_pmr_ops);

bool fh2m_inno_dma_buf_is_gem_ops(inno_dma_buf *dmabuf)
{
	return ((struct dma_buf *)dmabuf)->ops == &inno_pmr_dma_buf_ops_gem;
}
INNO_EXT_SYM(fh2m_inno_dma_buf_is_gem_ops);

inno_dma_buf *fh2m_inno_pmr_dma_buf_export(void *priv, uint64_t size, int flags, void *resv)
{
	struct dma_buf *dma_buf;

	DEFINE_DMA_BUF_EXPORT_INFO(dma_export_info);
	dma_export_info.priv  = priv;
	dma_export_info.ops   = &inno_pmr_dma_buf_ops;
	dma_export_info.size  = size;
	dma_export_info.flags = flags;
	dma_export_info.resv = (struct dma_resv *)resv;
	dma_buf = dma_buf_export(&dma_export_info);

	return dma_buf;
}
INNO_EXT_SYM(fh2m_inno_pmr_dma_buf_export);

inno_dma_buf *fh2m_inno_gem_dma_buf_export(inno_drm_device *drm_dev, inno_drm_gem_object *obj, inno_dma_resv *resv, uint64_t size, int flags)
{
	struct dma_buf *dma_buf;
	DEFINE_DMA_BUF_EXPORT_INFO(dma_export_info);

#if (DRM_VERSION < KERNEL_VERSION(4, 6, 0))
	/*
	 * It isn't possible to specify R/W access from user space,
	 * the DRM ioctl code only allows the DRM_CLOEXEC flag to be
	 * passed. Assume R/W access is required.
	 */
	flags |= O_RDWR;
#endif

	dma_export_info.priv  = obj;
	dma_export_info.ops   = &inno_pmr_dma_buf_ops_gem;
	dma_export_info.size  = size;
	dma_export_info.flags = flags;
	dma_export_info.resv = (struct dma_resv *)resv;

#if (DRM_VERSION >= KERNEL_VERSION(5, 4, 0))
	dma_buf = drm_gem_dmabuf_export(((struct drm_gem_object *)obj)->dev , &dma_export_info);
#elif (DRM_VERSION >= KERNEL_VERSION(4, 9, 0))
	if(fh2m_inno_warn_on(!drm_dev)) {
		printk("%s %d drm_dev shall not be NULL\n", __func__, __LINE__);
		return NULL;
	}
	dma_buf = drm_gem_dmabuf_export((struct drm_device *)drm_dev, &dma_export_info);
#else
	dma_buf = dma_buf_export(&dma_export_info);
#endif

	return dma_buf;
}
INNO_EXT_SYM(fh2m_inno_gem_dma_buf_export);
