/*************************************************************************/ /*!
@File			innovpu_dmabuf.c
@Title			innovpu dmabuf driver
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu dmabuf driver
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
#include "innovpu_dmabuf.h"
#include "inno_misc.h"
#include "inno_mm.h"
#include "innovpu_common.h"
#include "inno_drm_version.h"
#ifndef VPU_NO_DISPLAY
#ifndef ANDROID
#include "innodpu_drm_gem.h"
#endif
#endif


#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
MODULE_IMPORT_NS(DMA_BUF);
#endif

static struct vpudrv_buffer_t *vpu_dmabuf_to_vpubuf(struct dma_buf *buf)
{
	vpu_buf_obj_t *obj;

	if (!buf) {
		vpu_prerr("%s invalid arg buff is null\n", __func__);
		return NULL;
	}

	obj = buf->priv;
	if (!obj) {
		vpu_prerr("%s buf->priv null\n", __func__);
		return NULL;
	}

	return &(obj->vb);
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 19, 0))
static int vpu_attach_dma_buf(struct dma_buf *dma_buf, struct dma_buf_attachment *attach)
#else
static int vpu_attach_dma_buf(struct dma_buf *dma_buf, struct device *d, struct dma_buf_attachment *attach)
#endif
{
	struct vpu_buf_obj *obj;
	struct device *dev;

	if (!dma_buf || !attach) {
		vpu_prerr("%s invalid arg\r\n", __func__);
		return -1;
	}

	obj = dma_buf->priv;
	if (!obj)
		return -1;

	dev = obj->ctx->dev;

	/* Restrict access to innogpu */
	if (WARN_ON(!dev->parent) ||
		dev->parent != attach->dev->parent)
		return -EPERM;

	return 0;
}

static void vpu_detach_dma_buf(struct dma_buf *dma_buf, struct dma_buf_attachment *attachment)
{
	return;
}

static struct sg_table *vpu_map_dma_buf(struct dma_buf_attachment *attachment,
					 enum dma_data_direction dir)
{
	int err;
	struct sg_table *sgt;
	struct vpudrv_buffer_t *vb;

	if (!attachment) {
		vpu_prerr("%s invalid argument attachment\n", __func__);
		return NULL;
	}

	vb = vpu_dmabuf_to_vpubuf(attachment->dmabuf);
	if (!vb) {
		vpu_prerr("%s vpu_dmabuf_to_vpubuf fail\n", __func__);
		return NULL;
	}

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	err = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (err)
		goto err_free;

	sg_dma_address(sgt->sgl) = vb->dev_addr;//vb->phys_addr;
	sg_dma_len(sgt->sgl) = vb->size;

	return sgt;

err_free:
	kfree(sgt);
	return ERR_PTR(err);
}

static void vpu_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *sgt,
				   enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void vpu_dmabuf_release(struct dma_buf *dma_buf)
{
	struct vpu_buf_obj *obj;

	if (!dma_buf) {
		vpu_prerr("%s dma_buf is null\n", __func__);
		return;
	}

	obj = dma_buf->priv;
	if (!obj) {
		vpu_prerr("%s obj is null\n", __func__);
		return;
	}

	vpu_free_dma_buffer(&(obj->vb), obj->ctx);
	memset(&(obj->vb), 0, sizeof(vpudrv_buffer_t));
	dma_buf->priv =NULL;
	vpu_dbg(obj->ctx->vpudev, "dmabuf released");
	kfree(obj);
}

#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
static void *vpu_dmabuf_kmap_atomic(struct dma_buf *dma_buf, unsigned long page_num)
{
	return ERR_PTR(-ENODEV);
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
static void *vpu_dmabuf_kmap(struct dma_buf *dma_buf, unsigned long page_num)
{
	return ERR_PTR(-ENODEV);
}
#endif

static int vpu_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	struct vpudrv_buffer_t *vpu = NULL;

	if (!dma_buf || !vma) {
		vpu_prerr("%s invalid argument\n", __func__);
		return -EINVAL;
	}

	vpu = vpu_dmabuf_to_vpubuf(dma_buf);
	if (!vpu) {
		vpu_prerr("%s vpu_dmabuf_to_vpubuf fail\n", __func__);
		return -EINVAL;
	}

	if (vpu->size < vma->vm_end - vma->vm_start)
		return -EINVAL;
#if (DRM_VERSION < KERNEL_VERSION(6, 3, 0))
	vma->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vma, vma->vm_flags | VM_IO | VM_RESERVED);
#endif
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	fh2m_inno_pgprot_writecombine(&vma->vm_page_prot, &vma->vm_page_prot);
#else
	fh2m_inno_pgprot_noncached(&vma->vm_page_prot, &vma->vm_page_prot);
#endif

	return remap_pfn_range(vma, vma->vm_start, __phys_to_pfn(vpu->phys_addr) + vma->vm_pgoff, vma->vm_end-vma->vm_start, vma->vm_page_prot) ? -EAGAIN : 0;
}

static const struct dma_buf_ops vpu_dmabuf_ops =  {
	.attach = vpu_attach_dma_buf,
	.detach = vpu_detach_dma_buf,
	.map_dma_buf = vpu_map_dma_buf,
	.unmap_dma_buf = vpu_unmap_dma_buf,
	.release = vpu_dmabuf_release,
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
	.map_atomic = vpu_dmabuf_kmap_atomic,
#endif
#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
	.map = vpu_dmabuf_kmap,
#endif
#else
	.kmap_atomic = vpu_dmabuf_kmap_atomic,
	.kmap = vpu_dmabuf_kmap,
#endif
	.mmap = vpu_dmabuf_mmap,
};

#ifdef DMA_FENCE_SYNC_SUPPORT
static const char *vpu_fence_get_driver_name(struct dma_fence *fence)
{
#ifdef CONFIG_KALLSYMS
	//vpu_prinfo(" called by  %pF %s\n", __builtin_return_address(0), __func__);
#endif
	return "vpu dma fence";
}

static const char *vpu_fence_get_timeline_name(struct dma_fence *fence)
{
#ifdef CONFIG_KALLSYMS
	//vpu_prinfo(" called by  %pF %s\n", __builtin_return_address(0), __func__);
#endif
	return "inno_vpu_timeline";
}

static bool vpu_fence_enable_signaling(struct dma_fence * fence)
{
#ifdef CONFIG_KALLSYMS
	//vpu_prinfo(" called by  %pF %s\n", __builtin_return_address(0), __func__);
#endif
	return true;
}

static void vpu_fence_release(struct dma_fence *fence)
{

#ifdef CONFIG_KALLSYMS
	//vpu_prinfo(" called by  %pF %s\n", __builtin_return_address(0), __func__);
#endif
	if (!fence) {
		vpu_prerr("%s invalid argument fence\n", __func__);
		return;
	}

	if (fence->lock) {
		kfree(fence->lock);
		fence->lock = NULL;
	}

	if (fence) {
		kfree(fence);
		fence = NULL;
	}
}

/**double check if it's enough*/
static struct dma_fence_ops vpu_dma_fence_ops = {
	.get_driver_name = vpu_fence_get_driver_name,
	.get_timeline_name = vpu_fence_get_timeline_name,
	.enable_signaling = vpu_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release = vpu_fence_release,
};

int vpu_dma_fence_create(int fd, bool write, struct dma_buf *dma_buf_in, struct dma_fence **dma_fence_out)
{
	int ret = 0;
	spinlock_t *lock;
	struct dma_fence *fence;
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
	struct dma_resv_iter cursor;
	u32 shared_count = 0;
#else
	struct dma_resv_list *list;
#endif

	/* to facilitate kmd direct call */
	struct dma_buf *dmabuf = NULL;
	if (dma_buf_in) {
		dmabuf = dma_buf_in;
		get_dma_buf(dmabuf);
	} else {
		dmabuf = dma_buf_get(fd);
	}
	if (fh2m_inno_is_err_or_null(dmabuf)) {
		vpu_prerr("%s get dmabuf fail\n", __func__);
		return -1;
	}

	lock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (!lock) {
		vpu_prerr("%s lock kzalloc failed\r\n", __func__);
		return -ENOMEM;
	}
	spin_lock_init(lock);
	//初始化excl fence
	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence) {
		vpu_prerr("%s fence kzalloc failed\r\n", __func__);
		kfree(lock);
		return -ENOMEM;
	}
	dma_fence_init(fence, &vpu_dma_fence_ops, lock, 0, 0);

	if (write) {
#if (DRM_VERSION <= KERNEL_VERSION(5, 15, 2))
		INIT_LIST_HEAD(&dmabuf->cb_excl.cb.node);
#else
		INIT_LIST_HEAD(&dmabuf->cb_in.cb.node);
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
		dma_resv_lock(dmabuf->resv, NULL);
#else
		ww_mutex_lock(&(dmabuf->resv->lock), NULL);
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
			dma_resv_add_fence(dmabuf->resv, fence, DMA_RESV_USAGE_WRITE);
#else
		dma_resv_add_excl_fence(dmabuf->resv, fence);
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
		dma_resv_unlock(dmabuf->resv);
#else
		ww_mutex_unlock(&(dmabuf->resv->lock));
#endif
		//vpu_prinfo("dmabuf %p excl fence is %p, refs:%d\n", dmabuf, fence, kref_read(&fence->refcount));
	} else {
#if (DRM_VERSION <= KERNEL_VERSION(5, 15, 2))
		INIT_LIST_HEAD(&dmabuf->cb_shared.cb.node);
#else
		INIT_LIST_HEAD(&dmabuf->cb_out.cb.node);
#endif
#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
		dma_resv_lock(dmabuf->resv, NULL);
#else
		ww_mutex_lock(&(dmabuf->resv->lock), NULL);
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 0, 0))
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
		ret = dma_resv_reserve_fences(dmabuf->resv, 1);
#else
		ret = dma_resv_reserve_shared(dmabuf->resv, 1);
#endif
#else
		ret = dma_resv_reserve_shared(dmabuf->resv);
#endif
		if (ret == 0) {
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
			dma_resv_add_fence(dmabuf->resv, fence, DMA_RESV_USAGE_READ);
			dma_resv_iter_begin(&cursor, dmabuf->resv, DMA_RESV_USAGE_READ);
			dma_resv_for_each_fence(&cursor, dmabuf->resv, DMA_RESV_USAGE_READ, fence) {
				shared_count++;
			}
			dma_resv_iter_end(&cursor);
			if (shared_count == 0) {
				ret = -1;
				vpu_prerr("%s dma resv shared fence is 0\n", __func__);
			}
#else
			dma_resv_add_shared_fence(dmabuf->resv, fence);
			list = dma_resv_shared_list(dmabuf->resv);
			if (list) {
			//vpu_prinfo("dmabuf %p share fence is %p, count is %d, max is %d, refs: %d\n", dmabuf, fence, list->shared_count, list->shared_max, kref_read(&fence->refcount));
			} else {
				ret = -1;
				vpu_prerr("%s list is NULL\n", __func__);
			}
#endif
		} else {
			ret = -1;
			vpu_prerr("%s reservation shared fence error\n", __func__);
		}

#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
		dma_resv_unlock(dmabuf->resv);
#else
		ww_mutex_unlock(&(dmabuf->resv->lock));
#endif
	}

	if (ret == -1) {
		kfree(lock);
		kfree(fence);
		return -1;
	}

	dma_fence_put(fence);
	dma_buf_put(dmabuf);
	*dma_fence_out = fence;
	return ret;
}

int vpu_dma_fence_signal(int fd, bool write, struct dma_buf *dma_buf_in)
{
	int ret = 0, i;

	//vpu_prinfo("signal fence, %d:%d\r\n", fd, event);
	/* to facilitate kmd direct call */
	struct dma_buf *dmabuf = NULL;
	if (dma_buf_in) {
		dmabuf = dma_buf_in;
		get_dma_buf(dmabuf);
	} else {
		dmabuf = dma_buf_get(fd);
	}
	if (fh2m_inno_is_err_or_null(dmabuf)) {
		vpu_prerr("%s get dmabuf fail\n", __func__);
		return -1;
	}

	if (write) {
		/**signal excl fence*/
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
		u32 excl_fences = 0;
		struct dma_fence **fences_excl;
		ret = dma_resv_get_fences(dmabuf->resv, DMA_RESV_USAGE_WRITE, &excl_fences, &fences_excl);
		if (ret) {
			vpu_prerr("%s get fences excl error\n", __func__);
			dma_buf_put(dmabuf);
			return -1;
		}
		for (i=0; i < excl_fences; i++) {
			dma_fence_signal(fences_excl[i]);
			dma_fence_put(fences_excl[i]);
		}
		if (fences_excl)
			kfree(fences_excl);
#else
		struct dma_fence *fence_excl;
		fence_excl = dma_resv_get_excl_unlocked(dmabuf->resv);
		if (fh2m_inno_is_err_or_null(fence_excl)) {
			vpu_prerr("%s get fences error\n", __func__);
			dma_buf_put(dmabuf);
			return -1;
		}
		//vpu_prinfo("fence_excl is %p\n",fence_excl);
		//dma_fence_put(fence_excl);
		dma_fence_signal(fence_excl);
		dma_fence_put(fence_excl);
		//vpu_prinfo("efence refcont: %d", kref_read(&fence_excl->refcount));
#endif
	} else {
		/**signal shared fence*/
		struct dma_fence **fence_shared;
		unsigned int shared_count;

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
			ret = dma_resv_get_fences(dmabuf->resv, DMA_RESV_USAGE_READ, &shared_count, &fence_shared);
#else
		struct dma_fence *fence_excl;
		ret = dma_resv_get_fences(dmabuf->resv, &fence_excl, &shared_count, &fence_shared);
#endif
		if (ret) {
			vpu_prerr("%s get fences error\n", __func__);
			dma_buf_put(dmabuf);
			return -1;
		} else {
			//vpu_prinfo("get fences ok, excl  %p, shared  %p, shared count %d\n", fence_excl, fence_shared[0], shared_count);
		}

#if (DRM_VERSION < KERNEL_VERSION(5, 19, 0))
		if (fence_excl) {
			dma_fence_put(fence_excl);
			//vpu_prinfo("efence refcont: %d\n", kref_read(&fence_excl->refcount));
		}
#endif

		for (i=0; i< shared_count; i++) {
			dma_fence_signal(fence_shared[i]);
			dma_fence_put(fence_shared[i]);
		}
		if (fence_shared)
			kfree(fence_shared);
	}
	dma_buf_put(dmabuf);

	return ret;
}
#endif

/******************************************************************************
@函数名字	  vpu_export_dmabuf
@函数功能	  create vpu dmabuf

@输入参数	  ctx	vpu driver ctx
@输入参数	  obj	vpu buffer object
@返回值	  int	dmabuf fd
******************************************************************************/
int vpu_export_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj)
{
	int ret;
	int fd;
	vpudrv_buffer_t *pvb = NULL;
	struct dma_buf *dmabuf = NULL;
	vpu_buf_obj_t *dmabuf_obj = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (!ctx || !obj) {
		vpu_prerr("%s invalid argument\n", __func__);
		return -EINVAL;
	}
	pvb = &(obj->vb);
	ret = vpu_alloc_dma_buffer(pvb, ctx);
	if (ret < 0)
		return -ENOMEM;

	obj->ctx = ctx;
	dmabuf_obj = kzalloc(sizeof(vpu_buf_obj_t), GFP_KERNEL);
	memcpy(dmabuf_obj, obj, sizeof(vpu_buf_obj_t));

	exp_info.ops = &vpu_dmabuf_ops;
	exp_info.size = pvb->size;
	exp_info.flags = O_RDWR;//|O_CLOEXEC;
	exp_info.priv = dmabuf_obj;

	dmabuf = dma_buf_export(&exp_info);
	if (fh2m_inno_is_err(dmabuf)) {
		vpu_error(ctx->vpudev, "dma_buf_export failed\n");
		vpu_free_dma_buffer(pvb, ctx);
		return -ENOMEM;
	}

	fd = dma_buf_fd(dmabuf, O_RDWR);
	if(fd < 0) {
		dma_buf_put(dmabuf);
		vpu_error(ctx->vpudev, "dma_buf_fd failed\n");
		return -ENOMEM;
	}
	pvb->fd =fd;
	obj->buf = dma_buf_get(fd);
	dmabuf_obj->buf = dmabuf;
	dmabuf_obj->vb.fd = fd;
	vpu_dbg(ctx->vpudev, "vpu_export_dmabuftach dmabuf :%p create\n", dmabuf);
	vpu_dbg(ctx->vpudev, " phys_addr:0x%llx fd:%d\n", pvb->phys_addr, pvb->fd);
	return fd;
}

/**
 * @brief get vpu buffer from dmabuf fd
 * why not get vb in vpu buffer pool, because may import in other vpu or not import, vb not in pool
 *
 * @param[in] filp vpu filp
 * @param[in] fd dmabuf fd
 * @param[out] vb vpu buffer
 * @return int 0:success
 */
int vpu_dmafd_to_vb (vpu_drv_ctxs *vpu_drv_ctx, int fd, vpudrv_buffer_t *vb) {
	struct dma_buf *dma_buf;

	if (vpu_drv_ctx == NULL || vb == NULL) {
		vpu_prerr("Invalid argument\n");
		return -EINVAL;
	}
	vb->fd = fd;

	dma_buf = dma_buf_get(fd);
	if (fh2m_inno_is_err_or_null(dma_buf)) {
		vpu_error(vpu_drv_ctx->drv_context.vpudev, "get dmabuf fail\n");
		return -EADDRNOTAVAIL;
	}

	if (dma_buf->priv == NULL) {
		vpu_error(vpu_drv_ctx->drv_context.vpudev, "buf->priv null\n");
		dma_buf_put(dma_buf);
		return -EADDRNOTAVAIL;
	}

	if (dma_buf->ops == &vpu_dmabuf_ops) { // vpu dmabuf
		vpu_buf_obj_t *obj = (vpu_buf_obj_t *)dma_buf->priv;
		memcpy(vb, &(obj->vb), sizeof(vpudrv_buffer_t));
	}
#if !defined(VPU_NO_DISPLAY) && !defined(ANDROID)
	else if (dma_buf->ops == fh2m_innodpu_gem_get_dma_buf_ops()) {
		struct drm_gem_object *drm_obj = dma_buf->priv;
		vb->dev_addr = fh2m_innodpu_gem_get_dev_paddr(drm_obj);
		vb->phys_addr = fh2m_innodpu_gem_get_cpu_paddr(drm_obj);
		vb->size = fh2m_innodpu_gem_get_size_origin(drm_obj);

		if (vb->dev_addr > vpu_drv_ctx->drv_context.chip_info.vram_dev_end_addr) {
			/*domain maybe unknown.phys_addr 0 say it's invisible vram*/
			/*phys_addr same as vpu_alloc_dma_buffer*/
			vb->phys_addr = vb->dev_addr + vpu_drv_ctx->drv_context.chip_info.host_bar2_end_addr;
		}

		vb->base = vb->phys_addr;
		vb->domain = INNO_VPU_GEM_DOMAIN_VRAM;
	}
#endif
	else {
		vpu_error(vpu_drv_ctx->drv_context.vpudev, "unsupport dmabuf type\n");
		dma_buf_put(dma_buf);
		return -EINVAL;
	}


	dma_buf_put(dma_buf);

	return 0;
}

/******************************************************************************
@函数名字     vpu_import_dmabuf
@函数功能     import dmabuf from other module to vpu

@输入参数	  ctx  vpu driver ctx
@输入参数	  obj  vpu buffer object
@返回值      vpudrv_buffer_t	vpu buffer
******************************************************************************/
vpudrv_buffer_t *vpu_import_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj)
{
	int ret = 0;
	struct device *dev;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	vpudrv_buffer_t *vb = NULL;
	vpudrv_buffer_t *pvb;
	struct dma_buf *dma_buf;

	if (!ctx || !obj) {
		vpu_prerr("%s invalid argument\n", __func__);
		return NULL;
	}

	if (!(ctx->dev)) {
		vpu_prerr("%s ctx platform device NULL\n", __func__);
		return NULL;
	}

	dev = ctx->dev;
	pvb= &(obj->vb);
	dma_buf = dma_buf_get(pvb->fd);
	if (fh2m_inno_is_err_or_null(dma_buf)) {
		vpu_prerr("%s get dmabuf fail\n", __func__);
		return NULL;
	}
	obj->buf = dma_buf;
	if (dma_buf->ops == &vpu_dmabuf_ops) {
		/*
		 * The dmabuf is one of ours, so return the associated, rather than create a new one.
		 */
		vb = vpu_dmabuf_to_vpubuf(dma_buf);
		if (!vb)  {
			vpu_prerr("%s vpu_dmabuf_to_vpubuf fail\n", __func__);
		}
	} else {
		//import dmabuf of others
		attach = dma_buf_attach(dma_buf, dev);
		if (fh2m_inno_is_err(attach))
			return ERR_CAST(attach);

		///get_dma_buf(dma_buf);

		sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (fh2m_inno_is_err(sgt)) {
			ret = PTR_ERR(sgt);
			goto fail_detach;
		}

		//only one block;
		if (sgt->nents <= 0) {
			ret = -EINVAL;
			goto fail_ummap;
		}

		obj->ctx = ctx;
		obj->import_attach = attach;
		obj->sgt = sgt;
		vb = &(obj->vb);
		if (sgt->nents == 1) {
			vb->dev_addr = sg_dma_address(sgt->sgl);
			if (vb->dev_addr > ctx->chip_info.vram_dev_end_addr) {
				/*domain maybe unknown.phys_addr 0 say it's invisible vram*/
				/*phys_addr same as vpu_alloc_dma_buffer*/
				vb->phys_addr = vb->dev_addr + ctx->chip_info.host_bar2_end_addr;
				vb->domain = INNO_VPU_GEM_DOMAIN_VRAM;
			} else {
				vb->phys_addr = fh2m_dev_paddr_to_cpu_paddr(dev->parent, vb->dev_addr);
				vb->domain = INNO_VPU_GEM_DOMAIN_CPU;
			}
			vb->base = vb->phys_addr;
			vb->size = attach->dmabuf->size;
#if !defined(VPU_NO_DISPLAY) && !defined(ANDROID)
			if (dma_buf->ops == fh2m_innodpu_gem_get_dma_buf_ops()) {
				vb->size = fh2m_innodpu_gem_get_size_origin(dma_buf->priv);
			}
#endif
			vpu_dbg(ctx->vpudev, "import_attach: %p, sgt: %p, devaddr:0x%llx, phys_addr:0x%llx improt:%p \n",
				obj->import_attach, obj->sgt, vb->dev_addr, vb->phys_addr, dma_buf);
		} else {
			vb->domain = INNO_VPU_GEM_DOMAIN_DISCRETE;
		}
		return vb;
	}

	return vb;
fail_ummap:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}

#if 0
/******************************************************************************
@函数名字 vpu_destroy_dmabuf
@函数功能 destroy dmabuf

@输入参数	  ctx	vpu driver ctx
@输入参数	  fd	dmabuf fd
@返回值      void
******************************************************************************/
int vpu_destroy_dmabuf(vpu_drv_context *ctx, int fd)
{
	int ret = 0;

	struct dma_buf *dma_buf = dma_buf_get(fd);
	if (fh2m_inno_is_err_or_null(dma_buf)) {
		vpu_prinfo("bad fd: %ld\n", PTR_ERR(dma_buf));
		return -1;
	}

	/**force to release*/
	dma_buf->cb_shared.active = 0;
	dma_buf->cb_excl.active = 0;
	dma_buf_put(dma_buf);
	if (dma_buf->ops == &vpu_dmabuf_ops) {
		;//dma_buf_put(dma_buf);
	} else {
		vpu_buf_obj_t *obj;
		vpudrv_buffer_pool_t *pool, *n;
		vpudrv_buffer_t *pvb;

		list_for_each_entry_safe(pool, n, &ctx->vbp_head, list)
		{
			pvb = &(pool->buf.vb);
			if (pvb->fd == fd) {
				obj = to_vpu_buf_obj(pvb);

				if (obj->sgt) {
					vpu_dbg(ctx->vpu_mscdev.this_device, "import_attach: %p , sgt: %p", obj->import_attach, obj->sgt);
					dma_buf_unmap_attachment(obj->import_attach, obj->sgt, DMA_BIDIRECTIONAL);
				}
				if (obj->import_attach)
					dma_buf_detach(dma_buf, obj->import_attach);

				dma_buf_put(dma_buf);
				break;
			}
		}
	}
	vpu_dbg(ctx->vpu_mscdev.this_device, "dmabuf :%d destroyed", fd);

	return ret;
}
#endif

static void release_pending_signals(struct dma_buf *dmabuf)
{
	int ret, i;
#if (DRM_VERSION < KERNEL_VERSION(5, 19, 0))
	u32 shared_count;
	struct dma_fence **fence_shared;
	struct dma_fence *fence_excl;
#else
	int j;
	enum dma_resv_usage usage;
	u32 num_fences;
	struct dma_fence **fences;
#endif

	if (fh2m_inno_is_err_or_null(dmabuf)) {
		vpu_prinfo("%s invalid dmabuf\n", __func__);
		return;
	}

#if (DRM_VERSION <= KERNEL_VERSION(5, 15, 2))
	if ((dmabuf->cb_shared.active == 0) && (dmabuf->cb_excl.active == 0))
#else
		if ((dmabuf->cb_out.active == 0) && (dmabuf->cb_in.active == 0))
#endif
			return;

#if (DRM_VERSION < KERNEL_VERSION(5, 19, 0))
	ret = dma_resv_get_fences(dmabuf->resv, &fence_excl, &shared_count, &fence_shared);
	if (ret) {
		vpu_prerr("%s get fences error\n", __func__);
		return;
	}

	if (fence_excl) {
		dma_fence_signal(fence_excl);
		dma_fence_put(fence_excl);
	}

	for (i=0; i< shared_count; i++) {
		dma_fence_signal(fence_shared[i]);
		dma_fence_put(fence_shared[i]);
	}
	if (fence_shared)
		kfree(fence_shared);
#else
	usage = DMA_RESV_USAGE_WRITE;
	for (j=0; j < 2; j++) {
		ret = dma_resv_get_fences(dmabuf->resv, usage, &num_fences, &fences);
		if (ret) {
			vpu_prerr("%s get fences error\n", __func__);
			return;
		}

		for (i=0; i < num_fences; i++) {
			dma_fence_signal(fences[i]);
			dma_fence_put(fences[i]);
		}
		if (fences)
			kfree(fences);

		usage = DMA_RESV_USAGE_READ;
	}
#endif
}

/******************************************************************************
@函数名字 vpu_destroy_dmabuf
@函数功能 destroy dmabuf

@输入参数	  ctx	vpu driver ctx
@输入参数	  struct dma_buf *buf
@返回值      void
******************************************************************************/
void vpu_destroy_dmabuf(vpu_drv_context *ctx, vpu_buf_obj_t *obj)
{
	struct dma_buf *dma_buf;
	if (fh2m_inno_is_err_or_null(obj)) {
		vpu_prinfo("%s invalid buf obj\n", __func__);
		return;
	}

	dma_buf = obj->buf;
	if (fh2m_inno_is_err_or_null(dma_buf)) {
		vpu_prerr("%s get dmabuf fail\n", __func__);
		return;
	}

	vpu_dbg(ctx->vpudev, "enter vpu_destroy_dmabuf\n");
	/**release pending signals*/
	release_pending_signals(dma_buf);

	if (obj->buf_type == BUF_TYPE_VPU_IMPORT) {
		atomic64_sub(obj->vb.size, &(ctx->external_mem));
	}

	if (dma_buf->ops == &vpu_dmabuf_ops) {
		dma_buf_put(dma_buf);
	} else {
		if (obj->sgt) {
			dma_buf_unmap_attachment(obj->import_attach, obj->sgt, DMA_BIDIRECTIONAL);
			obj->sgt = NULL;
		}

		if (obj->import_attach) {
			dma_buf_detach(dma_buf, obj->import_attach);
			obj->import_attach = NULL;
		}

		dma_buf_put(dma_buf);
		vpu_dbg(ctx->vpudev, "dmabuf :%p destroyed\n", dma_buf);
	}

	if(obj->map_addr) {
		fh2m_inno_iounmap((void *)(obj->map_addr));
		obj->map_addr = 0;
	}
	vpu_dbg(ctx->vpudev, "exit vpu_destroy_dmabuf\n");
}

int vpu_dma_fence_wait(int fd, struct dma_buf *dma_buf_in, struct dma_fence *dma_fence_out)
{
	int ret, i;
	struct dma_buf *dmabuf = NULL;
#if (DRM_VERSION < KERNEL_VERSION(5, 19, 0))
	u32 shared_count;
	struct dma_fence **fence_shared;
	struct dma_fence *fence_excl = NULL;
#else
	int j;
	enum dma_resv_usage usage;
	u32 num_fences;
	struct dma_fence **fences;
#endif

	if (dma_buf_in) {
		dmabuf = dma_buf_in;
		get_dma_buf(dmabuf);
	} else {
		dmabuf = dma_buf_get(fd);
	}
	if (fh2m_inno_is_err_or_null(dmabuf)) {
		vpu_prerr("%s get dmabuf fail\n", __func__);
		return -1;
	}

#if (DRM_VERSION <= KERNEL_VERSION(5, 15, 2))
	if ((dmabuf->cb_shared.active == 0) && (dmabuf->cb_excl.active == 0)) {
#else
	if ((dmabuf->cb_out.active == 0) && (dmabuf->cb_in.active == 0)) {
#endif
		dma_buf_put(dmabuf);
		return 0;
	}

#if (DRM_VERSION < KERNEL_VERSION(5, 19, 0))
	ret = dma_resv_get_fences(dmabuf->resv, &fence_excl, &shared_count, &fence_shared);
	if (ret) {
		vpu_prerr("%s get fences error\n", __func__);
		dma_buf_put(dmabuf);
		return -1;
	}

	if (fence_excl && fence_excl != dma_fence_out) {
		dma_fence_wait_timeout(fence_excl, false, msecs_to_jiffies(100));
		dma_fence_put(fence_excl);
	}

	for (i=0; i< shared_count; i++) {
		if(fence_shared[i] && fence_shared[i] != dma_fence_out) {
			dma_fence_wait_timeout(fence_shared[i], false, msecs_to_jiffies(100));
			dma_fence_put(fence_shared[i]);
		}
		if(fence_shared[i] == dma_fence_out) {//the subsequent fence will not wait
			break;
		}
	}

#else
	usage = DMA_RESV_USAGE_WRITE;
	for (j=0; j < 2; j++) {
		ret = dma_resv_get_fences(dmabuf->resv, usage, &num_fences, &fences);
		if (ret) {
			vpu_prerr("%s get fences error\n", __func__);
			return -1;
		}

		for (i=0; i < num_fences; i++) {
			if(fences[i] && fences[i] != dma_fence_out) {
				dma_fence_wait_timeout(fences[i], false, msecs_to_jiffies(100));
				dma_fence_put(fences[i]);
			}
		}

		usage = DMA_RESV_USAGE_READ;
	}
#endif
	dma_buf_put(dmabuf);
	return 0;
}
