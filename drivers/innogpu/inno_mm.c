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
#include <linux/gfp.h>
#include <asm/page.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/pfn_t.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/shrinker.h>
#include <asm/io.h>
#include <linux/mmzone.h>
#include <linux/memblock.h>
#include <linux/kallsyms.h>
#include "inno_drm_version.h"
#if defined(CONFIG_X86)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0))
#include <asm/set_memory.h>
#else
#include <asm/cacheflush.h>
#endif
#endif

#include <linux/genalloc.h>
#include "inno_misc.h"
#include "inno_mm.h"
#include "inno_task.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
	#ifndef KYLIN_RELEASE_CODE
		#define mmap_write_lock(mm)   down_write(&mm->mmap_sem)
		#define mmap_write_unlock(mm) up_write(&mm->mmap_sem)

		#define mmap_read_lock(mm)    down_read(&mm->mmap_sem)
		#define mmap_read_unlock(mm)  up_read(&mm->mmap_sem)
	#endif
#endif

unsigned long fh2m_inno_page_size = PAGE_SIZE;
uint8_t fh2m_inno_page_shift = PAGE_SHIFT;
int fh2m_inno_page_struct_size = sizeof(struct page);
pgprot_t inno_page_kernel;
INNO_EXT_SYM(fh2m_inno_page_size);
INNO_EXT_SYM(fh2m_inno_page_shift);
INNO_EXT_SYM(fh2m_inno_page_struct_size);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
void *kvmalloc(size_t size, gfp_t flags)
{
		void *ret = NULL;
		ret = kmalloc(size, flags);
		if(!ret)
		{
			ret = vmalloc(size);
		}

		return ret;
}
#endif

void fh2m_inno_pgprot_init(void)
{
	inno_page_kernel = PAGE_KERNEL;
}
INNO_EXT_SYM(fh2m_inno_pgprot_init);

inno_pgprot *fh2m_inno_get_inno_page_kernel(void)
{
	return &inno_page_kernel;
}
INNO_EXT_SYM(fh2m_inno_get_inno_page_kernel);

unsigned long fh2m_inno_pgprot_val(inno_pgprot *pgprot)
{
	return pgprot_val(*(pgprot_t *)pgprot);
}
INNO_EXT_SYM(fh2m_inno_pgprot_val);

void *fh2m_inno_kmalloc_kernel(uint64_t size)
{
	return kmalloc(size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kmalloc_kernel);

void *fh2m_inno_krealloc_kernel(const void *p, size_t new_size)
{
	return krealloc(p, new_size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_krealloc_kernel);

void *fh2m_inno_kzalloc_kernel(uint64_t size)
{
	return kzalloc(size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kzalloc_kernel);

void *fh2m_inno_kmalloc_array(size_t n, size_t size)
{
	return kmalloc_array(n, size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kmalloc_array);

void *fh2m_inno_kzalloc_array(size_t n, size_t size)
{
	return kmalloc_array(n, size, GFP_KERNEL | __GFP_ZERO);
}
INNO_EXT_SYM(fh2m_inno_kzalloc_array);

void *fh2m_inno_kmalloc_page_array(size_t n)
{
	return fh2m_inno_kmalloc_array(n, sizeof(struct page *));
}
INNO_EXT_SYM(fh2m_inno_kmalloc_page_array);

void *fh2m_inno_kzalloc_page_array(size_t n)
{
	return fh2m_inno_kzalloc_array(n, sizeof(struct page *));
}
INNO_EXT_SYM(fh2m_inno_kzalloc_page_array);

void *fh2m_inno_kmalloc_atomic(uint64_t size)
{
	return kmalloc(size, GFP_ATOMIC);
}
INNO_EXT_SYM(fh2m_inno_kmalloc_atomic);

void fh2m_inno_kfree(const void *addr)
{
	kfree(addr);
}
INNO_EXT_SYM(fh2m_inno_kfree);

void *fh2m_inno_vmalloc(uint64_t size)
{
	return vmalloc(size);
}
INNO_EXT_SYM(fh2m_inno_vmalloc);

void *fh2m_inno_vzalloc(uint64_t size)
{
	return vzalloc(size);
}
INNO_EXT_SYM(fh2m_inno_vzalloc);
void fh2m_inno_vfree(const void *addr)
{
	vfree(addr);
}
INNO_EXT_SYM(fh2m_inno_vfree);

bool fh2m_inno_is_vmalloc_addr(const void *addr)
{
	return is_vmalloc_addr(addr);
}
INNO_EXT_SYM(fh2m_inno_is_vmalloc_addr);

bool fh2m_inno_is_kmalloc_addr(const void *addr)
{
	if(is_vmalloc_addr(addr))
	{
		return false;
	}
	else if (PAGE_OFFSET > (uint64_t)addr)
	{
		return false;
	}

	return true;
}
INNO_EXT_SYM(fh2m_inno_is_kmalloc_addr);

inno_page* fh2m_inno_vmalloc_to_page(const void *addr)
{
	return (inno_page*)vmalloc_to_page(addr);
}
INNO_EXT_SYM(fh2m_inno_vmalloc_to_page);

void fh2m_inno_set_page_reserved(inno_page *page)
{
	SetPageReserved(page);
}
INNO_EXT_SYM(fh2m_inno_set_page_reserved);

void fh2m_inno_clear_page_reserved(inno_page *page)
{
	ClearPageReserved(page);
}
INNO_EXT_SYM(fh2m_inno_clear_page_reserved);

void fh2m_inno_set_vmalloc_pages_reserved(void *addr, uint64_t size)
{
	int i = 0, page_cnt;
	uint64_t offset, start;
	struct page *page;

	if (!addr || !is_vmalloc_addr(addr))
		return;

	offset = (uint64_t)addr & ((1 << fh2m_inno_page_shift) - 1);
	page_cnt = (size + offset + fh2m_inno_page_size - 1) >> fh2m_inno_page_shift;

	for (start = (uint64_t)addr, i = 0; i < page_cnt; start += fh2m_inno_page_size, i++) {
		page = vmalloc_to_page((void *)start);
		if (page == NULL) {
			pr_err("vmalloc to page failed, addr: %#llx\n", start);
			break;
		}

		SetPageReserved(page);
	}
}
INNO_EXT_SYM(fh2m_inno_set_vmalloc_pages_reserved);

void fh2m_inno_clear_vmalloc_pages_reserved(void *addr, uint64_t size)
{
	int i = 0, page_cnt;
	uint64_t offset, start;
	struct page *page;

	if (!addr || !is_vmalloc_addr(addr))
		return;

	offset = (uint64_t)addr & ((1 << fh2m_inno_page_shift) - 1);
	page_cnt = (size + offset + fh2m_inno_page_size - 1) >> fh2m_inno_page_shift;

	for (start = (uint64_t)addr, i = 0; i < page_cnt; start += fh2m_inno_page_size, i++) {
		page = vmalloc_to_page((void *)start);
		if (page == NULL) {
			pr_err("vmalloc to page failed, addr: %#llx\n", start);
			break;
		}

		ClearPageReserved(page);
	}
}
INNO_EXT_SYM(fh2m_inno_clear_vmalloc_pages_reserved);

void *fh2m_inno_kvmalloc_kernel(uint64_t size)
{
	return kvmalloc(size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kvmalloc_kernel);

void fh2m_inno_kvfree(const void *addr)
{
	kvfree(addr);
}
INNO_EXT_SYM(fh2m_inno_kvfree);

void *fh2m_inno_devm_kmalloc_kernel(void *dev, uint64_t size)
{
	return devm_kmalloc((struct device *)dev, size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_devm_kmalloc_kernel);

void *fh2m_inno_devm_kzalloc_kernel(void *dev, uint64_t size)
{
	return devm_kzalloc((struct device *)dev, size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_devm_kzalloc_kernel);

void *fh2m_inno_devm_kcalloc_kernel(void *dev, uint64_t n, uint64_t size)
{
	return devm_kcalloc((struct device *)dev, n, size, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_devm_kcalloc_kernel);

void fh2m_inno_devm_kfree(void *dev, void *addr)
{
	devm_kfree((struct device *)dev, addr);
}
INNO_EXT_SYM(fh2m_inno_devm_kfree);

int fh2m_inno_devm_add_action(void *dev, void (*action)(void *), void *data)
{
	return devm_add_action((struct device *)dev, action, data);
}
INNO_EXT_SYM(fh2m_inno_devm_add_action);

uint64_t fh2m_inno_ksize(const void *addr)
{
	return ksize(addr);
}
INNO_EXT_SYM(fh2m_inno_ksize);

void *fh2m_inno_kmemdup_kernel(const void *src, uint64_t len)
{
	return kmemdup(src, len, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kmemdup_kernel);

uint64_t fh2m_inno_page_align(uint64_t addr)
{
	return PAGE_ALIGN(addr);
}
INNO_EXT_SYM(fh2m_inno_page_align);

struct list_head* fh2m_inno_page_lru(inno_page *page)
{
	return &(((struct page*)page)->lru);
}
INNO_EXT_SYM(fh2m_inno_page_lru);

uint32_t fh2m_inno_get_order(uint64_t size)
{
	return get_order(size);
}
INNO_EXT_SYM(fh2m_inno_get_order);

uint32_t fh2m_inno_compound_order(inno_page *page)
{
	return compound_order((struct page*)page);
}
INNO_EXT_SYM(fh2m_inno_compound_order);

inno_page *fh2m_inno_alloc_pages(uint32_t flags, uint32_t order)
{
	return alloc_pages(flags, order);
}
INNO_EXT_SYM(fh2m_inno_alloc_pages);

void fh2m_inno_page_set_mapping(inno_page *page, void* ptr)
{
	((struct page*)page)->mapping = (struct address_space *)ptr;
}
INNO_EXT_SYM(fh2m_inno_page_set_mapping);

inno_page *fh2m_inno_page_inc(inno_page *page, uint64_t order)
{
	return ((struct page*)page) + order;
}
INNO_EXT_SYM(fh2m_inno_page_inc);

gfp_t fh2m_inno_page_flag_comp(void)
{
	return __GFP_COMP;
}
INNO_EXT_SYM(fh2m_inno_page_flag_comp);

gfp_t fh2m_inno_page_flag_nowarn(void)
{
	return __GFP_NOWARN;
}
INNO_EXT_SYM(fh2m_inno_page_flag_nowarn);

gfp_t fh2m_inno_page_flag_kernel(void)
{
	return GFP_KERNEL;
}
INNO_EXT_SYM(fh2m_inno_page_flag_kernel);

inno_page *fh2m_inno_alloc_gtt_highmem_pages(uint32_t order)
{
	gfp_t gfp_flags = GFP_USER | __GFP_NOWARN | __GFP_NOMEMALLOC | __GFP_HIGHMEM;
	return alloc_pages(gfp_flags, order);
}
INNO_EXT_SYM(fh2m_inno_alloc_gtt_highmem_pages);

inno_page *fh2m_inno_virt_to_page(void *addr)
{
	return (inno_page *)virt_to_page(addr);
}
INNO_EXT_SYM(fh2m_inno_virt_to_page);

uint64_t fh2m_inno_virt_to_phys(void *addr)
{
	return (uint64_t)virt_to_phys(addr);
}
INNO_EXT_SYM(fh2m_inno_virt_to_phys);

unsigned long fh2m_inno_offset_in_page(void *addr)
{
	return offset_in_page(addr);
}
INNO_EXT_SYM(fh2m_inno_offset_in_page);

void fh2m_inno_split_page(inno_page *page, unsigned int order)
{
	split_page(page, order);
}
INNO_EXT_SYM(fh2m_inno_split_page);

uint32_t fh2m_inno_alloc_uma_flags(void *dev)
{
	struct device *_dev = (struct device *)dev;
	gfp_t flags = GFP_KERNEL;
	if (_dev) {
		if (*_dev->dma_mask == DMA_BIT_MASK(32))
			flags |= __GFP_DMA32;
		else if (*_dev->dma_mask < DMA_BIT_MASK(32))
			flags |= __GFP_DMA;
	}
	return flags;
}
INNO_EXT_SYM(fh2m_inno_alloc_uma_flags);

uint32_t fh2m_inno_alloc_osmem_flags(bool zero, void *_dev)
{
	struct device *dev = (struct device *)_dev;
	gfp_t flags = GFP_USER | __GFP_NOWARN | __GFP_NOMEMALLOC;

#if defined(PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY)
	/* Force use of HIGHMEM */
	flags |= __GFP_HIGHMEM;
#else
	if (dev) {
#if defined(CONFIG_64BIT) || defined(CONFIG_ARM_LPAE) || defined(CONFIG_X86_PAE)
		if (*dev->dma_mask > DMA_BIT_MASK(32)) {
			flags |= __GFP_HIGHMEM;
		} else if (*dev->dma_mask == DMA_BIT_MASK(32)) {
			flags |= __GFP_DMA32;
		} else {
			flags |= __GFP_DMA;
		}
#else
		if (*dev->dma_mask < DMA_BIT_MASK(32)) {
			flags |= __GFP_DMA;
		} else {
			flags |= __GFP_HIGHMEM;
		}
#endif
	}
#endif
	if (zero) {
		flags |= __GFP_ZERO;
	}

	return flags;
}
INNO_EXT_SYM(fh2m_inno_alloc_osmem_flags);

/* Get the GFP flags that we pass to the page allocator */
uint32_t fh2m_inno_alloc_osgtt_flags(bool zero, void *_dev)
{
	gfp_t gfp_flags = GFP_USER | __GFP_NOWARN | __GFP_NOMEMALLOC;
	gfp_flags &= (~__GFP_HARDWALL);
	/*
	 * This flag leads to continous memory reclamation,
	 * which is turn result in poor performance.
	 */
	gfp_flags &= (~__GFP_DIRECT_RECLAIM);
	/*注意：
	1、GFP_FLAG中是__GFP_HIGHMEM 和__GFP_DMA32，这两个不能组合在一起；
	2、当组合在一起时，会默认从__GFP_DMA区域去申请，导致最大只能申请16MB的物理内存
	*/
#if 0
	struct device *dev = (struct device *)_dev;

#if defined(PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY)
	/* Force use of HIGHMEM */
	gfp_flags |= __GFP_HIGHMEM;

	(void)dev;
#else
	if (dev)
	{
#if defined(CONFIG_64BIT) || defined(CONFIG_ARM_LPAE) || defined(CONFIG_X86_PAE)
		if (*dev->dma_mask > DMA_BIT_MASK(32))
		{
			/* If our system is able to handle large addresses use highmem */
			gfp_flags |= __GFP_HIGHMEM;
		}
		else if (*dev->dma_mask == DMA_BIT_MASK(32))
		{
			/* Limit to 32 bit.
			 * Achieved by setting __GFP_DMA32 for 64 bit systems */
			gfp_flags |= __GFP_DMA32;
		}
		else
		{
			/* Limit to size of DMA zone. */
			gfp_flags |= __GFP_DMA;
		}
#else
		if (*dev->dma_mask < DMA_BIT_MASK(32))
		{
			gfp_flags |= __GFP_DMA;
		}
		else
		{
			gfp_flags |= __GFP_HIGHMEM;
		}
#endif /* if defined(CONFIG_64BIT) || defined(CONFIG_ARM_LPAE) || defined(CONFIG_X86_PAE) */
	}

#endif /* if defined(PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY) */
#endif

	if (zero)
	{
		gfp_flags |= __GFP_ZERO;
	}

	/*强制从zone-dma32申请系统内存，这样申请到的物理内存地址一定在0-4G的范围内*/
	gfp_flags |= __GFP_DMA32;

	return gfp_flags;
}
INNO_EXT_SYM(fh2m_inno_alloc_osgtt_flags);

uint32_t fh2m_inno_gfp_reclaim(void)
{
	return __GFP_RECLAIM;
}
INNO_EXT_SYM(fh2m_inno_gfp_reclaim);

uint32_t fh2m_inno_gfp_noretry(void)
{
	return __GFP_NORETRY;
}
INNO_EXT_SYM(fh2m_inno_gfp_noretry);

void fh2m_inno_free_pages(inno_page *pages, uint32_t order)
{
	__free_pages((struct page *)pages, order);
}
INNO_EXT_SYM(fh2m_inno_free_pages);

uint64_t fh2m_inno_phys_to_pfn(uint64_t addr, uint64_t flags)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t pfnt =phys_to_pfn_t(addr, flags);
	return pfnt.val;
#else
	return addr >> PAGE_SHIFT;
#endif
}
INNO_EXT_SYM(fh2m_inno_phys_to_pfn);

uint64_t fh2m_inno_page_to_phys(inno_page *page)
{
	return page_to_phys((struct page *)(page));
}
INNO_EXT_SYM(fh2m_inno_page_to_phys);

inno_page *fh2m_inno_phys_to_page(uint64_t addr)
{
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	return phys_to_page(addr);
#else
	return NULL;
#endif
}
INNO_EXT_SYM(fh2m_inno_phys_to_page);

void *fh2m_inno_page_address(const inno_page *page)
{
	return page_address((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_page_address);

int fh2m_inno_set_memory_wb(unsigned long addr, int numpages)
{
#if defined(CONFIG_X86)
	return set_memory_wb(addr, numpages);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_memory_wb);

int fh2m_inno_set_memory_wc(unsigned long addr, int numpages)
{
#if defined(CONFIG_X86)
	return set_memory_wc(addr, numpages);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_memory_wc);

int fh2m_inno_set_memory_uc(unsigned long addr, int numpages)
{
#if defined(CONFIG_X86)
	return set_memory_uc(addr, numpages);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_memory_uc);

int fh2m_inno_set_pages_array_uc(inno_page **pages, int nums)
{
#if defined(CONFIG_X86)
	return set_pages_array_uc((struct page **)pages, nums);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_pages_array_uc);

int fh2m_inno_set_pages_array_wc(inno_page **pages, int nums)
{
#if defined(CONFIG_X86)
	return set_pages_array_wc((struct page **)pages, nums);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_pages_array_wc);

int fh2m_inno_set_pages_array_wb(inno_page **pages, int nums)
{
#if defined(CONFIG_X86)
	return set_pages_array_wb((struct page **)pages, nums);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_pages_array_wb);

void fh2m_inno_pgprot_noncached(inno_pgprot *old_prot, inno_pgprot *prot_res)
{
	pgprot_t *res = (pgprot_t *)prot_res;
	pgprot_t *prot = (pgprot_t *)old_prot;
	*res = pgprot_noncached(*prot);
}
INNO_EXT_SYM(fh2m_inno_pgprot_noncached);

void fh2m_inno_pgprot_cached(inno_pgprot *old_prot, inno_pgprot *prot_res)
{
#if defined(CONFIG_ARCH_EMEISWORD) || defined(CONFIG_CPU_RK3588)
	pgprot_t *res = (pgprot_t *)prot_res;
	pgprot_t *prot = (pgprot_t *)old_prot;
	*res = pgprot_noncached(*prot);
#endif
}
INNO_EXT_SYM(fh2m_inno_pgprot_cached);

void fh2m_inno_pgprot_writecombine(inno_pgprot *old_prot, inno_pgprot *prot_res)
{
	pgprot_t *res = (pgprot_t *)prot_res;
	pgprot_t *prot = (pgprot_t *)old_prot;
#if defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS) || defined(CONFIG_CPU_RK3588) || defined(CONFIG_ARCH_EMEISWORD)
	*res = pgprot_noncached(*prot);
#else
	*res = pgprot_writecombine(*prot);
#endif
}
INNO_EXT_SYM(fh2m_inno_pgprot_writecombine);

unsigned long fh2m_inno_get_vm_map_flag()
{
	return VM_MAP;
}
INNO_EXT_SYM(fh2m_inno_get_vm_map_flag);

void *fh2m_inno_vmap(inno_page **pages, unsigned int cnt, unsigned long flags, inno_pgprot *prot)
{
#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
	return vmap((struct page **)pages, cnt, flags, *((pgprot_t *)prot));
#elif (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
	return vm_map_ram((struct page **)pages, cnt, -1, *((pgprot_t *)prot));
#else
	if (pgprot_val(*((pgprot_t *)prot)) == pgprot_val(PAGE_KERNEL))
		return vm_map_ram((struct page **)pages, cnt, -1);
	else
		return vmap((struct page **)pages, cnt, flags, *((pgprot_t *)prot));
#endif
}
INNO_EXT_SYM(fh2m_inno_vmap);

void fh2m_inno_vunmap(void *addr, unsigned int count, inno_pgprot *prot)
{
#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
	vunmap(addr);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
	vm_unmap_ram(addr, count);
#else
	if (pgprot_val(*((pgprot_t *)prot)) == pgprot_val(PAGE_KERNEL))
		vm_unmap_ram(addr, count);
	else
		vunmap(addr);
#endif
}
INNO_EXT_SYM(fh2m_inno_vunmap);

inno_page *fh2m_inno_page_idx(inno_page* page, uint32_t index)
{
	return &(((struct page *)page)[index]);
}
INNO_EXT_SYM(fh2m_inno_page_idx);

void *fh2m_inno_kmap(inno_page *page)
{
	return kmap((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_kmap);

void fh2m_inno_kunmap(inno_page *page)
{
	kunmap(page);
}
INNO_EXT_SYM(fh2m_inno_kunmap);

void *fh2m_inno_kmap_atomic(inno_page *page)
{
	return kmap_atomic((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_kmap_atomic);

void fh2m_inno_kunmap_atomic(void *addr)
{
	kunmap_atomic(addr);
}
INNO_EXT_SYM(fh2m_inno_kunmap_atomic);

void fh2m_inno_current_mmap_write_lock(void)
{
	struct mm_struct *mm = current->mm;
	mmap_write_lock(mm);
}
INNO_EXT_SYM(fh2m_inno_current_mmap_write_lock);

void fh2m_inno_current_mmap_write_unlock(void)
{
	struct mm_struct *mm = current->mm;
	mmap_write_unlock(mm);
}
INNO_EXT_SYM(fh2m_inno_current_mmap_write_unlock);

void fh2m_inno_current_mmap_read_lock(void)
{
	struct mm_struct *mm = current->mm;
	mmap_read_lock(mm);
}
INNO_EXT_SYM(fh2m_inno_current_mmap_read_lock);

void fh2m_inno_current_mmap_read_unlock(void)
{
	struct mm_struct *mm = current->mm;
	mmap_read_unlock(mm);
}
INNO_EXT_SYM(fh2m_inno_current_mmap_read_unlock);

inno_vm_area *fh2m_inno_find_vma(uint64_t address)
{
	struct mm_struct *mm = current->mm;
	return find_vma(mm, address);
}
INNO_EXT_SYM(fh2m_inno_find_vma);

void fh2m_inno_unmap_mapping_range(inno_vm_area *vma, uint64_t addr, uint32_t size, int even_cows)
{
	struct address_space *mapping = ((struct vm_area_struct *)vma)->vm_file->f_mapping;
	unmap_mapping_range(mapping, addr, size, even_cows);
}
INNO_EXT_SYM(fh2m_inno_unmap_mapping_range);

uint64_t fh2m_inno_page_to_pfn(inno_page *page)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t pfn = page_to_pfn_t(page);
	return pfn.val;
#else
	return page_to_pfn(page);
#endif
}
INNO_EXT_SYM(fh2m_inno_page_to_pfn);

uint64_t fh2m_inno_pfn_down(uint64_t addr)
{
	return PFN_DOWN(addr);
}
INNO_EXT_SYM(fh2m_inno_pfn_down);

inno_page *fh2m_inno_pfn_to_page(uint64_t pfn)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t _pfn = { pfn };
	return pfn_t_to_page(_pfn);
#else
	return pfn_to_page(pfn);
#endif
}
INNO_EXT_SYM(fh2m_inno_pfn_to_page);

int fh2m_inno_pfn_valid(uint64_t pfn)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t _pfn = { pfn };
	return pfn_t_valid(_pfn);
#else
	return pfn_valid(pfn);
#endif
}
INNO_EXT_SYM(fh2m_inno_pfn_valid);

int fh2m_inno_page_count(inno_page *page)
{
	return page_count((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_page_count);

inno_mm_struct *fh2m_inno_get_current_mm_struct(void)
{
	return current->mm;
}
INNO_EXT_SYM(fh2m_inno_get_current_mm_struct);

void fh2m_inno_vm_area_pgoff_reset(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	vm->vm_pgoff = (vm->vm_start >> PAGE_SHIFT);
}
INNO_EXT_SYM(fh2m_inno_vm_area_pgoff_reset);

void fh2m_inno_vm_area_set_mixed_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	vm->vm_flags |= VM_MIXEDMAP;
#else
	vm_flags_set(((struct vm_area_struct *)vma), VM_MIXEDMAP);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_mixed_flag);

int fh2m_inno_vm_area_is_mixed_flag(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_flags & VM_MIXEDMAP;
}
INNO_EXT_SYM(fh2m_inno_vm_area_is_mixed_flag);

int fh2m_inno_vm_area_has_write_flag(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_flags & VM_WRITE;
}
INNO_EXT_SYM(fh2m_inno_vm_area_has_write_flag);

int fh2m_inno_vm_area_has_share_flag(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_flags & VM_SHARED;
}
INNO_EXT_SYM(fh2m_inno_vm_area_has_share_flag);

int fh2m_inno_vm_area_has_read_flag(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_flags & VM_READ;
}
INNO_EXT_SYM(fh2m_inno_vm_area_has_read_flag);

unsigned long fh2m_inno_vm_area_start(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_start;
}
INNO_EXT_SYM(fh2m_inno_vm_area_start);

unsigned long fh2m_inno_vm_area_end(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_end;
}
INNO_EXT_SYM(fh2m_inno_vm_area_end);

void fh2m_inno_vm_get_page_prot(inno_vm_area *vma, inno_pgprot * prot)
{
	*((pgprot_t *)prot) = vm_get_page_prot(((struct vm_area_struct *)vma)->vm_flags);
}
INNO_EXT_SYM(fh2m_inno_vm_get_page_prot);

void fh2m_inno_vm_set_page_prot(inno_vm_area *vma, inno_pgprot *prot)
{
	((struct vm_area_struct *)vma)->vm_page_prot = *((pgprot_t *)prot);
}
INNO_EXT_SYM(fh2m_inno_vm_set_page_prot);

void fh2m_inno_vm_area_set_io_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags |= VM_IO;
#else
	vm_flags_set(((struct vm_area_struct *)vma), VM_IO);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_io_flag);

void fh2m_inno_vm_area_set_dontdump_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags |= VM_DONTDUMP;
#else
	vm_flags_set((struct vm_area_struct *)vma, VM_DONTDUMP);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_dontdump_flag);

void fh2m_inno_vm_area_set_dontexpand_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags |= VM_DONTEXPAND;
#else
	vm_flags_set((struct vm_area_struct *)vma, VM_DONTEXPAND);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_dontexpand_flag);

void fh2m_inno_vm_area_set_dontcopy_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags |= VM_DONTCOPY;
#else
	vm_flags_set((struct vm_area_struct *)vma, VM_DONTCOPY);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_dontcopy_flag);

void fh2m_inno_vm_area_set_pfnmap_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags |= VM_PFNMAP;
#else
	vm_flags_set((struct vm_area_struct *)vma, VM_PFNMAP);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_pfnmap_flag);

void fh2m_inno_vm_area_unset_pfnmap_flag(inno_vm_area *vma)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	((struct vm_area_struct *)vma)->vm_flags &= ~VM_PFNMAP;
#else
	vm_flags_clear((struct vm_area_struct *)vma, VM_PFNMAP);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_area_unset_pfnmap_flag);

void fh2m_inno_vm_area_set_private_data(inno_vm_area *vma, void* data)
{
	((struct vm_area_struct *)vma)->vm_private_data = data;
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_private_data);

void* fh2m_inno_vm_area_get_private_data(inno_vm_area *vma)
{
	return ((struct vm_area_struct *)vma)->vm_private_data;
}
INNO_EXT_SYM(fh2m_inno_vm_area_get_private_data);

void fh2m_inno_vm_area_set_pgoff(inno_vm_area *vma, unsigned long pgoff)
{
	((struct vm_area_struct *)vma)->vm_pgoff = pgoff;
}
INNO_EXT_SYM(fh2m_inno_vm_area_set_pgoff);

unsigned long fh2m_inno_vm_area_len(inno_vm_area *vma)
{
	struct vm_area_struct *vm = (struct vm_area_struct *)vma;
	return vm->vm_end - vm->vm_start;
}
INNO_EXT_SYM(fh2m_inno_vm_area_len);

int fh2m_inno_vm_insert_mixed(inno_vm_area *vma, uint64_t addr, uint64_t pfn)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	pfn_t _pfn = { pfn };
	return vm_insert_mixed((struct vm_area_struct *)vma, addr, _pfn);
#elif defined(INNOGPU_VMF_INSERT_MIXED_PRESENT)
	vm_fault_t vmf;
	pfn_t _pfn = { pfn };
	vmf = vmf_insert_mixed((struct vm_area_struct *)vma, addr, _pfn);
	if (vmf & VM_FAULT_ERROR) {
		return vm_fault_to_errno(vmf, 0);
	} else {
		return 0;
	}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t _pfn = { pfn };
	return vm_insert_mixed((struct vm_area_struct *)vma, addr, _pfn);
#else
	return vm_insert_mixed((struct vm_area_struct *)vma, addr, pfn);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_insert_mixed);

int fh2m_inno_vm_insert_page(inno_vm_area *vma, uint64_t addr, inno_page *page)
{
	return vm_insert_page((struct vm_area_struct *)vma, addr, (struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_vm_insert_page);

int fh2m_inno_vm_insert_pfn(inno_vm_area *vma, unsigned long addr, unsigned long pfn)
{
#if DRM_VERSION >= KERNEL_VERSION(4,20,0)
	return vmf_insert_pfn((struct vm_area_struct *)vma, addr, pfn);
#else
	return vm_insert_pfn((struct vm_area_struct *)vma, addr, pfn);
#endif
}
INNO_EXT_SYM(fh2m_inno_vm_insert_pfn);

unsigned long fh2m_inno_vm_get_start(inno_vm_area *vma)
{
	return (unsigned long)((struct vm_area_struct *)vma)->vm_start;
}
INNO_EXT_SYM(fh2m_inno_vm_get_start);

unsigned long fh2m_inno_vmf_get_address(inno_vm_fault *vmf)
{
	unsigned long vmf_address;
#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
	vmf_address = (unsigned long)(((struct vm_fault *)vmf)->virtual_address);
#else
	vmf_address = ((struct vm_fault *)vmf)->address;
#endif
	return vmf_address;
}
INNO_EXT_SYM(fh2m_inno_vmf_get_address);

void fh2m_inno_vmf_set_page(inno_vm_fault *vmf, inno_page *page)
{
	((struct vm_fault *)vmf)->page = page;
}
INNO_EXT_SYM(fh2m_inno_vmf_set_page);

int fh2m_inno_set_page_dirty_lock(inno_page *page)
{
	return set_page_dirty_lock((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_set_page_dirty_lock);

int fh2m_inno_set_page_dirty(inno_page *page)
{
	return set_page_dirty((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_set_page_dirty);

void fh2m_inno_put_page(inno_page *page)
{
	put_page((struct page *)page);
}
INNO_EXT_SYM(fh2m_inno_put_page);

void fh2m_inno_put_user_pages(inno_page **pages, unsigned long npages)
{
	unsigned long index;
	for (index = 0; index < npages; index++)
		fh2m_inno_put_page(pages[index]);
}
INNO_EXT_SYM(fh2m_inno_put_user_pages);

void fh2m_inno_get_page(inno_page *page)
{
	get_page(page);
}
INNO_EXT_SYM(fh2m_inno_get_page);

int fh2m_inno_get_user_pages_fast(uint64_t addr, int nr_pages, int write, inno_page **pages)
{
	return get_user_pages_fast(addr, nr_pages, write, (struct page **)pages);
}
INNO_EXT_SYM(fh2m_inno_get_user_pages_fast);

long fh2m_inno_get_user_pages(uint64_t addr, int nr_pages, unsigned int flag, inno_page **pages, void* vmas)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0))
	return get_user_pages(addr, nr_pages, flag, (struct page **)pages, (struct vm_area_struct **)vmas);
#else
	return get_user_pages(addr, nr_pages, flag, (struct page **)pages);
#endif
}
INNO_EXT_SYM(fh2m_inno_get_user_pages);

int fh2m_inno_cache_line_size(void)
{
	return cache_line_size();
}
INNO_EXT_SYM(fh2m_inno_cache_line_size);

void *fh2m_inno_ioremap(uint64_t addr, uint64_t size)
{
	return ioremap(addr, size);
}
INNO_EXT_SYM(fh2m_inno_ioremap);

void *fh2m_inno_ioremap_nocache(uint64_t addr, uint64_t size)
{
#ifdef INNOGPU_IOREMAP_NOCACHE_PRESENT
	return ioremap_nocache(addr, size);
#else
	return ioremap(addr, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_ioremap_nocache);

void *fh2m_inno_ioremap_cache(uint64_t addr, uint64_t size)
{
#ifdef INNOGPU_IOREMAP_CACHE_PRESENT
	return ioremap_cache(addr, size);
#else
	return ioremap(addr, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_ioremap_cache);

void *fh2m_inno_ioremap_wc(uint64_t addr, uint64_t size)
{
	return ioremap_wc(addr, size);
}
INNO_EXT_SYM(fh2m_inno_ioremap_wc);

void *fh2m_inno_ioremap_wc_portable(uint64_t addr, uint64_t size)
{
#if defined(CONFIG_X86) || defined(CONFIG_ARM) ||(defined(CONFIG_ARM64) && !defined(CONFIG_CPU_RK3588) && !defined(CONFIG_ARCH_EMEISWORD)) || defined(CONFIG_SW64)
	return ioremap_wc(addr, size);
#elif defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)
	/*fix:loongarch not support wc!!!*/
#ifdef INNOGPU_IOREMAP_NOCACHE_PRESENT
	return ioremap_nocache(addr, size);
#else
	return ioremap(addr, size);
#endif
#else
	return ioremap(addr, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_ioremap_wc_portable);

void fh2m_inno_iounmap(void *ptr)
{
	iounmap(ptr);
}
INNO_EXT_SYM(fh2m_inno_iounmap);

void fh2m_inno_dump_stack(void)
{
	dump_stack();
}
INNO_EXT_SYM(fh2m_inno_dump_stack);

int fh2m_inno_access_ok(const void __user *addr, unsigned long size)
{
#if(LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	return access_ok(0, addr, size);
#elif !defined(INNOGPU_ACCESS_OK_2PARAM_PRESENT)
	return access_ok(0, addr, size);
#else
	return access_ok(addr, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_access_ok);

unsigned long fh2m_inno_copy_from_user(void *to, const void __user *from, unsigned long bytes)
{
#if(LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	if (access_ok(VERIFY_READ, from, bytes))
#elif !defined(INNOGPU_ACCESS_OK_2PARAM_PRESENT)
	if (access_ok(VERIFY_READ, from, bytes))
#else
	if (access_ok(from, bytes))
#endif
	{
		return __copy_from_user(to, from, bytes);
	}

	return bytes;
}
INNO_EXT_SYM(fh2m_inno_copy_from_user);

unsigned long fh2m_inno_copy_to_user(void __user *to, const void *from, unsigned long bytes)
{
#if(LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	if (access_ok(VERIFY_READ, to, bytes))
#elif !defined(INNOGPU_ACCESS_OK_2PARAM_PRESENT)
	if (access_ok(VERIFY_WRITE, to, bytes))
#else
	if (access_ok(to, bytes))
#endif
	{
		return __copy_to_user(to, from, bytes);
	}

	return bytes;
}
INNO_EXT_SYM(fh2m_inno_copy_to_user);

inno_kmem_cache *fh2m_inno_kmem_cache_create(const char *name, unsigned int size, unsigned int align, unsigned int flags, void (*ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}
INNO_EXT_SYM(fh2m_inno_kmem_cache_create);

void fh2m_inno_kmem_cache_destroy(inno_kmem_cache *s)
{
	kmem_cache_destroy((struct kmem_cache *)s);
}
INNO_EXT_SYM(fh2m_inno_kmem_cache_destroy);

void *fh2m_inno_kmem_cache_alloc_kernel(inno_kmem_cache *s)
{
	return kmem_cache_alloc((struct kmem_cache *)s, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_kmem_cache_alloc_kernel);

void fh2m_inno_kmem_cache_free(inno_kmem_cache *s, void *objp)
{
	kmem_cache_free((struct kmem_cache *)s, objp);
}
INNO_EXT_SYM(fh2m_inno_kmem_cache_free);

void *fh2m_inno_dma_alloc_coherent(void *dev, uint64_t size, dma_addr_t *dma_handle, uint32_t gfp)
{
	return dma_alloc_coherent((struct device *)dev, size, dma_handle, gfp);
}
INNO_EXT_SYM(fh2m_inno_dma_alloc_coherent);

void *fh2m_inno_dma_alloc_coherent_kernel(void *dev, uint64_t size, dma_addr_t *dma_handle)
{
	return dma_alloc_coherent((struct device *)dev, size, dma_handle, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_dma_alloc_coherent_kernel);

void fh2m_inno_dma_free_coherent(void *dev, uint64_t size, void *cpu_addr, dma_addr_t dma_handle)
{
	dma_free_coherent((struct device *)dev, size, cpu_addr, dma_handle);
}
INNO_EXT_SYM(fh2m_inno_dma_free_coherent);

void fh2m_inno_get_os_ram_stats(uint64_t *total_size, uint64_t *free_size)
{
	struct sysinfo meminfo;
	si_meminfo(&meminfo);

	*total_size = meminfo.totalram * meminfo.mem_unit;
	*free_size = meminfo.freeram * meminfo.mem_unit;
}
INNO_EXT_SYM(fh2m_inno_get_os_ram_stats);

#if !defined(INNOGPU_SHRINKER_REGISTER_PRESENT)
struct inno_shrinker {
	struct shrinker s;
	unsigned long (*count_objects)(inno_shrinker_t *, inno_shrink_control *sc);
	unsigned long (*scan_objects)(inno_shrinker_t *, inno_shrink_control *sc);
};

static unsigned long inno_shrinke_count_wrapper(struct shrinker *s,
		struct shrink_control *sc)
{
	struct inno_shrinker *shrinker = container_of(s, struct inno_shrinker, s);
	return shrinker->count_objects(shrinker, sc);
}

static unsigned long inno_shrinke_scan_wrapper(struct shrinker *s,
		struct shrink_control *sc)
{
	struct inno_shrinker *shrinker = container_of(s, struct inno_shrinker, s);
	return shrinker->scan_objects(shrinker, sc);
}
#endif

inno_shrinker_t *fh2m_inno_register_shrinker(
		unsigned long (*count_objects)(inno_shrinker_t *, inno_shrink_control *sc),
		unsigned long (*scan_objects)(inno_shrinker_t *, inno_shrink_control *sc))
{
#if defined(INNOGPU_SHRINKER_REGISTER_PRESENT)
	struct shrinker * shrinker = shrinker_alloc(0, "inno_shrinker");
	if (!shrinker)
		return NULL;
	shrinker->count_objects = (unsigned long (*)(struct shrinker *, struct shrink_control *sc))count_objects;
	shrinker->scan_objects = (unsigned long (*)(struct shrinker *, struct shrink_control *sc))scan_objects;
	shrinker->seeks = DEFAULT_SEEKS;
#else
	struct inno_shrinker *shrinker = (struct inno_shrinker *)kzalloc(
			sizeof(struct inno_shrinker), GFP_KERNEL);
	if (!shrinker)
		return NULL;
	shrinker->s.count_objects = inno_shrinke_count_wrapper;
	shrinker->s.scan_objects = inno_shrinke_scan_wrapper;
	shrinker->count_objects = count_objects;
	shrinker->scan_objects = scan_objects;
	shrinker->s.seeks = DEFAULT_SEEKS;
#endif

#if defined(INNOGPU_SHRINKER_REGISTER_PRESENT)
	shrinker_register(shrinker);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0))
	register_shrinker(&shrinker->s, "inno_shrinker");
#else
	register_shrinker(&shrinker->s);
#endif
	return shrinker;
}
INNO_EXT_SYM(fh2m_inno_register_shrinker);

void fh2m_inno_unregister_shrinker(inno_shrinker_t *shrinker)
{
#if defined(INNOGPU_SHRINKER_REGISTER_PRESENT)
	shrinker_free(shrinker);
#else
	unregister_shrinker(&((struct inno_shrinker *)shrinker)->s);
	kfree(shrinker);
#endif
}
INNO_EXT_SYM(fh2m_inno_unregister_shrinker);

unsigned long fh2m_inno_get_shrinker_obj_nr_to_scan(inno_shrink_control *sc)
{
	return ((struct shrink_control *)sc)->nr_to_scan;
}
INNO_EXT_SYM(fh2m_inno_get_shrinker_obj_nr_to_scan);

int fh2m_inno_remap_pfn_range(inno_vm_area *vma, unsigned long addr,
	unsigned long pfn, unsigned long size)
{
	return remap_pfn_range((struct vm_area_struct *)vma, addr, pfn, size,
		((struct vm_area_struct *)vma)->vm_page_prot);
}
INNO_EXT_SYM(fh2m_inno_remap_pfn_range);

bool fh2m_inno_is_valid_pmr_vaddr(inno_vm_area *vma, unsigned long vaddr,
		unsigned long size, void *pmr)
{
	struct vm_area_struct *v = vma;
	return (v->vm_start <= vaddr)     &&
		((vaddr + size) <= v->vm_end) &&
		(v->vm_private_data == pmr);
}
INNO_EXT_SYM(fh2m_inno_is_valid_pmr_vaddr);

void fh2m_inno_set_vm_area_priv(inno_vm_area *vma, void *priv)
{
	((struct vm_area_struct *)vma)->vm_private_data = priv;
}
INNO_EXT_SYM(fh2m_inno_set_vm_area_priv);

void *fh2m_inno_memset(void *s, int c, size_t count)
{
	return memset(s, c, count);
}
INNO_EXT_SYM(fh2m_inno_memset); /* Always export symbol fh2m_inno_memset */

void *fh2m_inno_memcpy(void *dest, const void *src, size_t count)
{
	return memcpy(dest, src, count);
}
INNO_EXT_SYM(fh2m_inno_memcpy);

void fh2m_inno_memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
	memcpy_fromio(to, from, count);
}
INNO_EXT_SYM(fh2m_inno_memcpy_fromio);

void fh2m_inno_memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
	memcpy_toio(to, from, count);
}
INNO_EXT_SYM(fh2m_inno_memcpy_toio);

void fh2m_inno_memset_io(volatile void __iomem *a, int b, size_t c)
{
	memset_io(a, b, c);
}
INNO_EXT_SYM(fh2m_inno_memset_io);

/*
 * Note:use this interface when accessing non system memory,
 * on some special platforms such as SW,
 * using memset to access graphics memory may result in errors
 * */
void fh2m_inno_memset_io_portable(void *s, int c, size_t count)
{
#if defined(CONFIG_X86) || defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)
	memset(s, c, count);
#else
	memset_io(s, c, count);
#endif
}
INNO_EXT_SYM(fh2m_inno_memset_io_portable);

#if defined(CONFIG_ZONE_DMA32)
unsigned long fh2m_inno_get_dma32_zone_pages(void)
{
	int nid = 0;
	pg_data_t *pgdat;
	int zone_index;
	unsigned long total_pages = 0;

	/* Per numa node */
	for_each_online_node(nid)
	{
		pgdat = NODE_DATA(nid);
		/* Per numa node's per zone */
		for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++)
		{
			struct zone *zone = pgdat->node_zones + zone_index;
			/* get dma32 mem zone's pages */
			if (zone_idx(zone) == ZONE_DMA32)
#if (DRM_VERSION >= KERNEL_VERSION(4,20,0))
				total_pages += zone_managed_pages(zone);
#else
				total_pages += zone->managed_pages;
#endif
		}
	}
	return total_pages;
}
#else
unsigned long fh2m_inno_get_dma32_zone_pages(void)
{
	return 0;
}
#endif

INNO_EXT_SYM(fh2m_inno_get_dma32_zone_pages);

unsigned long fh2m_inno_get_normal_high_zone_total_pages(void)
{
	int nid = 0;
	pg_data_t *pgdat;
	int zone_index;
	unsigned long total_high_pages = 0;
	unsigned long total_normal_pages = 0;

	/* Per numa node */
	for_each_online_node(nid)
	{
		pgdat = NODE_DATA(nid);
		/* Per numa node's per zone */
		for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++)
		{
			struct zone *zone = pgdat->node_zones + zone_index;
			/* get high mem zone's pages */
			if (is_highmem(zone))
#if (DRM_VERSION >= KERNEL_VERSION(4,20,0))
				total_high_pages += zone_managed_pages(zone);
#else
				total_high_pages += zone->managed_pages;
#endif

			/* Get normal mem zone's pages */
			if (zone_idx(zone) == ZONE_NORMAL)
#if (DRM_VERSION >= KERNEL_VERSION(4,20,0))
				total_normal_pages += zone_managed_pages(zone);
#else
				total_normal_pages += zone->managed_pages;
#endif

		}
	}
	return total_high_pages + total_normal_pages;
}
INNO_EXT_SYM(fh2m_inno_get_normal_high_zone_total_pages);

uint64_t fh2m_inno_get_zonedma32_totalsize(void)
{
#ifdef CONFIG_ZONE_DMA32
	struct zone *zone;
	int zone_index;
	unsigned long totalsize = 0;
	unsigned long total_managed_pages = 0;

	int nid;
	pg_data_t *pgdat;
	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++) {
			zone = pgdat->node_zones + zone_index;
			if (zone_idx(zone) == ZONE_DMA32) {
#if (DRM_VERSION >= KERNEL_VERSION(4,20,0))
				total_managed_pages += zone_managed_pages(zone);
#else
				total_managed_pages += zone->managed_pages;
#endif
			}
		}
	}
	totalsize = total_managed_pages * fh2m_inno_page_size;
	return totalsize;
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_get_zonedma32_totalsize);

uint64_t fh2m_inno_get_zonedma32_freesize(void)
{
#ifdef CONFIG_ZONE_DMA32
	int zone_index;
	struct zone *zone;
	unsigned long freesize = 0;

	int nid;
	struct pglist_data *pgdat;
	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++) {
			zone = pgdat->node_zones + zone_index;
			if (zone_idx(zone) == ZONE_DMA32) {
				freesize += zone_page_state(zone, NR_FREE_PAGES);
			}
		}
	}

	return freesize * fh2m_inno_page_size;
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_get_zonedma32_freesize);

bool fh2m_inno_mem_init_on_alloc(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,3,0))
/* Check both config and modparam setting */
#if PVRSRV_USE_LINUX_CONFIG_INIT_ON_ALLOC == 1
	return want_init_on_alloc(0x0);

/* Assume modparam setting not in use on system */
#elif PVRSRV_USE_LINUX_CONFIG_INIT_ON_ALLOC == 2
#   if defined(CONFIG_INIT_ON_ALLOC_DEFAULT_ON)
	return true;
#   else
	return false;
#   endif

/* Ignore both config and modparam settings */
#else
	return false;
#endif
#endif
	return false;
}
INNO_EXT_SYM(fh2m_inno_mem_init_on_alloc);

bool fh2m_inno_os_phys_non_contig_supported(void)
{
#if 1
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)) && !defined(CONFIG_VMAP_PFN)
	return false;
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0))
	return false;
#else
	return true;
#endif
#else
	/*
	 * if you unknowingly use non-contig allocation policy, may cause an MMU fault or DMA access to invisible memory,
	 * beacuse its address is not linear
	*/
	return false;
#endif
}
INNO_EXT_SYM(fh2m_inno_os_phys_non_contig_supported);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0) && defined(INNOGPU_UNMAP_KERNEL_RANGE_NOPRESENT))
	#include <linux/kprobes.h>
	static struct kprobe kp = {
		.symbol_name = "unmap_kernel_range"
	};
#endif

void fh2m_inno_os_unmap_phys_array_to_lin(void *addr, void *priv_data)
{
#if defined(CONFIG_VMAP_PFN)
		(void)priv_data;
		vunmap(addr);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
#if defined(INNOGPU_UNMAP_KERNEL_RANGE_NOPRESENT)
	#include <linux/kallsyms.h>
	typedef void (*unmap_kernel_range_ptr_t) (unsigned long addr, unsigned long size);
	static unmap_kernel_range_ptr_t unmap_kernel_range_ptr = NULL;

	if (!unmap_kernel_range_ptr) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0 )
			unmap_kernel_range_ptr = (unmap_kernel_range_ptr_t)kallsyms_lookup_name("unmap_kernel_range");
#else
			register_kprobe(&kp);
			unmap_kernel_range_ptr = (unmap_kernel_range_ptr_t)kp.addr;
			unregister_kprobe(&kp);
#endif
			if (unmap_kernel_range_ptr) {
				unmap_kernel_range_ptr((unsigned long) (uintptr_t) addr,
					get_vm_area_size(priv_data));
			}else{
				printk(KERN_ERR"%s: Cannot map into kernel, no method supported.", __func__);
				fh2m_inno_warn_on(1);
			}
		}
#else
				unmap_kernel_range((unsigned long) (uintptr_t) addr,
						get_vm_area_size(priv_data));
#endif
		free_vm_area(priv_data);
#else
		printk(KERN_ERR"%s: Cannot map into kernel, no method supported.", __func__);
		fh2m_inno_warn_on(1);
#endif
}
INNO_EXT_SYM(fh2m_inno_os_unmap_phys_array_to_lin);

void* fh2m_inno_os_map_phys_array_to_lin(CPU_INNO_PHYADDR* page_phy_addr, uint32_t pages_count, void ** pp_vlin_addr, void **pp_vpriv_data)
{
#if defined(CONFIG_VMAP_PFN)
	{
		uint32_t i;

		for (i = 0; i < pages_count; i++)
		{
			page_phy_addr[i].ui_addr = (page_phy_addr[i].ui_addr >> PAGE_SHIFT);
		}

		*pp_vlin_addr = vmap_pfn((unsigned long *)page_phy_addr,
							   (unsigned int)pages_count,
							   pgprot_device(PAGE_KERNEL));
		if (NULL != *pp_vlin_addr)
		{
			*pp_vpriv_data = NULL;
		}

		return (*pp_vlin_addr);
	}
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	{
		pte_t *pte[32], **pte_array;
		struct vm_struct *ps_vma;
		uint32_t i = 0;

		pte_array = &pte[0];
		if (sizeof(pte) < (sizeof(pte[0]) * pages_count))
		{
			pte_array = kzalloc(pages_count * sizeof(*pte_array), GFP_KERNEL);
			if (NULL == pte_array)
			{
				return NULL;
			}
		}

		ps_vma = alloc_vm_area((size_t)(pages_count << PAGE_SHIFT), pte_array);
		if (NULL == ps_vma)
		{
			*pp_vlin_addr = NULL;
			goto FreePTEArray;
		}

		for (i = 0; i < pages_count; i++)
		{
			*(pte_array[i]) = pte_mkspecial(pfn_pte((unsigned long) (page_phy_addr[i].ui_addr >> PAGE_SHIFT),
													pgprot_device(PAGE_KERNEL)));
		}

		smp_mb();

		*pp_vlin_addr = ps_vma->addr;
		*pp_vpriv_data = ps_vma;

FreePTEArray:
		if (pte_array != pte)
		{
			kfree(pte_array);
		}

		return (*pp_vlin_addr);
	}
#else
	printk(KERN_ERR "%s: Cannot map into kernel, no method supported.", __func__);
	fh2m_inno_warn_on(1);
	*pp_vlin_addr = NULL;
	return (*pp_vlin_addr);
#endif
}
INNO_EXT_SYM(fh2m_inno_os_map_phys_array_to_lin);

void fh2m_inno_set_prot_default_page_kernel(inno_pgprot *prot)
{
	*((pgprot_t *)prot) = PAGE_KERNEL;
}
INNO_EXT_SYM(fh2m_inno_set_prot_default_page_kernel);

void fh2m_inno_flush_cache_range(inno_vm_area* vma, unsigned long addr_start, unsigned long addr_end)
{
	flush_cache_range((struct vm_area_struct*)vma, addr_start, addr_end);
}
INNO_EXT_SYM(fh2m_inno_flush_cache_range);

unsigned long fh2m_inno_gen_pool_alloc(inno_gen_pool * pool, size_t size)
{
    return gen_pool_alloc((struct gen_pool *)pool, size);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_alloc);

int fh2m_inno_gen_pool_add_virt(inno_gen_pool * pool, unsigned long virt_start, phys_addr_t phys, size_t size, int nid)
{
    return gen_pool_add_virt((struct gen_pool *)pool, virt_start, phys, size, nid);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_add_virt);

void fh2m_inno_gen_pool_free(inno_gen_pool * pool, unsigned long start, size_t size)
{
    return gen_pool_free((struct gen_pool *)pool, start, size);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_free);

inno_gen_pool *fh2m_inno_gen_pool_create(int order, int nid)
{
    return gen_pool_create(order, nid);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_create);

phys_addr_t fh2m_inno_gen_pool_virt_to_phys(inno_gen_pool * pool, unsigned long start)
{
    return gen_pool_virt_to_phys(pool, start);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_virt_to_phys);

int fh2m_inno_gen_pool_add(inno_gen_pool * pool, unsigned long addr, size_t size, int nid)
{
	return gen_pool_add((struct gen_pool *)pool, addr, size, nid);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_add);

void fh2m_inno_gen_pool_destroy(inno_gen_pool * pool)
{
    gen_pool_destroy((struct gen_pool *)pool);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_destroy);

unsigned long fh2m_inno_gen_pool_chunk_start_addr(inno_gen_pool_chunk * chunk)
{
	return ((struct gen_pool_chunk *)chunk)->start_addr;
}
INNO_EXT_SYM(fh2m_inno_gen_pool_chunk_start_addr);

unsigned long fh2m_inno_gen_pool_chunk_end_addr(inno_gen_pool_chunk * chunk)
{
	return ((struct gen_pool_chunk *)chunk)->end_addr;
}
INNO_EXT_SYM(fh2m_inno_gen_pool_chunk_end_addr);

unsigned long fh2m_inno_gen_pool_avail_size(inno_gen_pool * pool)
{
	return gen_pool_avail((struct gen_pool *)pool);
}
INNO_EXT_SYM(fh2m_inno_gen_pool_avail_size);

inno_page *fh2m_inno_alloc_pages_by_node(uint32_t node_id, uint32_t flags, uint32_t order)
{
	return alloc_pages_node(node_id, flags, order);
}
INNO_EXT_SYM(fh2m_inno_alloc_pages_by_node);

uint64_t fh2m_inno_get_zone_normal_totalsize_by_node(uint32_t node_id)
{
	struct zone *zone;
	int zone_index;
	unsigned long totalsize = 0;
	unsigned long total_managed_pages = 0;

	pg_data_t *pgdat;

	pgdat = NODE_DATA(node_id);
	for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++) {
		zone = pgdat->node_zones + zone_index;
		if (zone_idx(zone) == ZONE_NORMAL) {
#if (DRM_VERSION >= KERNEL_VERSION(4,20,0))
			total_managed_pages += zone_managed_pages(zone);
#else
			total_managed_pages += zone->managed_pages;
#endif
		}
	}
	totalsize = total_managed_pages * fh2m_inno_page_size;
	return totalsize;
}
INNO_EXT_SYM(fh2m_inno_get_zone_normal_totalsize_by_node);

uint64_t fh2m_inno_get_zone_normal_freesize_by_node(uint32_t node_id)
{
	int zone_index;
	struct zone *zone;
	unsigned long freesize = 0;

	struct pglist_data *pgdat;

	pgdat = NODE_DATA(node_id);
	for (zone_index = 0; zone_index < MAX_NR_ZONES; zone_index++) {
		zone = pgdat->node_zones + zone_index;
		if (zone_idx(zone) == ZONE_NORMAL) {
			freesize += zone_page_state(zone, NR_FREE_PAGES);
		}
	}
	return freesize * fh2m_inno_page_size;
}
INNO_EXT_SYM(fh2m_inno_get_zone_normal_freesize_by_node);

void fh2m_inno_pages_insert(inno_page* page, struct list_head* head)
{
	list_add(&((struct page *)page)->lru, head);
}

INNO_EXT_SYM(fh2m_inno_pages_insert);

inno_page* fh2m_inno_get_first_page(struct list_head* head)
{
	struct page* first_page;

	first_page = list_first_entry(head, struct page, lru);
	list_del(&first_page->lru);
	return first_page;
}

INNO_EXT_SYM(fh2m_inno_get_first_page);

void fh2m_inno_get_next_mem_pfn_range(int *idx, int nid,
				unsigned long *out_start_pfn,
				unsigned long *out_end_pfn, int *out_nid, inno_memblock* memblock)
{
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
    struct memblock_region *r;
	struct memblock_type *type = &((struct memblock*)memblock)->memory;


	while (++*idx < type->cnt) {
		r = &type->regions[*idx];

		if (PFN_UP(r->base) >= PFN_DOWN(r->base + r->size))
			continue;
		if (nid == MAX_NUMNODES || nid == r->nid)
			break;
	}
	if (*idx >= type->cnt) {
		*idx = -1;
		return;
	}

	if (out_start_pfn)
		*out_start_pfn = PFN_UP(r->base);
	if (out_end_pfn)
		*out_end_pfn = PFN_DOWN(r->base + r->size);
	if (out_nid)
		*out_nid = r->nid;
#endif
}
INNO_EXT_SYM(fh2m_inno_get_next_mem_pfn_range);

unsigned long fh2m_inno_kallsyms_lookup_name(const char *name)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	return kallsyms_lookup_name("memblock");
#endif
	return 0;
}
INNO_EXT_SYM(fh2m_inno_kallsyms_lookup_name);

int fh2m_inno_get_user(void *dest, const void __user *src, bool is_u32)
{
	if (is_u32)
		return get_user(*(__le32 *)dest, (const __le32 __user *)src);
	else
		return get_user(*(u8 *)dest, (const u8 __user *)src);
}
INNO_EXT_SYM(fh2m_inno_get_user);

int fh2m_inno_put_user(const void *src, void __user *dest)
{
	return put_user(*(const u8 *)src, (u8 __user *)dest);
}
INNO_EXT_SYM(fh2m_inno_put_user);
