/*************************************************************************/ /*!
@File			innodpu_drm_gem.c
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
#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_drv.h>
#include <drm/drm_prime.h>
#include <linux/platform_device.h>
#endif

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/capability.h>
#include <drm/drm_mm.h>
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#include "innogpu_pci_drv.h"
#include "innogpu.h"
#include "osfunc_common.h"
#include "innodpu_drm_gem.h"
#include "pdp_drm.h"
#include "kernel_compatibility.h"
#include "innogpu_drm.h"
#include "pvrsrv_memallocflags.h"
#include "inno_drm.h"
#include "kernel_compat.h"

struct innodpu_shmem_allowlist {
	const char *name;
	bool full_match;
};

static __maybe_unused struct innodpu_shmem_allowlist s_shmem_allowlist[] =
{
	{"Xorg", 1},
	{"X", 1},
	{"kwin_x11", 0},
	{"browse", 0},
	{"cdos-desktop", 1},
	{"chrome", 1},
	{"firefox", 0},
	{"macro", 1},
	{"gnome-shell", 1},
};

bool innodpu_gem_check_memory_shared(struct drm_device *drm_dev,
	struct innodpu_drm_private *dpu_priv, uint32_t flag)
{
	bool visible = !((flag & DBM_GEM_INVISIBLE) >> 28);
	uint64_t name_len = fh2m_inno_strlen(current->comm);
	int i = 0;

	/* this use for test nocontinuous gem no merge this */
	if (s_dpu_not_use_shared_mem) {
		return false;
	}

	if (!dpu_priv->has_shared_mem) {
		return false;
	}

	if (dpu_priv->shared_vram_info.is_visible != visible) {
		return false;
	}

	if (flag & DBM_GEM_GTT) {
		return false;
	}

	for (i = 0; i < INNO_ARRAY_SIZE(s_shmem_allowlist); ++i) {
		if (s_shmem_allowlist[i].full_match) {
			if (fh2m_inno_strncmp(current->comm, s_shmem_allowlist[i].name, name_len) == 0)
				return true;
		} else {
			if (fh2m_inno_strstr(current->comm, s_shmem_allowlist[i].name))
				return true;
		}
	}

	return false;
}

int innodpu_multiuser_add(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file)
{
	int ret = 0;
	int retry = 3;

	innodpu_shared_mem *pshared_mem = &dev_priv->shared_vram_info;
	innodpu_shared_mem_user *puser = NULL, *tmp_user = NULL;
	struct drm_pdp_user_set *puser_add = data;

	if(!dev_priv->has_shared_mem) {
		fh2m_innodpu_err(drm_dev->dev, "innodpu not support multi-user.\n");
		return -EINVAL;
	}

	spin_lock(&pshared_mem->user_idr_lock);
	tmp_user = idr_find(&pshared_mem->uer_idr, puser_add->user_id);
	spin_unlock(&pshared_mem->user_idr_lock);
	if (tmp_user) {
		inno_drm_info(drm_dev->dev, "innodpu user-id %lld already exits.\n", puser_add->user_id);
		return 0;
	}

	puser = devm_kmalloc(drm_dev->dev, sizeof(*puser), fh2m_hal_get_inno_gfp_kernel());
	if (!puser) {
		fh2m_innodpu_err(drm_dev->dev, "innodpu alloc user_info failed, short of memory.\n");
		return -ENOMEM;
	}
	memset(puser, 0, sizeof(*puser));

	puser->mem_manager = innodpu_mem_manager_init(drm_dev, pshared_mem->is_visible, SHMEM_VRAM_POSITION, pshared_mem);
	puser->user_id = puser_add->user_id;

	do {
		spin_lock(&pshared_mem->user_idr_lock);
		ret = idr_alloc(&pshared_mem->uer_idr, puser, puser_add->user_id, puser_add->user_id + 1, GFP_ATOMIC);
		spin_unlock(&pshared_mem->user_idr_lock);
		if (ret == puser_add->user_id)
			break;
	} while (--retry > 0);
	if (retry <=0)
		fh2m_innodpu_err(drm_dev->dev, "innodpu push user(%p)-%lld error.\n", puser, puser->user_id);

	inno_drm_info(drm_dev->dev, "innodpu push user(%p)-%lld.\n", puser, puser->user_id);

	return ret;
}

int innodpu_multiuser_set(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file)
{
	innodpu_shared_mem * pshared_mem = &dev_priv->shared_vram_info;
	innodpu_shared_mem_user *puser = NULL;
	struct drm_pdp_user_set *puser_set = data;

	spin_lock(&pshared_mem->user_idr_lock);
	puser = idr_find(&pshared_mem->uer_idr, puser_set->user_id);
	spin_unlock(&pshared_mem->user_idr_lock);
	if (!puser) {
		fh2m_innodpu_err(drm_dev->dev,"innodpu can't switch user-%lld, need to set.\n", puser_set->user_id);
		return -ENODEV;
	}
	pshared_mem->current_user = puser;

	inno_drm_info(drm_dev->dev, "innodpu switch user(%p)-%ld.\n", puser, puser->user_id);
	return 0;
}

int innodpu_multiuser_del(struct drm_device *drm_dev,
	struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file)
{
	innodpu_shared_mem *pshared_mem = &dev_priv->shared_vram_info;
	innodpu_shared_mem_user *puser = NULL;
	struct drm_pdp_user_del *puser_del = data;

	spin_lock(&pshared_mem->user_idr_lock);
	puser = idr_find(&pshared_mem->uer_idr, puser_del->user_id);
	spin_unlock(&pshared_mem->user_idr_lock);
	if (!puser) {
		fh2m_innodpu_err(drm_dev->dev,"innodpu can't del user-%lld, need to set.\n", puser_del->user_id);
		return -ENODEV;
	}

	spin_lock(&pshared_mem->user_idr_lock);
	idr_remove(&pshared_mem->uer_idr, puser_del->user_id);
	spin_unlock(&pshared_mem->user_idr_lock);

	innodpu_mem_manager_fini(drm_dev, puser->mem_manager);

	if (puser == pshared_mem->current_user)
		pshared_mem->current_user = NULL;

	devm_kfree(drm_dev->dev, puser);

	inno_drm_info(drm_dev->dev, "innodpu del user(%p)-%ld.\n", puser, puser->user_id);
	return 0;
}

void innodpu_xorg_monitor_switch_user(struct drm_device *drm_dev,
	innodpu_shared_mem *pshared_mem)
{
	int tgid = current->tgid;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "innodpu dev_priv is NULL.\n");
		return;
	}

	if ((!pshared_mem->current_user) || (pshared_mem->current_user->user_id != tgid)) {
		struct drm_pdp_user_add user_add = {.user_id=tgid, .flags=0};
		struct drm_pdp_user_set user_set = {.user_id=tgid, .flags=0};

		/* clear all sharemem */
		{
			innodpu_shared_mem *pshared_mem = &dev_priv->shared_vram_info;

			unsigned int vm_size = 0, vm_offset = 0;
			unsigned long vram_zero_size = dev_priv->zero_gem->info.size;
			unsigned long vram_share_size = pshared_mem->size;
			void * axi_vram_src = (void *)dev_priv->zero_gem->info.dev_paddr;
			void *axi_vram_dst = NULL;

			for (vm_offset = 0; vm_offset < vram_share_size; vm_offset += vm_size) {
				vm_size = ((vram_share_size - vm_offset) < vram_zero_size) ?
					(vram_share_size - vm_offset) : vram_zero_size;
				axi_vram_dst = (void *)pshared_mem->dev_paddr + vm_offset;
				innodpu_dma_memcpy(drm_dev->dev, &axi_vram_src, &axi_vram_dst, &vm_size, 1, GDDR2GDDR);
			}
		}

		if (innodpu_multiuser_add(drm_dev, dev_priv, &user_add, NULL) < 0)
			return;

		innodpu_multiuser_set(drm_dev, dev_priv, &user_set, NULL);
	}
}

int innodpu_gem_dbmflags_to_position(struct drm_device *drm_dev,
	struct innodpu_drm_private *dpu_priv, uint32_t flags)
{
	innodpu_mem_positon pos = 0;

	/* check share mem first */
	if (innodpu_gem_check_memory_shared(drm_dev, dpu_priv, flags)) {
		pos = SHMEM_VRAM_POSITION;
		goto exit;
	}

	if (flags & DBM_GEM_GTT) {
		pos = SYS_GTT_POSITION;
	} else {
		pos = VRAM_POSITION;
	}

exit:

	return pos;
}

int innodpu_get_dmbflags_to_class(uint32_t flags, innodpu_mem_positon pos)
{
	innodpu_mem_class class = 0;

	switch (pos) {

	case SHMEM_VRAM_POSITION:
	case VRAM_POSITION:
		class = CONTINUOUS_VRAM;
		break;

	case SYS_GTT_POSITION:
		class = GTT;
		break;

	default:
		class = -1;
		break;
	}

	return class;
}

static __attribute__ ((unused)) void innodpu_gem_write2file(const char *name, void *addr, int length)
{
	struct file *fp;
	loff_t pos = 0;

	fp = filp_open(name, O_RDWR | O_CREAT | O_APPEND, 0644);
	if (IS_ERR(fp)) {
		DRM_ERROR("create file error\n");
		return;
	}
#if (DRM_VERSION < KERNEL_VERSION(4, 14, 0))
	kernel_write(fp, addr, length, pos);
#else
	kernel_write(fp, addr, length, &pos);
#endif

	filp_close(fp , NULL);
}

/* this func use for clear continuous vram to zero */
static void innodpu_gem_vbuffer_clear(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj, struct device* pdev)
{
	int vm_offset = 0, vm_size = 0;
	void *axi_vram_src = NULL, *axi_vram_dst = NULL;

	/* check zero vram ready or not */
	if (!(mem_manager->zero_gem != NULL && mem_manager->zero_gem->is_ready)) {
		return;
	}

	for (vm_offset = 0; vm_offset < innodpu_obj->base.size; vm_offset += vm_size) {
		vm_size = ((innodpu_obj->base.size - vm_offset) < mem_manager->zero_gem->info.size) ?
			(innodpu_obj->base.size - vm_offset) : mem_manager->zero_gem->info.size;
		axi_vram_src = (void *)mem_manager->zero_gem->info.dev_paddr;
		axi_vram_dst = (void *)innodpu_obj->dev_paddr + vm_offset;

		innodpu_dma_memcpy(pdev, &axi_vram_src, &axi_vram_dst, &vm_size, 1, GDDR2GDDR);
	}

	return;
}

static unsigned int innodpu_gem_merge_nocontinuous_page(innodpu_gem_object *innodpu_obj,
	int *len_array, unsigned int max_block_size)
{
	int i = 0;
	unsigned int block_num = 0;

	*(len_array) = PAGE_SIZE;

	/* max block size if for zero vram clear, cause 0 vram just 1Mbytes */
	if (max_block_size) {
		for (i = 1; i < innodpu_obj->pmr->base_array_size; i++) {
			if ((innodpu_obj->pmr->base_array[i -1] + PAGE_SIZE == innodpu_obj->pmr->base_array[i]) &&
				(*(len_array + block_num) <= max_block_size - PAGE_SIZE)) {
				*(len_array + block_num) += PAGE_SIZE;
			} else {
				block_num++;
				*(len_array + block_num) = PAGE_SIZE;
			}
		}
	} else {
		for (i = 1; i < innodpu_obj->pmr->base_array_size; i++) {
			if ((innodpu_obj->pmr->base_array[i -1] + PAGE_SIZE) == innodpu_obj->pmr->base_array[i]) {
				*(len_array + block_num) += PAGE_SIZE;
			} else {
				block_num++;
				*(len_array + block_num) = PAGE_SIZE;
			}
		}
	}

	block_num++;

	return block_num;
}

/* this func use for clear nocontinuous vram to zero */
static __maybe_unused void innodpu_gem_vbuffer_clear_nocontinuous(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj, struct device* pdev)
{

	if (!(mem_manager->zero_gem != NULL && mem_manager->zero_gem->is_ready)) {
		return;
	}

	if (innodpu_obj->pmr->base_array_size == 1) {
		int vm_offset = 0, vm_size = 0;
		void *axi_vram_src = NULL, *axi_vram_dst = NULL;

		for (vm_offset = 0; vm_offset < innodpu_obj->base.size; vm_offset += vm_size) {
			vm_size = ((innodpu_obj->base.size - vm_offset) < mem_manager->zero_gem->info.size) ?
				(innodpu_obj->base.size - vm_offset) : mem_manager->zero_gem->info.size;
			axi_vram_src = (void *)mem_manager->zero_gem->info.dev_paddr;
			axi_vram_dst = (void *)innodpu_obj->pmr->base_array[0] + vm_offset;

			innodpu_dma_memcpy(pdev, &axi_vram_src, &axi_vram_dst, &vm_size, 1, GDDR2GDDR);
		}
	} else {
		void **src_array = NULL, **dst_array = NULL;
		int *len_array = NULL;
		int num = 0, i = 0;
		unsigned int vram_offset = 0;

		src_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		dst_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		len_array = fh2m_inno_vmalloc(sizeof(int) * innodpu_obj->pmr->base_array_size);

		if (src_array && dst_array && len_array) {
			num = innodpu_gem_merge_nocontinuous_page(innodpu_obj,
				len_array, mem_manager->zero_gem->info.size);
			if (num < 1 || num > innodpu_obj->pmr->base_array_size) {
				gem_err(innodpu_obj->base.dev->dev, "nocontinuous page merge err.\n");
			} else {
				for (i = 0; i < num; i++) {
					*(src_array + i) = (void *)innodpu_obj->mem_manager->zero_gem->info.dev_paddr;
					*(dst_array + i) = (void *)innodpu_obj->pmr->base_array[vram_offset / PAGE_SIZE];
					vram_offset += *(len_array + i);
				}
				innodpu_dma_memcpy(pdev, src_array, dst_array, len_array, num, GDDR2GDDR);
			}
		}

		if (src_array)
			fh2m_inno_vfree(src_array);
		if (dst_array)
			fh2m_inno_vfree(dst_array);
		if (len_array)
			fh2m_inno_vfree(len_array);
	}
}

/* this func use for alloc innodpu_obj memory and init gem_obj */
static innodpu_gem_object *innodpu_gem_private_object_init(
		struct drm_device *drm_dev, size_t size, struct dma_resv *resv)
{
	innodpu_gem_object *innodpu_obj = NULL;
	innodpu_obj = kzalloc(sizeof(*innodpu_obj), fh2m_hal_get_inno_gfp_kernel());
	if (!innodpu_obj)
		return NULL;
	innodpu_obj->dfd = 0;
	INIT_LIST_HEAD(&innodpu_obj->vm_head);
	mutex_init(&innodpu_obj->vm_lock);

#if (DRM_VERSION < KERNEL_VERSION(5, 2, 0))
	if (!resv)
		dma_resv_init(&innodpu_obj->_resv);
#else
	innodpu_obj->base.resv = resv;
#endif
	drm_gem_private_object_init(drm_dev, &innodpu_obj->base, size);

	return innodpu_obj;
}

static void innodpu_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = NULL;
	innodpu_mem_manager *mem_manager = NULL;

	if (!obj || !obj->dev) {
		return;
	}

	innodpu_obj = to_innodpu_obj(obj);
	if (!innodpu_obj) {
		return;
	}

	mem_manager = innodpu_obj->mem_manager;
	if (!mem_manager) {
		return;
	}

	if (mem_manager->vm_open) {
		mem_manager->vm_open(vma);
	}

	drm_gem_object_get(obj);

	return;
}

static void innodpu_gem_wdma_sync_pvm(innodpu_gem_object *innodpu_obj,
	struct gem_vm_list *pvm)
{
	struct drm_gem_object *obj = &innodpu_obj->base;
	void *src = NULL, *dst = NULL;
	int len;

	src = pvm->vaddr;
	len = pvm->size;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		dst = (void *)innodpu_obj->dev_paddr + pvm->offset;
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		if (innodpu_obj->pmr->base_array_size == 1) {
			dst = (void *)innodpu_obj->pmr->base_array[0] + pvm->offset;
		} else {
			dst = (void *)innodpu_obj->pmr->base_array[pvm->offset / PAGE_SIZE];
		}
	}

	list_del(&pvm->list);
#ifdef GEM_USED_VMALLOC
	if (pvm->cpu_write)
		innodpu_dma_memcpy_for_smallbar_sg(obj->dev->dev, &src, &dst, &len, 1, SYS2GDDR);
	fh2m_inno_vfree(pvm->vaddr);
#else
	if (pvm->cpu_write)
		innodpu_dma_memcpy_for_smallbar(obj->dev->dev, &src, &dst, &len, 1, SYS2GDDR);
	fh2m_inno_kfree(pvm->vaddr);
#endif

	fh2m_inno_kfree(pvm);

	return;
}

static void innodpu_gem_invisible_vram_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	struct gem_vm_list *pvm = NULL, *tmpn = NULL;

	mutex_lock(&innodpu_obj->vm_lock);
	list_for_each_entry_safe(pvm, tmpn, &innodpu_obj->vm_head, list) {
		innodpu_gem_wdma_sync_pvm(innodpu_obj, pvm);
	}
	mutex_unlock(&innodpu_obj->vm_lock);

	return;
}

static void innodpu_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = NULL;
	innodpu_mem_manager *mem_manager = NULL;

	if (!obj || !obj->dev) {
		return;
	}

	innodpu_obj = to_innodpu_obj(obj);
	if (!innodpu_obj) {
		return;
	}

	mem_manager = innodpu_obj->mem_manager;
	if (!mem_manager) {
		return;
	}

	if (mem_manager->vm_close) {
		mem_manager->vm_close(vma);
	}

	drm_gem_object_put(obj);
}

static int innodpu_gem_invisible_vram_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);

	struct gem_vm_list *pvm = NULL;
	struct page *page = NULL;
	void *axi_vram_src = NULL, *dst = NULL;

	unsigned long pfn = 0, offset = 0;
	unsigned long vmf_address = 0;
	int ret = 0;

#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
	vmf_address = (unsigned long)vmf->virtual_address;
#else
	vmf_address = vmf->address;
#endif
	offset = vmf_address - vma->vm_start;
	pvm = fh2m_inno_kzalloc_kernel(sizeof(*pvm));
	if (!pvm) {
		gem_err(obj->dev->dev, "VM Fault alloc memory failed.\n");
		ret = -ENOMEM;
		goto err;
	}
	pvm->dev_paddr  = innodpu_obj->dev_paddr + offset;
	pvm->offset = offset;
	pvm->size = PAGE_SIZE;
#ifdef GEM_USED_VMALLOC
	pvm->vaddr = fh2m_inno_vmalloc(pvm->size);
#else
	pvm->vaddr = fh2m_inno_kzalloc_kernel(pvm->size);
#endif
	if (!pvm->vaddr) {
		gem_err(obj->dev->dev, "vmalloc Fault alloc memory failed.\n");
		ret = -ENOMEM;
		goto err;
	}

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		axi_vram_src = (void *)innodpu_obj->dev_paddr + pvm->offset;
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		if (innodpu_obj->pmr->base_array_size == 1) {
			axi_vram_src = (void *)innodpu_obj->pmr->base_array[0] + pvm->offset;
		} else {
			axi_vram_src = (void *)innodpu_obj->pmr->base_array[offset / PAGE_SIZE];
		}
	}
	dst = (void *)pvm->vaddr;

#ifdef GEM_USED_VMALLOC
	innodpu_dma_memcpy_for_smallbar_sg(obj->dev->dev, &axi_vram_src, &dst, &pvm->size, 1, GDDR2SYS);
	page = vmalloc_to_page(pvm->vaddr);
#else
	innodpu_dma_memcpy_for_smallbar(obj->dev->dev, &axi_vram_src, &dst, &pvm->size, 1, GDDR2SYS);
	page = virt_to_page(pvm->vaddr);
#endif
	pfn = page_to_pfn(page);
	ret = vmf_insert_pfn(vma, vmf_address, pfn);

	INIT_LIST_HEAD(&pvm->list);
	mutex_lock(&innodpu_obj->vm_lock);
	pvm->cpu_write = !innodpu_obj->cpu_prep ||
		innodpu_obj->cpu_prep_write;
	list_add_tail(&pvm->list, &innodpu_obj->vm_head);
	mutex_unlock(&innodpu_obj->vm_lock);

	return ret;

err:
	if (pvm) {
		if (pvm->vaddr) {
			#ifdef GEM_USED_VMALLOC
			fh2m_inno_vfree(pvm->vaddr);
			#else
			fh2m_inno_kfree(pvm->vaddr);
			#endif
		}
		fh2m_inno_kfree(pvm);
	}

	return ret;
}

static int innodpu_gem_visible_vram_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	unsigned long pfn = 0, paddr = 0, offset = 0;
	unsigned long vmf_address = 0;
	int err = 0;

#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
	vmf_address = (unsigned long)vmf->virtual_address;
#else
	vmf_address = vmf->address;
#endif
	offset = vmf_address - vma->vm_start;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		paddr = innodpu_obj->cpu_paddr + offset;
		pfn = paddr >> PAGE_SHIFT;
		err = vmf_insert_pfn(vma, vmf_address, pfn);
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		if (innodpu_obj->pmr->base_array_size == 1) {
			/* this mean nocontinuous vram actullay continuous */
			paddr = fh2m_dev_paddr_to_cpu_paddr(obj->dev->dev, innodpu_obj->pmr->base_array[0] + offset);
			pfn = paddr >> PAGE_SHIFT;
			err = vmf_insert_pfn(vma, vmf_address, pfn);
		} else {
			/* this mean nocontinuous vram really uncontinuous, base_array is an page array */
			paddr = fh2m_dev_paddr_to_cpu_paddr(obj->dev->dev, innodpu_obj->pmr->base_array[offset / PAGE_SIZE]);
			pfn = paddr >> PAGE_SHIFT;
			err  = vmf_insert_pfn(vma, vmf_address, pfn);
		}
	}

	return err;
}

static int innodpu_gem_gtt_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page = NULL;
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	unsigned long pfn = 0, offset = 0;
	unsigned long vmf_address = 0;
	int err = 0;

	if (innodpu_obj->gtt == NULL) {
		gem_err(obj->dev->dev, "gtt data err\n");
		return -ENOMEM;
	}

#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
	vmf_address = (unsigned long)vmf->virtual_address;
#else
	vmf_address = vmf->address;
#endif
	offset = vmf_address - vma->vm_start;

	if (offset > innodpu_obj->gtt->size) {
		gem_err(obj->dev->dev, "vm fault offset more than gtt obj size\n");
		return -ENOMEM;
	}

	page = innodpu_obj->gtt->page_array[offset / PAGE_SIZE];
	pfn = page_to_pfn(page);
	err  = vmf_insert_pfn(vma, vmf_address, pfn);

	return err;
}

#if (DRM_VERSION < KERNEL_VERSION(4, 11, 0))
static vm_fault_t innodpu_gem_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
#else
static vm_fault_t innodpu_gem_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#endif
	struct drm_gem_object *obj = vma->vm_private_data;
	innodpu_gem_object *innodpu_obj = NULL;
	innodpu_mem_manager *mem_manager = NULL;
	int err = 0;

	if (!obj || !obj->dev) {
		return VM_FAULT_NOPAGE;
	}

	innodpu_obj = to_innodpu_obj(obj);
	if (!innodpu_obj) {
		return VM_FAULT_NOPAGE;
	}

	mem_manager = innodpu_obj->mem_manager;
	if (!mem_manager) {
		return VM_FAULT_NOPAGE;
	}

	/* make sure vmfualt callback must valid */
	if (mem_manager->vm_fault) {
		err = mem_manager->vm_fault(vma, vmf);
	}

#if (DRM_VERSION >= KERNEL_VERSION(4, 20, 0))
	return err;
#endif

	switch (err) {
	case 0:
	case -EBUSY:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

struct vm_operations_struct innodpu_gem_vm_ops = {
	.open = innodpu_gem_vm_open,
	.close = innodpu_gem_vm_close,
	.fault = innodpu_gem_vm_fault,
};

/* dma buf ops */
static int innodpu_gem_prime_attach(struct dma_buf *dma_buf,
#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
								 struct device *dev,
#endif
								 struct dma_buf_attachment *attach)
{
	struct drm_gem_object *gem_obj = dma_buf->priv;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(gem_obj);

	/* Restrict access to Rogue */
	if (gem_obj && gem_obj->dev && gem_obj->dev->dev) {
		if ((gem_obj->dev->dev != attach->dev->parent) &&
			(gem_obj->dev->dev != attach->dev)) {
			innodpu_obj->class = GTT;
			return 0;
		} else {
			gem_info(gem_obj->dev->dev, "%s(%d) Visible-%d  size-%#llx, paddr-%#llx, daddr-%#llx, innodpu_obj-%pK\n",
				innodpu_obj->name, gem_obj->name, innodpu_obj->mem_manager->visible, innodpu_obj->base.size,
				innodpu_obj->cpu_paddr, innodpu_obj->dev_paddr, innodpu_obj);
		}
	} else {
		WARN_ON(1);
	}

	return 0;
}

static void innodpu_gem_prime_detach(struct dma_buf *dma_buf, struct dma_buf_attachment *attach)
{
	struct drm_gem_object *gem_obj = dma_buf->priv;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(gem_obj);

	/* Restrict access to Rogue */
	if (WARN_ON(!gem_obj->dev->dev) ||
		((gem_obj->dev->dev != attach->dev->parent) &&
		 (gem_obj->dev->dev != attach->dev)))
		return;

	gem_info(gem_obj->dev->dev, "%s(%d) visible-%d  size-%#lx, paddr-%#llx, daddr-%#llx, innodpu_obj-%pK\n",
			innodpu_obj->name, gem_obj->name, innodpu_obj->mem_manager->visible, innodpu_obj->base.size,
			innodpu_obj->cpu_paddr, innodpu_obj->dev_paddr, innodpu_obj);

	return;
}

static struct sg_table *innodpu_gem_prime_map_dma_buf(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	struct drm_gem_object *gem_obj = attach->dmabuf->priv;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(gem_obj);
	innodpu_mem_manager *mem_manager = NULL;
	struct sg_table *sgt = NULL;
	struct scatterlist *sg = NULL;
	int ret = 0, i = 0;
	uint64_t gtt_addr = 0;

	if (innodpu_obj == NULL) {
		return NULL;
	}
	mem_manager = innodpu_obj->mem_manager;

	sgt = fh2m_inno_kmalloc_kernel(sizeof(*sgt));
	if (!sgt) {
		gem_err(gem_obj->dev->dev, "%s %d alloc sgt vram err\n", __func__, __LINE__);
		return NULL;
	}

	if (innodpu_obj->dma_map_export_host_addr) {
		ret = fh2m_os_sg_alloc_table(sgt, 1, fh2m_hal_get_inno_gfp_kernel());
		if (ret)
			goto err_free_sgt;
		sg_dma_address(sgt->sgl) = innodpu_obj->cpu_paddr;
		sg_dma_len(sgt->sgl) = gem_obj->size;
	} else if (innodpu_obj->class == GTT) {
		ret = fh2m_os_sg_alloc_table_from_pages(sgt, (void **)innodpu_obj->gtt->page_array,
		innodpu_obj->gtt->page_num, 0, innodpu_obj->gtt->size, fh2m_hal_get_inno_gfp_kernel());
		if (ret)
			goto err_free_sgt;

		inno_for_each_sg(sgt->sgl, sg, sgt->nents, i) {
			if (strstr(attach->dev->driver->name,"inno")) {
				/* inno note: use attach->dev instead of gem_obj->dev->dev, avoid dual inno gpu card address mapping issue
				 * make sure attach->dev is the current innogpu pci device
				 */
				gtt_addr = fh2m_cpu_paddr_to_gtt_paddr(attach->dev, (uint64_t)fh2m_inno_dma_map_page(attach->dev, sg_page(sg), 0, sg->length));
			} else {
				gtt_addr = (uint64_t)fh2m_inno_dma_map_page(attach->dev, sg_page(sg), 0, sg->length);
			}
			sg_dma_address(sg) = gtt_addr;
			sg_dma_len(sg) = sg->length;
		}
	} else if (innodpu_obj->class == CONTINUOUS_VRAM) {
		ret = fh2m_os_sg_alloc_table(sgt, 1, fh2m_hal_get_inno_gfp_kernel());
		if (ret)
			goto err_free_sgt;
		sg_dma_address(sgt->sgl) = innodpu_obj->dev_paddr;
		sg_dma_len(sgt->sgl) = gem_obj->size;
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		ret = fh2m_os_sg_alloc_table(sgt, innodpu_obj->pmr->base_array_size, fh2m_hal_get_inno_gfp_kernel());
		if (ret)
			goto err_free_sgt;

		if (innodpu_obj->pmr->base_array_size != 1) {
			inno_for_each_sg(sgt->sgl, sg, sgt->nents, i) {
				sg_dma_address(sg) = innodpu_obj->pmr->base_array[i];
				sg_dma_len(sg) = PAGE_SIZE;
			}
		} else {
			sg_dma_address(sgt->sgl) = innodpu_obj->pmr->base_array[0];
			sg_dma_len(sgt->sgl) = innodpu_obj->pmr->actual_size;
		}
	}

	return sgt;

err_free_sgt:
	gem_err(gem_obj->dev->dev, "sg table alloc err\n");
	fh2m_inno_kfree(sgt);
	return NULL;
}

static void innodpu_gem_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
	struct sg_table *sgt, enum dma_data_direction dir)
{
	fh2m_os_sg_free_table(sgt);
	kfree(sgt);
}

static void innodpu_gem_dmabuf_release(struct dma_buf *dma_buf)
{
	drm_gem_dmabuf_release(dma_buf);
}

void innodpu_drm_fix_vma_flags(struct vm_area_struct *vma,
	pgprot_t vm_page_prot, unsigned long vm_flags)
{
	struct drm_gem_object *obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;

	obj = (struct drm_gem_object *)vma->vm_private_data;
	innodpu_obj = to_innodpu_obj(obj);
#if defined(CONFIG_CPU_RK3588) || defined(CONFIG_ARCH_EMEISWORD) \
	|| defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)

	if (!(innodpu_obj->class == GTT) && innodpu_obj->mem_manager->visible) {
		vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
		#if (DRM_VERSION >= KERNEL_VERSION(4, 14, 0))
			/* refer to the implementation of drm_gem_mmap_obj
			 * need add pgprot_decrypted before kernel 4.14
			 */
			vma->vm_page_prot = pgprot_decrypted(vma->vm_page_prot);
		#endif
	}

#endif

#if !defined(CONFIG_X86)
	if (!innodpu_obj->mem_manager->visible || innodpu_obj->class == GTT) {
#if (DRM_VERSION >= KERNEL_VERSION(6, 3, 0))
		vm_flags_clear(vma, VM_IO);
#else
		vma->vm_flags &= ~VM_IO;
#endif
		vma->vm_page_prot = vm_page_prot;
	}
#endif

	return;
}

static int innodpu_drm_gem_obj_mmap(struct drm_gem_object *obj,
	unsigned long obj_size, struct vm_area_struct *vma)
{
	innodpu_gem_object *innodpu_obj = NULL;
	__maybe_unused pgprot_t vm_page_prot = vma->vm_page_prot;
	__maybe_unused unsigned long vm_flags = vma->vm_flags;
	int ret = 0;

	innodpu_obj = to_innodpu_obj(obj);

	ret = drm_gem_mmap_obj(obj, obj->size, vma);

	innodpu_drm_fix_vma_flags(vma, vm_page_prot, vm_flags);

	return ret;
}

static int innodpu_gem_prime_mmap(struct dma_buf *dma_buf,
	struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = dma_buf->priv;
	int err = 0;

	mutex_lock(&obj->dev->struct_mutex);
	err = innodpu_drm_gem_obj_mmap(obj, obj->size, vma);
	mutex_unlock(&obj->dev->struct_mutex);

	return err;
}

#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
static void *innodpu_gem_prime_kmap(struct dma_buf *dma_buf, unsigned long page_num)
{
#ifdef CONFIG_KALLSYMS
	gem_info(NULL, "map called\n");
#endif

	return NULL;
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
static void *innodpu_gem_prime_kmap_atomic(struct dma_buf *dma_buf, unsigned long page_num)
{
	return NULL;
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
#if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
void *innodpu_gem_prime_vmap(struct drm_gem_object *obj)
#else
static void *innodpu_gem_prime_vmap(struct drm_gem_object *obj)
#endif
#elif (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static int innodpu_gem_prime_vmap(struct drm_gem_object *obj, struct dma_buf_map *map)
#else
static int innodpu_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
#endif
{
	void *virtual_addr = NULL;
	pgprot_t prot_writecombine;
	innodpu_gem_object *innodpu_obj =  to_innodpu_obj(obj);

	if (innodpu_obj->class == GTT) {
		fh2m_inno_pgprot_writecombine(fh2m_inno_get_inno_page_kernel(), &prot_writecombine);
		virtual_addr = fh2m_inno_vmap(innodpu_obj->gtt->page_array, obj->size/fh2m_inno_page_size,0,&prot_writecombine);
		if (virtual_addr == NULL) {
			pr_err("[%s:%d]size = %#zx,vmap fail\n",__func__,__LINE__,obj->size);
		}

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
		return virtual_addr;
#else
		if (map) {
			map->vaddr = virtual_addr;
			map->is_iomem = false;
		}

		return 0;
#endif
	} else {
		pr_err("[%s:%d]size = %#zx,class:%#x not support !\n",__func__,__LINE__,obj->size,innodpu_obj->class);
#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
		return NULL;
#else
		return -EFAULT;
#endif
	}
}

#if (DRM_VERSION < KERNEL_VERSION(5, 11, 0))
#if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
void innodpu_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
#else
static void innodpu_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
#endif
#elif (DRM_VERSION < KERNEL_VERSION(5, 18, 0))
static void innodpu_gem_prime_vunmap(struct drm_gem_object *obj, struct dma_buf_map *map)
#else
static void innodpu_gem_prime_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
#endif
{
	pgprot_t prot_writecombine;
#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
	void *vaddr = NULL;

	if (map)
		vaddr = map->vaddr;
#endif

	if (vaddr != NULL) {
		fh2m_inno_pgprot_writecombine(fh2m_inno_get_inno_page_kernel(), &prot_writecombine);
		fh2m_inno_vunmap(vaddr,obj->size/fh2m_inno_page_size,&prot_writecombine);
	}
}


static const struct dma_buf_ops s_innodpu_gem_prime_dmabuf_ops = {
	.attach = innodpu_gem_prime_attach,
	.detach = innodpu_gem_prime_detach,
	.map_dma_buf = innodpu_gem_prime_map_dma_buf,
	.unmap_dma_buf = innodpu_gem_prime_unmap_dma_buf,
	.release = innodpu_gem_dmabuf_release,
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#if (DRM_VERSION < KERNEL_VERSION(4, 19, 0))
	.map_atomic = innodpu_gem_prime_kmap_atomic,
#endif
#if (DRM_VERSION < KERNEL_VERSION(5, 6, 0))
	.map = innodpu_gem_prime_kmap,
#endif
#else
	.kmap_atomic = innodpu_gem_prime_kmap_atomic,
	.kmap = innodpu_gem_prime_kmap,
#endif
	.mmap = innodpu_gem_prime_mmap,
#if (DRM_VERSION >= KERNEL_VERSION(4, 17, 0))
	.vmap = drm_gem_dmabuf_vmap,
	.vunmap = drm_gem_dmabuf_vunmap,
#endif
};

struct dma_buf *innodpu_gem_prime_export(
#if (DRM_VERSION < KERNEL_VERSION(5, 4, 0))
	struct drm_device *dev,
#endif
	struct drm_gem_object *gem_obj, int flags)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(gem_obj);
	struct dma_buf *dbuf = NULL;
#if (DRM_VERSION >= KERNEL_VERSION(4, 1, 0))
	DEFINE_DMA_BUF_EXPORT_INFO(export_info);
#endif

#if (DRM_VERSION >= KERNEL_VERSION(4, 1, 0))
	export_info.ops = &s_innodpu_gem_prime_dmabuf_ops;
	export_info.resv = innodpu_obj->resv;
	export_info.size = gem_obj->size;
	export_info.priv = gem_obj;
	export_info.flags = flags;

#if (DRM_VERSION >= KERNEL_VERSION(5, 4, 0))
	dbuf = drm_gem_dmabuf_export(gem_obj->dev, &export_info);
#else
#if (DRM_VERSION >= KERNEL_VERSION(4, 9, 0))
	dbuf = drm_gem_dmabuf_export(dev, &export_info);
#else
	dbuf = dma_buf_export(&export_info);
#endif
#endif

	dbuf->exp_name = gem_obj->dev->render->kdev->kobj.name;
#else
	dbuf =
		dma_buf_export(gem_obj, &s_innodpu_gem_prime_dmabuf_ops, gem_obj->size, flags, innodpu_obj->resv);
#endif

	return dbuf;
}

/* drm gem func ops start */
#if (DRM_VERSION > KERNEL_VERSION(5, 2, 0))
static void innodpu_gem_func_free(struct drm_gem_object *obj)
{
        innodpu_gem_object_free(obj);
        return;
}
static int innodpu_gem_func_open(struct drm_gem_object *obj, struct drm_file *file)
{
        return 0;
}
static void innodpu_gem_func_close(struct drm_gem_object *obj, struct drm_file *file)
{
                return;
}
static void innodpu_gem_func_print_info(struct drm_printer *p, unsigned int indent,
                const struct drm_gem_object *obj)
{
        return;
}
static struct dma_buf *innodpu_gem_func_export(struct drm_gem_object *obj, int flags)
{
#if (DRM_VERSION >= KERNEL_VERSION(5, 4, 0))
        return innodpu_gem_prime_export(obj, flags);
#else
        return innodpu_gem_prime_export(obj->dev, obj, flags);
#endif
}

static struct drm_gem_object_funcs innodpu_gem_funcs = {
        .free = innodpu_gem_func_free,
        .open = innodpu_gem_func_open,
        .close = innodpu_gem_func_close,
        .print_info = innodpu_gem_func_print_info,
        .vm_ops = &innodpu_gem_vm_ops,
        .export = innodpu_gem_func_export,
        .vmap = innodpu_gem_prime_vmap,
        .vunmap = innodpu_gem_prime_vunmap,
};
#endif
/* drm gem func ops end */

struct drm_gem_object *innodpu_gem_prime_import(struct drm_device *drm_dev, struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);
	innodpu_gem_object *innodpu_obj = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct drm_gem_object *gem_obj = NULL;

	if (obj->dev == drm_dev) {
		BUG_ON(dma_buf->ops != &s_innodpu_gem_prime_dmabuf_ops);

		/*
		 * The dmabuf is one of ours, so return the associated
		 * PDP GEM object, rather than create a new one.
		 */
		drm_gem_object_get(obj);
		return obj;
	}

	innodpu_obj = innodpu_gem_private_object_init(drm_dev, dma_buf->size, dma_buf->resv);
	if (!innodpu_obj) {
		pr_err("[%s:%d]size = %#zx\n",__func__,__LINE__,dma_buf->size);
		return NULL;
	}

	gem_obj = &innodpu_obj->base;

	innodpu_obj->mem_manager = dev_priv->gtt_mem_manager;
	innodpu_obj->class = GTT;
	innodpu_obj->is_duplicate = true;

	attach = dma_buf_attach(dma_buf, drm_dev->dev);
	get_dma_buf(dma_buf);

#if (DRM_VERSION >= KERNEL_VERSION(5, 2, 0))
        if (!gem_obj->funcs) {
                gem_obj->funcs = &innodpu_gem_funcs;
        }
#endif
        if (!gem_obj->name) {
                gem_obj->name = idr_alloc(&drm_dev->object_name_idr, obj, 1, 0, fh2m_hal_get_inno_gfp_kernel());
        }


	innodpu_obj->base.import_attach = attach;
	innodpu_obj->resv = dma_buf->resv;
	return &innodpu_obj->base;
}

#if (DRM_VERSION <= KERNEL_VERSION(5, 2, 0))
struct reservation_object * innodpu_gem_prime_res_obj(struct drm_gem_object *gem_obj)
{
	innodpu_gem_object *innodpu_obj;

	innodpu_obj = to_innodpu_obj(gem_obj);

	return innodpu_obj->resv;
}
#endif

/* free innodpu obj resource func start*/
static void innodpu_gem_shared_free(void *manager, void *obj)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;

	/* this should never established */
	if (mem_manager->manage_mode != MEM_DRM_MM_MODE) {
		return;
	}

	/* share memory shuold clear to 0 when free */
	innodpu_gem_vbuffer_clear(mem_manager, innodpu_obj, drm_dev->dev);

	drm_mm_remove_node(&innodpu_obj->mm_node);

	return;
}

static void innodpu_gem_gtt_free(void *manager, void *obj)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;

	if (innodpu_obj->class != GTT) {
		return;
	}

	fh2m_hal_gtt_free(drm_dev->dev, mem_manager->role, innodpu_obj->gtt);

	list_del(&innodpu_obj->mem_node);

	return;
}

static void innodpu_gem_vram_free(void *manager, void *obj)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;
	bool continuous = (innodpu_obj->class == NO_CONTINUOUS_VRAM) ? false : true;

	if (continuous) {
		if (mem_manager->visible) {
			fh2m_hal_vram_free(drm_dev->dev, mem_manager->role, innodpu_obj->dev_paddr);
		} else {
			fh2m_hal_inv_vram_free(drm_dev->dev, mem_manager->role, innodpu_obj->dev_paddr);
		}
	} else {
		fh2m_hal_physmem_free(drm_dev->dev, mem_manager->role, mem_manager->visible, innodpu_obj->pmr);
	}

	list_del(&innodpu_obj->mem_node);

	return;
}

static void innodpu_gem_object_free_priv(innodpu_gem_object *innodpu_obj)
{
	innodpu_mem_manager *mem_manager = innodpu_obj->mem_manager;
	struct drm_gem_object *gem_obj = &innodpu_obj->base;
	struct drm_device *drm_dev = gem_obj->dev;

	struct innodpu_drm_private *dev_priv = NULL;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);
	else
		return;

	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (mem_manager->mem_free == NULL) {
		gem_err(drm_dev->dev, "mem_manager mem free callback is null\n");
		return;
	}

	drm_gem_free_mmap_offset(gem_obj);

#if (DRM_VERSION < KERNEL_VERSION(5, 2, 0))
	if (&innodpu_obj->_resv == innodpu_obj->resv)
		dma_resv_fini(&innodpu_obj->_resv);
#endif

	if (gem_obj->import_attach) {
		drm_prime_gem_destroy(gem_obj, innodpu_obj->sgt);
		gem_obj->import_attach = NULL;
	}

	mutex_lock(&mem_manager->mem_lock);
	mem_manager->mem_free(mem_manager, innodpu_obj);
	mutex_unlock(&mem_manager->mem_lock);

	drm_gem_object_release(gem_obj);
	kfree(innodpu_obj->name);
	kfree(innodpu_obj);
}

void innodpu_gem_object_free(struct drm_gem_object *gem_obj)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(gem_obj);
	innodpu_mem_manager *mem_manager = innodpu_obj->mem_manager;

	if (innodpu_obj->is_duplicate) {
		if (gem_obj->import_attach) {
			drm_prime_gem_destroy(gem_obj, innodpu_obj->sgt);
			kfree(innodpu_obj);
		}

		return;
	}

	gem_info(gem_obj->dev->dev, "(%d)%s Visible-%d class-%d size-%#llx, paddr-%#llx, daddr-%#llx, innodpu_obj-%#llx\n",
		innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->mem_manager->visible,
		innodpu_obj->base.size, innodpu_obj->class,
		innodpu_obj->cpu_paddr, innodpu_obj->dev_paddr, innodpu_obj);

	/* delete from power manager(s3/s4) list */
	mutex_lock(&mem_manager->pm_lock);
	list_del(&innodpu_obj->pm_node);
	mutex_unlock(&mem_manager->pm_lock);

	/* actually release buffer resource */
	innodpu_gem_object_free_priv(innodpu_obj);

	return;
}
/* free innodpu obj resource func end*/

/* create innodpu obj resource func start */

#define inno_show_mm(mm) do { \
	struct drm_printer __p = drm_debug_printer(__func__); \
	drm_mm_print((mm), &__p); } while (0);

static int innodpu_gem_shared_alloc(void *manager, void *obj, size_t size)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;
	bool visible = 0;
	int err = 0;

	visible = mem_manager->visible;

	/* this should never established  */
	if ((mem_manager->manage_mode != MEM_DRM_MM_MODE) || innodpu_obj->class != CONTINUOUS_VRAM) {
		err = -1;
		goto exit;
	}

	err = drm_mm_insert_node(&mem_manager->gem_mm, &innodpu_obj->mm_node, size);
	if (err) {
		gem_info(drm_dev->dev, "drm mm short of vram memory, error: %d, size: %zu\n", err, size);
		goto exit;
	}

	innodpu_obj->dev_paddr = innodpu_obj->mm_node.start;
	if (visible) {
		innodpu_obj->cpu_paddr = fh2m_dev_paddr_to_cpu_paddr(drm_dev->dev, innodpu_obj->dev_paddr);
	}

exit:
	return err;
}

static int innodpu_gem_gtt_alloc(void *manager, void *obj, size_t size)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;
	uint64_t gtt_flags = s_dpu_gtt_mem_clear ? PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC : 0;
	int err = 0;

	if ((innodpu_obj->class != GTT) || (mem_manager->manage_mode != MEM_LIST_MODE)) {
		err = -1;
		goto exit;
	}

	innodpu_obj->gtt = fh2m_hal_gtt_alloc(drm_dev->dev, mem_manager->role, size, gtt_flags);
	if (!innodpu_obj->gtt) {
		gem_err(drm_dev->dev, "list short of gtt memory, size-%ld\n", size);
		err = -1;
		goto exit;
	}

	list_add_tail(&innodpu_obj->mem_node, &mem_manager->mem_list);
exit:
	return err;
}

/*
 * 1.By the user flags bit26(DBM_GEM_NEED_CONTINUOUS), to judge target vram must be continuous nor not;
 * 2.Preferentially Call fh2m_hal_vram_alloc to alloc continuos vram;
 * 3.if step2 get continuous vram successfully, return success to user;
 * 4.Or
 * 4.1 if step1 judge target vram can be nocontinuous; try to call fh2m_hal_physmem_alloc to alloc nocontinuous varm;
 * 4.2 or if step1 judge target vram must be continuous, return error to user;
 */
static int innodpu_gem_vram_alloc(void *manager, void *obj, size_t size)
{
	int err = 0;
	uint64_t vram_alloc_flags = 0;
	bool not_need_clear = false;
	bool not_need_continuous = false;
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;
	struct drm_device *drm_dev = mem_manager->drm_dev;
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);

	if ((innodpu_obj->class != CONTINUOUS_VRAM) || (mem_manager->manage_mode != MEM_LIST_MODE)) {
		err = -1;
		return err;
	}

	/* The wayland application has a preformance bottleneck of zeroing the video
	* memory everytime it is requested, and the wayland appliaction will actively zero the video
	* memory time it is requested.*/
	not_need_clear = (innodpu_obj->flags & DBM_GEM_NO_CLEAR) ? true : false;

	not_need_continuous = (innodpu_obj->flags & DBM_GEM_NEED_CONTINUOUS) ? false : true;
	/* if not_need_continuous, make fh2m_hal_vram_alloc not output failed info */
	if (not_need_continuous) {
		vram_alloc_flags |= BIT(HAL_PHYSMEM_ALLOC_NO_WARN);
	}

	if (not_need_continuous && dev_priv->has_uncontinuous_mem) {
		uint64_t mem_alloc_flags = 0;

		HAL_PHYSMEM_ALLOC_FLAG_SET(mem_alloc_flags, NON_CONTIG);
		if (mem_manager->visible)
			HAL_PHYSMEM_ALLOC_FLAG_SET(mem_alloc_flags, LMA);
		else
			HAL_PHYSMEM_ALLOC_FLAG_SET(mem_alloc_flags, LMA_INV);

		innodpu_obj->pmr = fh2m_hal_physmem_alloc(drm_dev->dev, mem_manager->role, size,
			mem_manager->visible, mem_alloc_flags);

		if (!innodpu_obj->pmr) {
			err = -1;
		} else {
			innodpu_obj->class = NO_CONTINUOUS_VRAM;
			if (!not_need_clear)
				innodpu_gem_vbuffer_clear_nocontinuous(mem_manager, innodpu_obj, drm_dev->dev);
		}
	} else {
		innodpu_obj->dev_paddr = fh2m_hal_vram_alloc(drm_dev->dev, mem_manager->role, mem_manager->visible, size, vram_alloc_flags);
		if (innodpu_obj->dev_paddr) {
			if (mem_manager->visible)
				innodpu_obj->cpu_paddr = fh2m_dev_paddr_to_cpu_paddr(drm_dev->dev, innodpu_obj->dev_paddr);
			if (!not_need_clear)
				innodpu_gem_vbuffer_clear(mem_manager, innodpu_obj, drm_dev->dev);
		} else {
			err = -1;
		}
	}

	if (!err) {
		list_add_tail(&innodpu_obj->mem_node, &mem_manager->mem_list);
	} else {
		gem_err(drm_dev->dev, "list short of vram memory, size-%ld \n", size);
	}

	return err;
}

struct drm_gem_object *innodpu_gem_object_create_priv(struct drm_device *drm_dev,
	innodpu_mem_manager *mem_manager, size_t size, u32 flags)
{
	innodpu_gem_object *innodpu_obj = NULL;
	struct drm_gem_object *gem_obj = NULL;
	struct innodpu_drm_private *dev_priv = NULL;
	int err = 0;
	int class = 0;

	if (mem_manager->mem_alloc == NULL) {
		gem_err(drm_dev->dev, "mem_manager alloc callback is null\n");
		return NULL;
	}

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return NULL;
	}

	class = innodpu_get_dmbflags_to_class(flags, mem_manager->pos);
	if (class == -1) {
		gem_err(drm_dev->dev, "target mem obj class get failed\n");
		return NULL;
	}

	innodpu_obj = innodpu_gem_private_object_init(drm_dev, size, NULL);
	if (!innodpu_obj) {
		gem_err(drm_dev->dev, "Failed create inno_gem_object, short of system memory\n");
		return NULL;
	}
	gem_obj = &innodpu_obj->base;

	innodpu_obj->name =  kasprintf(fh2m_hal_get_inno_gfp_kernel(), "%s-%d", current->comm, current->pid);
	if (innodpu_obj->name == NULL) {
		gem_err(drm_dev->dev, "Failed create obj-name, short of memory\n");
		err = -ENOMEM;
		goto err_name;
	}

	innodpu_obj->mem_manager = mem_manager;
	innodpu_obj->dma_map_export_host_addr = false;
	innodpu_obj->class = class;
	innodpu_obj->flags = flags;

	mutex_lock(&mem_manager->mem_lock);
	err = mem_manager->mem_alloc(mem_manager, innodpu_obj, size);
	mutex_unlock(&mem_manager->mem_lock);
	if (err) {
		goto err_vram_alloc;
	}

#if (DRM_VERSION >= KERNEL_VERSION(5, 2, 0))
	if (!gem_obj->funcs) {
		gem_obj->funcs = &innodpu_gem_funcs;
	}
	innodpu_obj->resv = innodpu_obj->base.resv;
#else
	innodpu_obj->resv = &innodpu_obj->_resv;
#endif

	mutex_lock(&drm_dev->object_name_lock);
	if (!gem_obj->name) {
		gem_obj->name = idr_alloc(&drm_dev->object_name_idr, gem_obj, 1, 0, fh2m_hal_get_inno_gfp_kernel());
	}
	mutex_unlock(&drm_dev->object_name_lock);

	INIT_LIST_HEAD(&innodpu_obj->pm_node);
	mutex_lock(&mem_manager->pm_lock);
	list_add_tail(&innodpu_obj->pm_node, &mem_manager->pm_list);
	mutex_unlock(&mem_manager->pm_lock);/*  */

	return &innodpu_obj->base;

err_vram_alloc:
	kfree(innodpu_obj->name);
err_name:
	kfree(innodpu_obj);
	return NULL;
}

int innodpu_gem_object_create(struct drm_file *drm_file, struct drm_device *drm_dev,
	innodpu_mem_manager *mem_manager, void *data, bool is_priv)
{
	int ret = 0;
	size_t size = 0;
	uint32_t flags = 0, pitch = 0;
	uint32_t *handle = NULL;
	struct drm_pdp_gem_create *priv_args = NULL;
	struct drm_mode_create_dumb *dumb_args = NULL;
	struct drm_gem_object *gem_obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;

	if (is_priv) {
		priv_args = (struct drm_pdp_gem_create *)data;

		flags = priv_args->flags;
		size = PAGE_ALIGN(priv_args->size);
		handle = &priv_args->handle;
	} else {
		dumb_args = (struct drm_mode_create_dumb *)data;

		handle = &dumb_args->handle;
		flags = dumb_args->flags;
#ifdef HW_ALIGN
		pitch = ALIGN(DIV_ROUND_UP(dumb_args->width * dumb_args->bpp, 8), 16);
#else
		pitch = dumb_args->width * (ALIGN(dumb_args->bpp, 8) >> 3);
#endif
		size = PAGE_ALIGN(pitch * dumb_args->height);

		dumb_args->pitch = pitch;
		dumb_args->size = size;
	}

	gem_obj = innodpu_gem_object_create_priv(drm_dev, mem_manager, size, flags);
	if (!gem_obj) {
		return -ENOMEM;
	}
	innodpu_obj = to_innodpu_obj(gem_obj);

	if (is_priv) {
		innodpu_obj->size_origin = priv_args->size;
		innodpu_obj->size_align = size;
	} else {
		innodpu_obj->size_origin = pitch * dumb_args->height;
		innodpu_obj->size_align = size;
	}

	ret = drm_gem_handle_create(drm_file, gem_obj, handle);
	if (ret) {
		goto err_handle;
	}
	drm_gem_object_put(gem_obj);

	gem_info(drm_dev->dev, "%s(%d) Visible-%d Pos-%d Class-%d Input: size-%llu create handle. ret-%d, innodpu_obj-%pK flags-0x%x\n",
		innodpu_obj->name, gem_obj->name, mem_manager->visible,
		mem_manager->pos, innodpu_obj->class,
		size, ret, innodpu_obj, innodpu_obj->flags);

	gem_info(drm_dev->dev, DRM_VRAM_FMT, DRM_VRAM_ARG(innodpu_obj));

	return ret;

err_handle:
	drm_gem_object_put(gem_obj);
	innodpu_gem_object_free(gem_obj);
	return ret;
}
/* create innodpu obj resource func end */

/* s3/s4 backup/recover func start */
static void innodpu_gem_gtt_backup(void *manager, void *obj)
{
	return;
}

static void innodpu_gem_gtt_recover(void *manager, void *obj)
{
	return;
}

static void innodpu_gem_nocontinuous_vram_backup(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj)
{
	struct drm_gem_object *obj;
	struct device *pdev = NULL;
	int size = 0, ret = 0;

	obj = &innodpu_obj->base;
	innodpu_obj->suspend_data = fh2m_inno_vmalloc(innodpu_obj->base.size);

	if (innodpu_obj->suspend_data == NULL) {
		gem_err(obj->dev->dev, "vmalloc failed\n");
		return;
	}

	pdev = obj->dev->dev;
	size = (int)(innodpu_obj->base.size);

	gem_info(innodpu_obj->base.dev->dev, "backup (%d)%s, flags %#x\n", innodpu_obj->base.name,
		innodpu_obj->name, innodpu_obj->flags);

	if (innodpu_obj->pmr->base_array_size == 1) {
		void *src = NULL, *dst = NULL;

		if (mem_manager->visible) {
			src = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdev,
				fh2m_dev_paddr_to_cpu_paddr(pdev, innodpu_obj->pmr->base_array[0]));
			dst = (void*)innodpu_obj->suspend_data;
			ret = innodpu_dma_memcpy3(pdev, &src, &dst, &size, 1, GDDR2SYS);
		} else {
			src = (void *)innodpu_obj->pmr->base_array[0];
			dst = (void *)innodpu_obj->suspend_data;
			ret = innodpu_dma_memcpy_for_smallbar_sg(pdev, &src, &dst, &size, 1, GDDR2SYS);
		}
		if (ret) {
			gem_err(obj->dev->dev,"innodpu_dma_memcpy3 suspend failed\n");
		}
	} else {
		void **src_array = NULL, **dst_array = NULL;
		int *len_array = NULL;
		int i = 0, num = 0;
		unsigned int vram_offset = 0;

		src_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		dst_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		len_array = fh2m_inno_vmalloc(sizeof(int) * innodpu_obj->pmr->base_array_size);

		if (src_array && dst_array && len_array) {
			num = innodpu_gem_merge_nocontinuous_page(innodpu_obj, len_array, 0);

			if (num < 1 || num > innodpu_obj->pmr->base_array_size) {
				gem_err(innodpu_obj->base.dev->dev, "nocontinuous page merge err.\n");
				return;
			}

			if (mem_manager->visible) {
				for (i = 0; i < num; i++) {
					*(src_array + i) = (void *)fh2m_cpu_paddr_to_pcie_paddr(pdev,
						fh2m_dev_paddr_to_cpu_paddr(pdev, innodpu_obj->pmr->base_array[vram_offset / PAGE_SIZE]));
					*(dst_array + i) = (void *)innodpu_obj->suspend_data + vram_offset;
					vram_offset += *(len_array + i);
				}
				ret = innodpu_dma_memcpy3(pdev, src_array, dst_array,
					len_array, num, GDDR2SYS);
			} else {
				for (i = 0; i < num; i++) {
					*(src_array + i) = (void *)innodpu_obj->pmr->base_array[vram_offset / PAGE_SIZE];
					*(dst_array + i) = (void *)innodpu_obj->suspend_data + vram_offset;
					vram_offset += *(len_array + i);
				}
				ret = innodpu_dma_memcpy_for_smallbar_sg(pdev, src_array, dst_array,
					len_array, num, GDDR2SYS);
			}
			if (ret) {
				gem_err(obj->dev->dev, "innodpu_dma_memcpy3 suspend failed\n");
			}
		}

		if (src_array)
			fh2m_inno_vfree(src_array);
		if (dst_array)
			fh2m_inno_vfree(dst_array);
		if (len_array)
			fh2m_inno_vfree(len_array);
	}

	return;
}

static void innodpu_gem_continuous_vram_backup(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj)
{
	struct drm_gem_object *obj;
	struct device *pdev = NULL;
	void *src = NULL, *dst = NULL;
	int size = 0, ret = 0;

	obj = &innodpu_obj->base;
	innodpu_obj->suspend_data = fh2m_inno_vmalloc(innodpu_obj->base.size);

	if (innodpu_obj->suspend_data == NULL) {
		gem_err(obj->dev->dev, "vmalloc failed\n");
		return;
	}

	pdev = obj->dev->dev;
	size = (int)(innodpu_obj->base.size);

	gem_info(innodpu_obj->base.dev->dev, "backup (%d)%s, flags %#x\n",
			 innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->flags);

	if (mem_manager->visible) {
		src = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdev, innodpu_obj->cpu_paddr);
		dst = (void*)innodpu_obj->suspend_data;
		ret = innodpu_dma_memcpy3(pdev, &src, &dst, &size, 1, GDDR2SYS);
	} else {
		src = (void *)innodpu_obj->dev_paddr;
		dst = (void *)innodpu_obj->suspend_data;
		ret = innodpu_dma_memcpy_for_smallbar_sg(pdev, &src, &dst, &size, 1, GDDR2SYS);
	}

	if (ret) {
		gem_err(obj->dev->dev, "innodpu_dma_memcpy3 suspend failed\n");
	}

	return;
}

static void innodpu_gem_nocontinuous_vram_recover(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj)
{
	struct drm_gem_object *obj = &innodpu_obj->base;
	struct device *pdev = obj->dev->dev;
	int size = (int)(innodpu_obj->base.size);

	if (!innodpu_obj->suspend_data) {
		return;
	}

	if (!fh2m_inno_is_vmalloc_addr(innodpu_obj->suspend_data)) {
		return;
	}

	gem_info(innodpu_obj->base.dev->dev, "recover (%d)%s, flags %#x\n",
			 innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->flags);

	if (innodpu_obj->pmr->base_array_size == 1) {
		void *src = NULL, *dst = NULL;

		if (mem_manager->visible) {
			dst = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdev,
				fh2m_dev_paddr_to_cpu_paddr(pdev, innodpu_obj->pmr->base_array[0]));
			src = (void*)innodpu_obj->suspend_data;
			innodpu_dma_memcpy3(pdev, &src, &dst, &size, 1, SYS2GDDR);
		} else {
			src = (void *)innodpu_obj->suspend_data;
			dst = (void *)innodpu_obj->pmr->base_array[0];
			innodpu_dma_memcpy_for_smallbar_sg(pdev, &src, &dst, &size, 1, SYS2GDDR);
		}
	} else {
		void **src_array = NULL, **dst_array = NULL;
		int *len_array = NULL;
		int i = 0, num = 0;
		unsigned int vram_offset = 0;

		src_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		dst_array = fh2m_inno_vmalloc(sizeof(void *) * innodpu_obj->pmr->base_array_size);
		len_array = fh2m_inno_vmalloc(sizeof(int) * innodpu_obj->pmr->base_array_size);

		if (src_array && dst_array && len_array) {
			num = innodpu_gem_merge_nocontinuous_page(innodpu_obj, len_array, 0);
			if (num < 1 || num > innodpu_obj->pmr->base_array_size) {
				gem_err(innodpu_obj->base.dev->dev, "nocontinuous page merge err.\n");
				return;
			}

			if (mem_manager->visible) {
				for (i = 0; i < num; i++) {
					*(dst_array + i) = (void *)fh2m_cpu_paddr_to_pcie_paddr(pdev,
						fh2m_dev_paddr_to_cpu_paddr(pdev, innodpu_obj->pmr->base_array[vram_offset / PAGE_SIZE]));
					*(src_array + i) = (void *)innodpu_obj->suspend_data + vram_offset;
					vram_offset += *(len_array + i);
				}
				innodpu_dma_memcpy3(pdev, src_array, dst_array,
					len_array, num, SYS2GDDR);
			} else {
				for (i = 0; i < num; i++) {
					*(dst_array + i) = (void *)innodpu_obj->pmr->base_array[vram_offset / PAGE_SIZE];
					*(src_array + i) = (void *)innodpu_obj->suspend_data + vram_offset;
					vram_offset += *(len_array + i);
				}
				innodpu_dma_memcpy_for_smallbar_sg(pdev, src_array, dst_array,
					len_array, num, SYS2GDDR);
			}
		}

		if (src_array)
			fh2m_inno_vfree(src_array);
		if (dst_array)
			fh2m_inno_vfree(dst_array);
		if (len_array)
			fh2m_inno_vfree(len_array);
	}

	fh2m_inno_vfree(innodpu_obj->suspend_data);
	innodpu_obj->suspend_data = NULL;

	return;
}

static void innodpu_gem_continuous_vram_recover(innodpu_mem_manager *mem_manager,
	innodpu_gem_object *innodpu_obj)
{
	if (innodpu_obj->suspend_data) {
		if (fh2m_inno_is_vmalloc_addr(innodpu_obj->suspend_data)) {
			struct drm_gem_object *obj = &innodpu_obj->base;
			struct device *pdev = obj->dev->dev;
			int size = (int)(innodpu_obj->base.size);
			void *src = NULL;
			void *dst = NULL;

			gem_info(innodpu_obj->base.dev->dev, "recover (%d)%s, flags %#x\n",
					 innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->flags);

			if (mem_manager->visible) {
				dst = (void*)fh2m_cpu_paddr_to_pcie_paddr(pdev, innodpu_obj->cpu_paddr);
				src = (void*)innodpu_obj->suspend_data;
				innodpu_dma_memcpy3(pdev, &src, &dst, &size, 1, SYS2GDDR);
			} else {
				src = (void *)innodpu_obj->suspend_data;
				dst = (void *)innodpu_obj->dev_paddr;
				innodpu_dma_memcpy_for_smallbar_sg(pdev, &src, &dst, &size, 1, SYS2GDDR);
			}
		}

		fh2m_inno_vfree((void *)innodpu_obj->suspend_data);
		innodpu_obj->suspend_data = NULL;
	}

	return;
}

static void innodpu_gem_vram_backup(void *manager, void *obj)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		innodpu_gem_continuous_vram_backup(mem_manager, innodpu_obj);
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		innodpu_gem_nocontinuous_vram_backup(mem_manager, innodpu_obj);
	}

	return;
}

static void innodpu_gem_vram_recover(void *manager, void *obj)
{
	innodpu_mem_manager *mem_manager = (innodpu_mem_manager *)manager;
	innodpu_gem_object *innodpu_obj = (innodpu_gem_object *)obj;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		innodpu_gem_continuous_vram_recover(mem_manager, innodpu_obj);
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		innodpu_gem_nocontinuous_vram_recover(mem_manager, innodpu_obj);
	}

	return;
}
/* s3/s4 backup/recover func end */

/* extern func call by other scr code file */
void innodpu_sysmem_thaw_free(innodpu_mem_manager *mem_manager)
{
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	if (mem_manager->manage_mode == MEM_DRM_MM_MODE) {
		const struct drm_mm_node *entry = NULL;

		drm_mm_for_each_node(entry, &mem_manager->gem_mm) {
			innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
			if (innodpu_obj->suspend_data) {
				gem_info(innodpu_obj->base.dev->dev, "free (%d)%s, flags %#x\n",
						 innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->flags);
				fh2m_inno_vfree((void *)innodpu_obj->suspend_data);
				innodpu_obj->suspend_data = NULL;
			}
		}
	} else {
		mutex_lock(&mem_manager->pm_lock);
		list_for_each_entry_safe(innodpu_obj, tmp, &mem_manager->pm_list, pm_node) {
			if (innodpu_obj->suspend_data) {
				gem_info(innodpu_obj->base.dev->dev, "free (%d)%s, flags %#x\n",
						 innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->flags);
				fh2m_inno_vfree((void *)innodpu_obj->suspend_data);
				innodpu_obj->suspend_data = NULL;
			}
		}
		mutex_unlock(&mem_manager->pm_lock);
	}
}

void innodpu_gem_backup(innodpu_mem_manager *mem_manager)
{
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	if (mem_manager->manage_mode == MEM_DRM_MM_MODE) {
		const struct drm_mm_node *entry = NULL;

		drm_mm_for_each_node(entry, &mem_manager->gem_mm) {
			innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
			mem_manager->mem_backup(mem_manager, innodpu_obj);
#ifdef DRM_PM_GEM_VERIFY_MEMBACK
			{
				char _tmpname[100] = {'\0'};
				sprintf(_tmpname, "/tmp/backup-%d_%s.bin", innodpu_obj->base.name, innodpu_obj->name);
				innodpu_gem_write2file(_tmpname, innodpu_obj->suspend_data, innodpu_obj->base.size);
			}
#endif
			gem_info(innodpu_obj->base.dev->dev, "DEBUG-BACKUP: (%d)%s innodpu_obj-flag[0x%x] suspend data %p\n",
				innodpu_obj->base.name, innodpu_obj->name,
				innodpu_obj->flags, innodpu_obj->suspend_data);
		}
	} else {
		mutex_lock(&mem_manager->pm_lock);
		list_for_each_entry_safe(innodpu_obj, tmp, &mem_manager->pm_list, pm_node) {
			mem_manager->mem_backup(mem_manager, innodpu_obj);
#ifdef DRM_PM_GEM_VERIFY_MEMBACK
			{
				char _tmpname[100] = {'\0'};
				sprintf(_tmpname, "/tmp/backup-%d_%s.bin", innodpu_obj->base.name, innodpu_obj->name);
				innodpu_gem_write2file(_tmpname, innodpu_obj->suspend_data, innodpu_obj->base.size);
			}
#endif
			gem_info(innodpu_obj->base.dev->dev, "DEBUG-BACKUP: (%d)%s innodpu_obj-flag[0x%x] suspend data %p\n",
				innodpu_obj->base.name, innodpu_obj->name,
				innodpu_obj->flags, innodpu_obj->suspend_data);
		}
		mutex_unlock(&mem_manager->pm_lock);
	}

	return;
}

void innodpu_gem_recover(innodpu_mem_manager *mem_manager)
{
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	if (mem_manager->manage_mode == MEM_DRM_MM_MODE) {
		const struct drm_mm_node *entry = NULL;

		drm_mm_for_each_node(entry, &mem_manager->gem_mm) {
			innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
			if (innodpu_obj->suspend_data) {
#ifdef DRM_PM_GEM_VERIFY_MEMBACK
				{
					char _tmpname[100] = {'\0'};
					sprintf(_tmpname, "/tmp/recover-%d_%s.bin", innodpu_obj->base.name, innodpu_obj->name);
					innodpu_gem_write2file(_tmpname, innodpu_obj->suspend_data, innodpu_obj->base.size);
				}
#endif
				gem_info(innodpu_obj->base.dev->dev, "DEBUG-RECOVER: (%d)%s innodpu_obj-flag[0x%x] suspend data %p\n",
					innodpu_obj->base.name, innodpu_obj->name,
					innodpu_obj->flags, innodpu_obj->suspend_data);
				mem_manager->mem_recover(mem_manager, innodpu_obj);
			}
		}
	} else {
		mutex_lock(&mem_manager->pm_lock);
		list_for_each_entry_safe(innodpu_obj, tmp, &mem_manager->pm_list, pm_node) {
			if (innodpu_obj->suspend_data) {
#ifdef DRM_PM_GEM_VERIFY_MEMBACK
				{
					char _tmpname[100] = {'\0'};
					sprintf(_tmpname, "/tmp/recover-%d_%s.bin", innodpu_obj->base.name, innodpu_obj->name);
					innodpu_gem_write2file(_tmpname, innodpu_obj->suspend_data, innodpu_obj->base.size);
				}
#endif
				gem_info(innodpu_obj->base.dev->dev, "DEBUG-RECOVER: (%d)%s innodpu_obj-flag[0x%x] suspend data %p\n",
					innodpu_obj->base.name, innodpu_obj->name,
					innodpu_obj->flags, innodpu_obj->suspend_data);
				mem_manager->mem_recover(mem_manager, innodpu_obj);
			}
		}
		mutex_unlock(&mem_manager->pm_lock);
	}

	return;
}

/* cuase zero vram always equal with 0 */
void innodpu_gem_zero_vram_recover(struct drm_device *drm_dev, innodpu_zero_gem *zero_gem)
{
	void *src = NULL, *dst = NULL;
	int ret = 0, size = GEM_ZERO_SIZE;

	if (zero_gem->backup_buffer) {
		dst = (void *)fh2m_cpu_paddr_to_pcie_paddr(drm_dev->dev, zero_gem->info.cpu_paddr);
		src = (void *)zero_gem->backup_buffer;
		ret = innodpu_dma_memcpy3(drm_dev->dev, &src, &dst, &size, 1, SYS2GDDR);
	}

	if (ret) {
		gem_err(drm_dev->dev, "innodpu_dma_memcpy3 recover zero-mem failed\n");
	}

	return;
}

void innodpu_gem_suspend(innodpu_mem_manager *mem_manager)
{

	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	mutex_lock(&mem_manager->pm_lock);
	list_for_each_entry_safe(innodpu_obj, tmp, &mem_manager->pm_list, pm_node) {
		mem_manager->mem_backup(mem_manager, innodpu_obj);
	}
	mutex_unlock(&mem_manager->pm_lock);
}

innodpu_mem_manager *innodpu_mem_manager_init(struct drm_device *drm_dev,
	bool visible, innodpu_mem_positon pos, innodpu_shared_mem *shared_mem_info)
{
	innodpu_mem_manager *mem_manager = NULL;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return NULL;
	}

	if (pos == SHMEM_VRAM_POSITION && shared_mem_info == NULL) {
		gem_err(drm_dev->dev, "shared_mem_info invalid\n");
		return NULL;
	}

	mem_manager = fh2m_inno_kzalloc_kernel(sizeof(*mem_manager));
	if (!mem_manager) {
		gem_err(drm_dev->dev, "mem_manager init visible-%d pos-%d is failed\n", visible, pos);
		return NULL;
	}

	mem_manager->drm_dev = drm_dev;
	mem_manager->visible = visible;
	mem_manager->pos = pos;
	mem_manager->role = &dev_priv->role;
	mem_manager->zero_gem = dev_priv->zero_gem;

	mutex_init(&mem_manager->mem_lock);
	mutex_init(&mem_manager->pm_lock);
	INIT_LIST_HEAD(&mem_manager->pm_list);

	switch (mem_manager->pos) {
	case VRAM_POSITION:
		mem_manager->mem_alloc = innodpu_gem_vram_alloc;
		mem_manager->mem_free = innodpu_gem_vram_free;
		mem_manager->mem_backup = innodpu_gem_vram_backup;
		mem_manager->mem_recover = innodpu_gem_vram_recover;
		if (mem_manager->visible) {
			mem_manager->vm_fault = innodpu_gem_visible_vram_vm_fault;
			mem_manager->vm_close = NULL;
			mem_manager->vm_open = NULL;
			mem_manager->size = 0x6400000; /* 100Mbytes, this is a fake value */
		} else {
			mem_manager->vm_fault = innodpu_gem_invisible_vram_vm_fault;
			mem_manager->vm_close = innodpu_gem_invisible_vram_close;
			mem_manager->vm_open = NULL;
			mem_manager->size = 0x19000000; /* 400Mbytes, this is a fake value */
		}
		break;

	case SHMEM_VRAM_POSITION:
		mem_manager->mem_alloc = innodpu_gem_shared_alloc;
		mem_manager->mem_free = innodpu_gem_shared_free;
		mem_manager->mem_backup = innodpu_gem_vram_backup;
		mem_manager->mem_recover = innodpu_gem_vram_recover;
		if (mem_manager->visible) {
			mem_manager->vm_fault = innodpu_gem_visible_vram_vm_fault;
			mem_manager->vm_close = NULL;
			mem_manager->vm_open = NULL;
		} else {
			mem_manager->vm_fault = innodpu_gem_invisible_vram_vm_fault;
			mem_manager->vm_close = innodpu_gem_invisible_vram_close;
			mem_manager->vm_open = NULL;
		}
		break;

	case SYS_GTT_POSITION:
		mem_manager->mem_alloc = innodpu_gem_gtt_alloc;
		mem_manager->mem_free = innodpu_gem_gtt_free;
		mem_manager->mem_backup = innodpu_gem_gtt_backup;
		mem_manager->mem_recover = innodpu_gem_gtt_recover;
		mem_manager->vm_fault = innodpu_gem_gtt_vm_fault;
		mem_manager->vm_open = NULL;
		mem_manager->vm_close = NULL;
		break;
	}

	if (mem_manager->pos == SHMEM_VRAM_POSITION) {
		mem_manager->manage_mode = MEM_DRM_MM_MODE; /* for shared mem */
		drm_mm_init(&mem_manager->gem_mm, shared_mem_info->dev_paddr, shared_mem_info->size);
		gem_info(drm_dev->dev, "mem_manager init visible-%d pos-%d vram_baddr-%#llx, size-%#llx\n",
			mem_manager->visible, mem_manager->pos, shared_mem_info->dev_paddr, shared_mem_info->size);
	} else {
		mem_manager->manage_mode = MEM_LIST_MODE; /* for nomral-vram/gtt */
		INIT_LIST_HEAD(&mem_manager->mem_list);
		gem_info(drm_dev->dev, "mem_manager init visible-%d pos-%d\n",
			mem_manager->visible, mem_manager->pos);
	}

	return mem_manager;
}

void innodpu_mem_manager_fini(struct drm_device *drm_dev, innodpu_mem_manager *mem_manager)
{
	struct innodpu_drm_private *dev_priv = NULL;
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (mem_manager->pos == SHMEM_VRAM_POSITION) {
		const struct drm_mm_node *entry = NULL;

		mutex_lock(&mem_manager->mem_lock);
		drm_mm_for_each_node(entry, &mem_manager->gem_mm) {
			innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
			mem_manager->mem_free(mem_manager, innodpu_obj);
			kfree(innodpu_obj->name);
			kfree(innodpu_obj);
			innodpu_obj = NULL;
		}
		drm_mm_takedown(&mem_manager->gem_mm);
		mutex_unlock(&mem_manager->mem_lock);

	} else {
		mutex_lock(&mem_manager->mem_lock);
		list_for_each_entry_safe(innodpu_obj, tmp, &mem_manager->mem_list, mem_node) {
			mem_manager->mem_free(mem_manager, innodpu_obj);
			kfree(innodpu_obj->name);
			kfree(innodpu_obj);
			innodpu_obj = NULL;
		}
		mutex_unlock(&mem_manager->mem_lock);
	}

	mutex_destroy(&mem_manager->mem_lock);
	mutex_destroy(&mem_manager->pm_lock);

	fh2m_inno_kfree(mem_manager);

	return;
}

/* share mem pre alloc start */
int innodpu_gem_share_pre_alloc(struct drm_device *drm_dev,
	innodpu_shared_mem *pshared_mem, unsigned long mem_size)
{
	int ret = 0;
	bool visible = true;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		ret = -EINVAL;
		goto exit;
	}

	if (fh2m_hal_has_inv_mem(drm_dev->dev)) {
		visible = false;
	}

	pshared_mem->dev_paddr = fh2m_hal_vram_alloc(drm_dev->dev, &dev_priv->role, visible, mem_size, 0);
	if (!pshared_mem->dev_paddr) {
		gem_err(drm_dev->dev, "share momory %s alloc failed. size %#llx\n",
			visible ? "visible" : "invisible", mem_size);
		ret = -ENOMEM;
		goto exit;
	}
	pshared_mem->cpu_paddr = visible ? fh2m_dev_paddr_to_cpu_paddr(drm_dev->dev, pshared_mem->dev_paddr) : 0;
	pshared_mem->is_visible = visible;
	pshared_mem->size = mem_size;

	gem_info(drm_dev->dev, "%s alloc multi-user shared memory ret-%d\nvisible-%d devaddr-%#llx, cpuaddr-%#llx, size is %#llx ret\n",
		drm_dev->driver->name, ret, pshared_mem->is_visible,
		pshared_mem->dev_paddr, pshared_mem->cpu_paddr);

exit:
	return ret;
}

void innodpu_gem_share_pre_free(struct drm_device *drm_dev,
	innodpu_shared_mem *pshared_mem)
{
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (pshared_mem->is_visible) {
		fh2m_hal_vram_free(drm_dev->dev, &dev_priv->role, pshared_mem->dev_paddr);
	} else {
		fh2m_hal_inv_vram_free(drm_dev->dev, &dev_priv->role, pshared_mem->dev_paddr);
	}

	return;
}
/* shared mem pre alloc end */

/* zero mem init start */
static void innodpu_zeromem_clear(struct work_struct *work)
{
	void __iomem *vaddr = NULL;
	innodpu_zero_gem *zero_mem = NULL;

	zero_mem = container_of(work, innodpu_zero_gem, gemclear_work);
	vaddr = (void __iomem *)fh2m_inno_ioremap_wc_portable(zero_mem->info.cpu_paddr, zero_mem->info.size);
	OSDeviceMemSet(vaddr, 0, zero_mem->info.size);
	fh2m_inno_iounmap(vaddr);
	zero_mem->is_ready = true;

	gem_info(zero_mem->drm_dev->dev, "zeromem init finish, size 0x%zx\n", zero_mem->info.size);
}

innodpu_zero_gem *innodpu_zero_mem_init(struct drm_device *drm_dev, uint64_t size)
{
	struct innodpu_drm_private *dev_priv = NULL;
	innodpu_zero_gem *zero_mem = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		goto err;
	}

	zero_mem = fh2m_inno_kzalloc_kernel(sizeof(innodpu_zero_gem));
	if (!zero_mem) {
		gem_err(drm_dev->dev, "zeromem struct init faild, short of memory.\n");
		goto err;
	}

	zero_mem->backup_buffer = fh2m_inno_vzalloc(ALIGN(size, PAGE_SIZE));
	if (!zero_mem->backup_buffer) {
		gem_err(drm_dev->dev, "zeromem buffer init faild, short of memory.\n");
		goto err;
	}

	zero_mem->info.dev_paddr = fh2m_hal_vram_alloc(drm_dev->dev, &dev_priv->role,
		true, ALIGN(size, PAGE_SIZE), 0);
	if (!zero_mem->info.dev_paddr) {
		gem_err(drm_dev->dev, "zeromem init faild, short of memory-size 0x%lx\n",
			ALIGN(size, PAGE_SIZE));
		goto err;
	}
	zero_mem->info.cpu_paddr = fh2m_dev_paddr_to_cpu_paddr(drm_dev->dev, zero_mem->info.dev_paddr);
	zero_mem->info.size = ALIGN(size, PAGE_SIZE);
	zero_mem->info.is_visible = true;
	zero_mem->drm_dev = drm_dev;

	INIT_WORK(&zero_mem->gemclear_work, innodpu_zeromem_clear);
	schedule_work(&zero_mem->gemclear_work);

	gem_info(drm_dev->dev, "zero init visible-%d dev_paddr-%#lx cpu_paddr-%#lx size-%d\n",
		true, zero_mem->info.dev_paddr, zero_mem->info.cpu_paddr, zero_mem->info.size);

	return zero_mem;

err:
	if (zero_mem) {
		if (zero_mem->backup_buffer) {
			fh2m_inno_vfree(zero_mem->backup_buffer);
		}
		fh2m_inno_kfree(zero_mem);
	}

	return NULL;
}

void innodpu_zero_mem_fini(struct drm_device *drm_dev, innodpu_zero_gem *zero_mem)
{
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	cancel_work_sync(&zero_mem->gemclear_work);
	fh2m_hal_vram_free(drm_dev->dev, &dev_priv->role, zero_mem->info.dev_paddr);
	fh2m_inno_vfree(zero_mem->backup_buffer);
	fh2m_inno_kfree(zero_mem);

	return;
}
/* zero mem init end */

/* pdp vga buffer func start */
static void innodpu_pdp_vga_buffer_draw(void *buffer, int size,
	unsigned int width, unsigned int height)
{
	uint32_t *point = (uint32_t *)(buffer);

	fh2m_inno_memset(buffer, 0, size);
	point[0] = 0xFFFFFFFF;
	point[width - 1] = 0xFFFFFFFF;
	point[(height - 1) * width] = 0xFFFFFFFF;
	point[(height * width) -1] = 0xFFFFFFFF;

	return;
}

bool innodpu_pdp_vga_buffer_set(innodpu_pdp_vga_gem *pdp_vga_gem,
	inno_dev* dev, unsigned int target_width, unsigned int target_height)
{
	unsigned int width = 0;
	unsigned int height = 0;
	void *dst = NULL, *src = NULL;
	int size = 0;

	width = target_width > pdp_vga_gem->max_width ? pdp_vga_gem->max_width : target_width;
	height = target_height > pdp_vga_gem->max_height ? pdp_vga_gem->max_height : target_height;
	size = width * height * 4;

	/* draw buffer, is has speical needs just mofify this func */
	innodpu_pdp_vga_buffer_draw(pdp_vga_gem->buffer, size, width, height);

	if (pdp_vga_gem->info.is_visible) {
		/* visible copy */
		src = pdp_vga_gem->buffer;
		dst = (void*)fh2m_cpu_paddr_to_pcie_paddr(dev, pdp_vga_gem->info.cpu_paddr);
		innodpu_dma_memcpy3(dev, &src, &dst, &size, 1, SYS2GDDR);
	} else {
		/* invisible copy */
		src = (void *)pdp_vga_gem->buffer;
		dst = (void *)pdp_vga_gem->info.dev_paddr;
		innodpu_dma_memcpy_for_smallbar_sg(dev, &src, &dst, &size, 1, SYS2GDDR);
	}

	return true;
}

innodpu_pdp_vga_gem *innodpu_pdp_vga_mem_init(struct drm_device *drm_dev)
{
	struct innodpu_drm_private *dev_priv = NULL;
	innodpu_pdp_vga_gem *pdp_vga_gem = NULL;
	unsigned long size = 0;
	bool visible = true;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return NULL;
	}

	pdp_vga_gem = fh2m_inno_kzalloc_kernel(sizeof(innodpu_pdp_vga_gem));
	if (!pdp_vga_gem) {
		gem_err(drm_dev->dev, "pdp_vga_gem struct init faild, short of memory.\n");
		goto err;
	}

	pdp_vga_gem->max_width = VGA_MAX_WIDTH;
	pdp_vga_gem->max_height = VGA_MAX_HEIGHT;
	size = pdp_vga_gem->max_width * pdp_vga_gem->max_height * 4;

	pdp_vga_gem->buffer = fh2m_inno_vzalloc(size);
	if (!pdp_vga_gem->buffer) {
		gem_err(drm_dev->dev, "alloc pdp vga draw buffer memory err size 0x%lx\n", size);
		goto err;
	}

	if (fh2m_hal_has_inv_mem(drm_dev->dev)) {
		visible = false;
	}

	pdp_vga_gem->info.dev_paddr = fh2m_hal_vram_alloc(drm_dev->dev, &dev_priv->role, visible, size, 0);
	if (!pdp_vga_gem->info.dev_paddr) {
		gem_err(drm_dev->dev, "pdp_vga_mem init faild, short of memory-size 0x%lx\n");
		goto err;
	}

	pdp_vga_gem->info.cpu_paddr = visible ? fh2m_dev_paddr_to_cpu_paddr(drm_dev->dev, pdp_vga_gem->info.dev_paddr) : 0;
	pdp_vga_gem->info.size = size;
	pdp_vga_gem->info.is_visible = visible;

	return pdp_vga_gem;

err:
	if (pdp_vga_gem) {
		if (pdp_vga_gem->buffer) {
			fh2m_inno_vfree(pdp_vga_gem->buffer);
		}

		fh2m_inno_kfree(pdp_vga_gem);
	}
	return NULL;
}

void innodpu_pdp_vga_mem_fini(struct drm_device *drm_dev, innodpu_pdp_vga_gem *pdp_vga_gem)
{
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (pdp_vga_gem->info.is_visible) {
		fh2m_hal_vram_free(drm_dev->dev, &dev_priv->role, pdp_vga_gem->info.dev_paddr);
	} else {
		fh2m_hal_inv_vram_free(drm_dev->dev, &dev_priv->role, pdp_vga_gem->info.dev_paddr);
	}

	if (pdp_vga_gem->buffer != NULL) {
		fh2m_inno_vfree(pdp_vga_gem->buffer);
	}

	fh2m_inno_kfree(pdp_vga_gem);

	return;
}
/* pdp vga mem fini end */

int innodpu_gem_vram_count(int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	struct drm_dpu_vram_count *m = (struct drm_dpu_vram_count *)data;

	if (innodpu_obj->class == GTT) {
		return 0;
	}

	if (innodpu_obj->mem_manager->visible) {
		m->visiable_vram_usage += obj->size;
	} else {
		m->invisiable_vram_usage += obj->size;
	}

	return 0;
}

int inno_gem_object_dump_vram_ioctl(struct drm_device *drm_dev,
		void *data, struct drm_file *drm_file)
{
	struct drm_dpu_vram_count *args = (struct drm_dpu_vram_count *)data;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		gem_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	gem_info(drm_dev->dev, "DRM_PDP_GEM_GET legacy,use DRM_PDP_CHIP_INFO instead\n");

	args->visiable_vram_size = dev_priv->visible_mem_manager->size;
	args->visiable_vram_usage = 0;

	mutex_lock(&drm_dev->object_name_lock);
	idr_for_each(&drm_dev->object_name_idr, innodpu_gem_vram_count, args);
	mutex_unlock(&drm_dev->object_name_lock);

	if (dev_priv->invisible_mem_manager) {
		args->invisiable_vram_size = dev_priv->invisible_mem_manager->size;
		args->invisiable_vram_usage = 0;
		mutex_lock(&drm_dev->object_name_lock);
		idr_for_each(&drm_dev->object_name_idr, innodpu_gem_vram_count, args);
		mutex_unlock(&drm_dev->object_name_lock);
	} else {
		args->invisiable_vram_size = 0;
		args->invisiable_vram_usage = 0;
	}
	return 0;
}

static int innodpu_gem_lookup_our_object(struct drm_file *drm_file,
	u32 handle, struct drm_gem_object **objp)
{
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(drm_file, handle);
	if (!obj)
		return -ENOENT;

	if (obj->import_attach) {
		/*
		 * The dmabuf associated with the object is not one of ours.
		 * Our own buffers are handled differently on import.
		 */
		drm_gem_object_put(obj);
		return -EINVAL;
	}

	*objp = obj;
	return 0;
}

int inno_gem_object_cpu_prep_ioctl(struct drm_device *dev, void *data, struct drm_file *drm_file)
{
	struct drm_pdp_gem_cpu_prep *args = (struct drm_pdp_gem_cpu_prep *)data;
	struct drm_gem_object *gem_obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	bool write = ! !(args->flags & PDP_GEM_CPU_PREP_WRITE);
	bool wait = !(args->flags & PDP_GEM_CPU_PREP_NOWAIT);
	bool cpu_write = write || !(args->flags & PDP_GEM_CPU_PREP_READ);
	struct gem_vm_list *pvm = NULL;
	int err = 0;

	if (args->flags & ~(PDP_GEM_CPU_PREP_READ | PDP_GEM_CPU_PREP_WRITE | PDP_GEM_CPU_PREP_NOWAIT)) {
		gem_err(dev->dev, "invalid flags: %#08x\n", args->flags);
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);
	err = innodpu_gem_lookup_our_object(drm_file, args->handle, &gem_obj);
	if (err)
		goto exit_unlock;

	innodpu_obj = to_innodpu_obj(gem_obj);
	gem_info(gem_obj->dev->dev, "Visible-%d  size-%#llx, paddr-%#llx, daddr-%#llx\n",
			innodpu_obj->mem_manager->visible, gem_obj->size,
			innodpu_obj->cpu_paddr, innodpu_obj->dev_paddr);

	if (innodpu_obj->cpu_prep) {
		err = -EBUSY;
		goto exit_unref;
	}

	if (wait) {
		long lerr;

		lerr = dma_resv_wait_timeout_rcu(innodpu_obj->resv, innodpu_dma_resv_usage_rw(write), true, 30 * HZ);
		if (!lerr)
			err = -EBUSY;
		else if (lerr < 0)
			err = lerr;
	} else {
		if (!dma_resv_test_signaled_rcu(innodpu_obj->resv, innodpu_dma_resv_usage_rw(write)))
			err = -EBUSY;
	}
	if (!err) {
		mutex_lock(&innodpu_obj->vm_lock);
		innodpu_obj->cpu_prep_write = cpu_write;
		innodpu_obj->cpu_prep = true;
		if (cpu_write)
			list_for_each_entry(pvm, &innodpu_obj->vm_head, list)
				pvm->cpu_write = true;
		mutex_unlock(&innodpu_obj->vm_lock);
	}

exit_unref:
	drm_gem_object_put(gem_obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);

	return err;
}

int inno_gem_object_cpu_fini_ioctl(struct drm_device *dev, void *data, struct drm_file *drm_file)
{
	struct drm_pdp_gem_cpu_fini *args = (struct drm_pdp_gem_cpu_fini *)data;
	struct drm_gem_object *gem_obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	int err = 0;

	if (args->pad) {
		gem_err(dev->dev, "invalid pad (this should always be 0)\n");
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);
	err = innodpu_gem_lookup_our_object(drm_file, args->handle, &gem_obj);
	if (err)
		goto exit_unlock;

	innodpu_obj = to_innodpu_obj(gem_obj);
	gem_info(gem_obj->dev->dev, "Visible-%d  size-%#llx, paddr-%#llx, daddr-%#llx\n",
			innodpu_obj->mem_manager->visible, gem_obj->size,
			innodpu_obj->cpu_paddr, innodpu_obj->dev_paddr);

	if (!innodpu_obj->cpu_prep) {
		err = -EINVAL;
		goto exit_unref;
	}

	mutex_lock(&innodpu_obj->vm_lock);
	innodpu_obj->cpu_prep = false;
	innodpu_obj->cpu_prep_write = true;
	mutex_unlock(&innodpu_obj->vm_lock);

exit_unref:
	drm_gem_object_put(gem_obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);

	return err;
}

int inno_gem_object_inv_get_ioctl(struct drm_device *dev, void *data, struct drm_file *drm_file)
{
	struct drm_dpu_gem_inv_get *args = (struct drm_dpu_gem_inv_get *)data;
	struct drm_gem_object *gem_obj = NULL;
	innodpu_gem_object *innodpu_obj = NULL;
	int err = 0;

	mutex_lock(&dev->struct_mutex);
	err = innodpu_gem_lookup_our_object(drm_file, args->handle, &gem_obj);
	if (err)
		goto exit_unlock;

	innodpu_obj = to_innodpu_obj(gem_obj);
	if (!err)
		args->isInv = !innodpu_obj->mem_manager->visible;

	drm_gem_object_put(gem_obj);
exit_unlock:
	mutex_unlock(&dev->struct_mutex);

	return err;
}

struct dma_resv *innodpu_gem_get_resv(struct drm_gem_object *obj)
{
	return (to_innodpu_obj(obj)->resv);
}

unsigned long fh2m_innodpu_gem_get_dev_paddr(struct drm_gem_object *obj)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	unsigned long dev_paddr = 0;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		dev_paddr = (unsigned long)innodpu_obj->dev_paddr;
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM &&
		innodpu_obj->pmr->base_array_size == 1) {
		dev_paddr =  (unsigned long)innodpu_obj->pmr->base_array[0];
	} else {
		fh2m_inno_dump_stack();
	}

	return dev_paddr;
}
INNO_EXT_SYM(fh2m_innodpu_gem_get_dev_paddr);

unsigned long innodpu_gem_get_dev_vaddr(struct drm_gem_object *obj)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	unsigned long dev_vaddr = 0;

	if (!s_dpu_support_smmu) {
		fh2m_inno_dump_stack();
		return dev_vaddr;
	}

	if (innodpu_obj->class == GTT) {
		/* todo support convert gtt addr by smmu */
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM) {
		/* todo support convert no continuos vram addr by smmu */
	} else {
		/* no support other class gem now */
		fh2m_inno_dump_stack();
	}

	return dev_vaddr;
}

unsigned long fh2m_innodpu_gem_get_cpu_paddr(struct drm_gem_object *obj)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	unsigned long cpu_paddr = 0;

	/* Memory Management :(GTT、visible vram、invisible vram)
	 * visibile、invisible vram：continue、no continue
	 * invisible vram cannot perform device-side to cpu-side address translation. */
	if (!innodpu_obj->mem_manager || !innodpu_obj->mem_manager->visible)
		return 0;

	if (innodpu_obj->class == CONTINUOUS_VRAM) {
		cpu_paddr = (unsigned long)innodpu_obj->cpu_paddr;
	} else if (innodpu_obj->class == NO_CONTINUOUS_VRAM &&
		innodpu_obj->pmr->base_array_size == 1) {
		cpu_paddr =  (unsigned long)fh2m_dev_paddr_to_cpu_paddr(innodpu_obj->base.dev->dev, innodpu_obj->pmr->base_array[0]);
	} else {
		fh2m_inno_dump_stack();
	}

	return cpu_paddr;
}
INNO_EXT_SYM(fh2m_innodpu_gem_get_cpu_paddr);

unsigned long fh2m_innodpu_gem_get_size_origin(struct drm_gem_object *obj)
{
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);

	return innodpu_obj->size_origin;
}
INNO_EXT_SYM(fh2m_innodpu_gem_get_size_origin);

const struct dma_buf_ops *fh2m_innodpu_gem_get_dma_buf_ops() {
	return &s_innodpu_gem_prime_dmabuf_ops;
}
INNO_EXT_SYM(fh2m_innodpu_gem_get_dma_buf_ops);
