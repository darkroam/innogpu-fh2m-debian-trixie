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

#include <linux/sched.h>
#include <linux/mm.h>
#include "inno_misc.h"
#include "inno_srvkm.h"
#include "inno_debug.h"
#include "img_types.h"
#include "inno_drm_version.h"
#include "inno_pmr.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0))
typedef int vm_fault_t;
#endif

#if defined(DEBUG)
IMG_UINT32 PMRAllocFail;

#if defined(__linux__)
#include <linux/moduleparam.h>

module_param(PMRAllocFail, uint, 0644);
MODULE_PARM_DESC(PMRAllocFail, "When number of PMR allocs reaches "
				 "this value, it will fail (default value is 0 which "
				 "means that alloc function will behave normally).");
#endif /* defined(__linux__) */
#endif /* defined(DEBUG) */


static void inno_pmr_mmap_open(struct vm_area_struct *vma)
{
	void *pmr = vma->vm_private_data;
	fh2m_inno_printk(KERN_INFO "%s: Unexpected mmap open call, this is probably an app bug", __func__);
	fh2m_inno_printk(KERN_INFO "%s: vma: 0x%p, vaddr: %#lx, len: %#lx, pmr: 0x%p",
			__func__, vma, vma->vm_start, vma->vm_end - vma->vm_start, pmr);

	fh2m_inno_pmr_ref(pmr);
	if (fh2m_inno_pmr_lock_sys_phys_addr(pmr) != 0) {
		fh2m_inno_printk(KERN_ERR "%s: could not lock phy addr", __func__);
		fh2m_inno_pmr_unref(pmr);
	}
}

static uint32_t __maybe_unused task_id(void)
{
	if (in_interrupt())
		return 0xffffffff;
	return task_tgid_nr(current);
}

static void inno_pmr_mmap_close(struct vm_area_struct *vma)
{
	void *pmr = vma->vm_private_data;
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined SUPPORT_PMR_DEFERRED_FREE
	const int MEM_ALLOC_TYPE_MAP_UMA_LMA = 10;
#else
	const int MEM_ALLOC_TYPE_MAP_UMA_LMA = 8;
#endif
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	{
		uintptr_t vaddr = vma->vm_start;
		while (vaddr < vma->vm_end) {
			fh2m_inno_stats_remove_mem_alloc_record(MEM_ALLOC_TYPE_MAP_UMA_LMA,
					(uint64_t)vaddr, task_id());
			vaddr += PAGE_SIZE;
		}
	}
#else
	fh2m_inno_stats_decr_mem_alloc_stat(MEM_ALLOC_TYPE_MAP_UMA_LMA,
			vma->vm_end - vma->vm_start, task_id());
#endif
#endif
	if (inno_pmr_is_inv(pmr)) {
		inno_pmr_flush(pmr);
	}
	fh2m_inno_pmr_unlock_sys_phys_addr(pmr);
	/* Decrement the mapping count before Unref of PMR (as Unref could destroy the PMR) */
	fh2m_inno_pmr_cpumapcount_dec(pmr);
	fh2m_inno_pmr_unref(pmr);
}

static int inno_pmr_mmap_access(struct vm_area_struct *vma, unsigned long addr,
		void *buf, int len, int write)
{
	void *pmr = vma->vm_private_data;
	unsigned long offset = addr - vma->vm_start;
	size_t bytes_copied;
	int err;
	int ret = -EINVAL;

	if (write)
		err = fh2m_inno_pmr_write_bytes(pmr, offset, buf, len, &bytes_copied);
	else
		err = fh2m_inno_pmr_read_bytes(pmr, offset, buf, len, &bytes_copied);

	if (err != 0)
		fh2m_inno_printk(KERN_ERR "%s: Error from %s (%d)", __func__, write ?
			"pmr_write_bytes" : "pmr_read_bytes", err);
	else
		ret = bytes_copied;

	return ret;
}

#if (DRM_VERSION < KERNEL_VERSION(4, 11, 0))
static vm_fault_t inno_pmr_mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
#else
static vm_fault_t inno_pmr_mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#endif
	int err;
	void *pmr = vma->vm_private_data;

	err = inno_pmr_handle_page_fault(pmr, vma, vmf);
	if (err < 0)
	{
		fh2m_inno_printk(KERN_ERR "%s: Error from %s (%d)", __func__, "inno_pmr_handle_page_fault", err);
		err = VM_FAULT_ERROR;
	}
 
	return err;
}

static const struct vm_operations_struct inno_pmr_mmap_ops = {
	.open = &inno_pmr_mmap_open,
	.close = &inno_pmr_mmap_close,
	.access = &inno_pmr_mmap_access,
	.fault = &inno_pmr_mmap_fault,
};

void fh2m_inno_set_vm_area_pmr_ops(void *vma)
{
	((struct vm_area_struct *)vma)->vm_ops = &inno_pmr_mmap_ops;
}
INNO_EXT_SYM(fh2m_inno_set_vm_area_pmr_ops);
