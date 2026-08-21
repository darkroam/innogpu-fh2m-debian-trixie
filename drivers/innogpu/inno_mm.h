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
#ifndef __INNO_MM_H__
#define __INNO_MM_H__

#include <linux/types.h>

extern unsigned long fh2m_inno_page_size;
extern uint8_t  fh2m_inno_page_shift;
extern int fh2m_inno_page_struct_size;

#define INNO_PAGE_IDX_SIZE(idx) (fh2m_inno_page_struct_size * (idx))

typedef void inno_page;
typedef void inno_pgprot;
typedef void inno_vm_area;
typedef void inno_vm_fault;
typedef void inno_kmem_cache;
typedef void inno_mm_struct;
typedef void inno_shrinker_t;
typedef void inno_shrink_control;
typedef void inno_gen_pool;
typedef void inno_gen_pool_chunk;
typedef void inno_memblock;
typedef struct
{
#if defined(UNDER_WDDM) || defined(WINDOWS_WDF)
	uintptr_t ui_addr;
#elif defined(__linux__) && defined(__KERNEL__)
	phys_addr_t ui_addr;
#else
	uint64_t ui_addr;
#endif
} CPU_INNO_PHYADDR;

void fh2m_inno_pgprot_init(void);

unsigned long fh2m_inno_pgprot_val(inno_pgprot *pgprot);

void *fh2m_inno_kmalloc_kernel(uint64_t size);

void *fh2m_inno_krealloc_kernel(const void *p, size_t new_size);

void *fh2m_inno_kmalloc_atomic(uint64_t size);

void *fh2m_inno_kzalloc_kernel(uint64_t size);

void *fh2m_inno_kmalloc_array(size_t n, size_t size);

void *fh2m_inno_kzalloc_array(size_t n, size_t size);

void *fh2m_inno_kmalloc_page_array(size_t n);

void *fh2m_inno_kzalloc_page_array(size_t n);

void fh2m_inno_kfree(const void *);

uint64_t fh2m_inno_ksize(const void *addr);

void *fh2m_inno_vmalloc(uint64_t size);

void *fh2m_inno_vzalloc(uint64_t size);

void fh2m_inno_vfree(const void *);

bool fh2m_inno_is_vmalloc_addr(const void *addr);

bool fh2m_inno_is_kmalloc_addr(const void *addr);

void *fh2m_inno_kvmalloc_kernel(uint64_t size);

void fh2m_inno_kvfree(const void *addr);

inno_page* fh2m_inno_vmalloc_to_page(const void *addr);

void fh2m_inno_set_page_reserved(inno_page *page);

void fh2m_inno_clear_page_reserved(inno_page *page);

void fh2m_inno_set_vmalloc_pages_reserved(void *addr, uint64_t size);

void fh2m_inno_clear_vmalloc_pages_reserved(void *addr, uint64_t size);

void *fh2m_inno_devm_kmalloc_kernel(void *dev, uint64_t size);

void *fh2m_inno_devm_kzalloc_kernel(void *dev, uint64_t size);

void *fh2m_inno_devm_kcalloc_kernel(void *dev, uint64_t n, uint64_t size);

void fh2m_inno_devm_kfree(void *dev, void *addr);

int fh2m_inno_devm_add_action(void *dev, void (*action)(void *), void *data);

void *fh2m_inno_kmemdup_kernel(const void *src, uint64_t len);

uint64_t fh2m_inno_page_align(uint64_t addr);

struct list_head* fh2m_inno_page_lru(inno_page *page);

uint32_t fh2m_inno_get_order(uint64_t size);

uint32_t fh2m_inno_compound_order(inno_page *page);

inno_page *fh2m_inno_alloc_pages(uint32_t flags, uint32_t order);

void fh2m_inno_page_set_mapping(inno_page *page, void* ptr);

inno_page *fh2m_inno_page_inc(inno_page *page, uint64_t order);

gfp_t fh2m_inno_page_flag_comp(void);

gfp_t fh2m_inno_page_flag_nowarn(void);

gfp_t fh2m_inno_page_flag_kernel(void);

inno_page *fh2m_inno_alloc_gtt_highmem_pages(uint32_t order);

inno_page *fh2m_inno_virt_to_page(void *addr);

unsigned long fh2m_inno_offset_in_page(void *addr);

uint64_t fh2m_inno_virt_to_phys(void *addr);

void fh2m_inno_split_page(inno_page *page, unsigned int order);

uint32_t fh2m_inno_alloc_uma_flags(void *dev);

uint32_t fh2m_inno_alloc_osmem_flags(bool zero, void *dev);

uint32_t fh2m_inno_alloc_osgtt_flags(bool zero, void *dev);

uint32_t fh2m_inno_gfp_reclaim(void);

uint32_t fh2m_inno_gfp_noretry(void);

void fh2m_inno_free_pages(inno_page *pages, uint32_t order);

uint64_t fh2m_inno_page_to_phys(inno_page *page);

inno_page *fh2m_inno_phys_to_page(uint64_t addr);

void *fh2m_inno_page_address(const inno_page *page);

int fh2m_inno_set_memory_wb(unsigned long addr, int numpages);

int fh2m_inno_set_memory_wc(unsigned long addr, int numpages);

int fh2m_inno_set_memory_uc(unsigned long addr, int numpages);

int fh2m_inno_set_pages_array_uc(inno_page **pages, int nums);

int fh2m_inno_set_pages_array_wc(inno_page **pages, int nums);

int fh2m_inno_set_pages_array_wb(inno_page **pages, int nums);

void fh2m_inno_pgprot_noncached(inno_pgprot *old_prot, inno_pgprot *prot_res);

void fh2m_inno_pgprot_cached(inno_pgprot *old_prot, inno_pgprot *prot_res);

void fh2m_inno_pgprot_writecombine(inno_pgprot *old_prot, inno_pgprot *prot_res);

inno_pgprot *fh2m_inno_get_inno_page_kernel(void);

unsigned long fh2m_inno_get_vm_map_flag(void);

void *fh2m_inno_vmap(inno_page **pages, unsigned int cnt, unsigned long flags, inno_pgprot *prot);

void fh2m_inno_vunmap(void *addr, unsigned int count, inno_pgprot *prot);

inno_page *fh2m_inno_page_idx(inno_page* page, uint32_t index);

void *fh2m_inno_kmap(inno_page *page);

void fh2m_inno_kunmap(inno_page *page);

void *fh2m_inno_kmap_atomic(inno_page *page);

void fh2m_inno_kunmap_atomic(void *addr);

void fh2m_inno_current_mmap_write_lock(void);

void fh2m_inno_current_mmap_write_unlock(void);

void fh2m_inno_current_mmap_read_lock(void);

void fh2m_inno_current_mmap_read_unlock(void);

inno_vm_area *fh2m_inno_find_vma(uint64_t address);

void fh2m_inno_unmap_mapping_range(inno_vm_area *vma, uint64_t addr, uint32_t size, int even_cows);

uint64_t fh2m_inno_phys_to_pfn(uint64_t addr, uint64_t flags);

uint64_t fh2m_inno_pfn_down(uint64_t addr);

uint64_t fh2m_inno_page_to_pfn(inno_page *page);

inno_page *fh2m_inno_pfn_to_page(uint64_t pfn);

int fh2m_inno_pfn_valid(uint64_t pfn);

int fh2m_inno_page_count(inno_page *page);

inno_mm_struct *fh2m_inno_get_current_mm_struct(void);

void fh2m_inno_vm_area_pgoff_reset(inno_vm_area *vma);

unsigned long fh2m_inno_vm_area_start(inno_vm_area *vma);

unsigned long fh2m_inno_vm_area_end(inno_vm_area *vma);

void fh2m_inno_vm_area_set_mixed_flag(inno_vm_area *vma);

int fh2m_inno_vm_area_is_mixed_flag(inno_vm_area *vma);

int fh2m_inno_vm_area_has_write_flag(inno_vm_area *vma);

int fh2m_inno_vm_area_has_share_flag(inno_vm_area *vma);

int fh2m_inno_vm_area_has_read_flag(inno_vm_area *vma);

void fh2m_inno_vm_get_page_prot(inno_vm_area *vma, inno_pgprot *prot);

void fh2m_inno_vm_set_page_prot(inno_vm_area *vma, inno_pgprot *prot);

void fh2m_inno_vm_area_set_io_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_set_dontdump_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_set_dontexpand_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_set_dontcopy_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_set_pfnmap_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_unset_pfnmap_flag(inno_vm_area *vma);

void fh2m_inno_vm_area_set_private_data(inno_vm_area *vma, void* data);

void* fh2m_inno_vm_area_get_private_data(inno_vm_area *vma);

void fh2m_inno_vm_area_set_pgoff(inno_vm_area *vma, unsigned long pgoff);

unsigned long fh2m_inno_vm_area_len(inno_vm_area *vma);

int fh2m_inno_vm_insert_mixed(inno_vm_area *vma, uint64_t addr, uint64_t pfn);

int fh2m_inno_vm_insert_page(inno_vm_area *vma, uint64_t addr, inno_page *page);

int fh2m_inno_vm_insert_pfn(inno_vm_area *vma, unsigned long addr, unsigned long pfn);

unsigned long fh2m_inno_vm_get_start(inno_vm_area *vma);

unsigned long fh2m_inno_vmf_get_address(inno_vm_fault *vmf);

void fh2m_inno_vmf_set_page(inno_vm_fault *vmf, inno_page *page);

int fh2m_inno_set_page_dirty_lock(inno_page *page);

int fh2m_inno_set_page_dirty(inno_page *page);

void fh2m_inno_put_page(inno_page *page);

void fh2m_inno_get_page(inno_page *page);

int fh2m_inno_get_user_pages_fast(uint64_t addr, int nr_pages, int write, inno_page **pages);

long fh2m_inno_get_user_pages(uint64_t addr, int nr_pages, unsigned int flag, inno_page **pages, void* vmas);

int fh2m_inno_cache_line_size(void);

void *fh2m_inno_ioremap(uint64_t addr, uint64_t size);

void *fh2m_inno_ioremap_nocache(uint64_t addr, uint64_t size);

void *fh2m_inno_ioremap_cache(uint64_t addr, uint64_t size);

void *fh2m_inno_ioremap_wc(uint64_t addr, uint64_t size);

void *fh2m_inno_ioremap_wc_portable(uint64_t addr, uint64_t size);

void fh2m_inno_iounmap(void *ptr);

int fh2m_inno_access_ok(const void __user *addr, unsigned long size);

unsigned long fh2m_inno_copy_from_user(void *to, const void __user *from, unsigned long bytes);

unsigned long fh2m_inno_copy_to_user(void __user *to, const void *from, unsigned long bytes);

inno_kmem_cache *fh2m_inno_kmem_cache_create(const char *name, unsigned int size, unsigned int align, unsigned int flags, void (*ctor)(void *));

void fh2m_inno_kmem_cache_destroy(inno_kmem_cache *s);

void *fh2m_inno_kmem_cache_alloc_kernel(inno_kmem_cache *s);
void fh2m_inno_kmem_cache_free(inno_kmem_cache *s, void *objp);

void *fh2m_inno_dma_alloc_coherent(void *dev, uint64_t size, dma_addr_t *dma_handle, uint32_t gfp);

void *fh2m_inno_dma_alloc_coherent_kernel(void *dev, uint64_t size, dma_addr_t *dma_handle);

void fh2m_inno_dma_free_coherent(void *dev, uint64_t size, void *cpu_addr, dma_addr_t dma_handle);

void fh2m_inno_get_os_ram_stats(uint64_t *total_size, uint64_t *free_size);

inno_shrinker_t *fh2m_inno_register_shrinker(
		unsigned long (*count_objects)(inno_shrinker_t *, inno_shrink_control *sc),
		unsigned long (*scan_objects)(inno_shrinker_t *, inno_shrink_control *sc));

void fh2m_inno_unregister_shrinker(inno_shrinker_t *s);

unsigned long fh2m_inno_get_shrinker_obj_nr_to_scan(inno_shrink_control *sc);

void fh2m_inno_set_vm_area_priv(inno_vm_area *vma, void *priv);

bool fh2m_inno_is_valid_pmr_vaddr(inno_vm_area *vma, unsigned long vaddr,
		unsigned long size, void *pmr);

int fh2m_inno_remap_pfn_range(inno_vm_area *vma, unsigned long addr,
	unsigned long pfn, unsigned long size);

void *fh2m_inno_memset(void *s, int c, size_t count);

void *fh2m_inno_memcpy(void *dest, const void *src, size_t count);

void fh2m_inno_memcpy_fromio(void *to, const volatile void __iomem *from, size_t count);

void fh2m_inno_memcpy_toio(volatile void __iomem *to, const void *from, size_t count);

void fh2m_inno_memset_io(volatile void __iomem *a, int b, size_t c);

void fh2m_inno_memset_io_portable(void *s, int c, size_t count);

unsigned long fh2m_inno_get_dma32_zone_pages(void);

unsigned long fh2m_inno_get_normal_high_zone_total_pages(void);

uint64_t fh2m_inno_get_zonedma32_totalsize(void);

uint64_t fh2m_inno_get_zonedma32_freesize(void);

bool fh2m_inno_mem_init_on_alloc(void);

bool fh2m_inno_os_phys_non_contig_supported(void);

void fh2m_inno_os_unmap_phys_array_to_lin(void *addr, void *priv_data);

void* fh2m_inno_os_map_phys_array_to_lin(CPU_INNO_PHYADDR* page_phy_addr, uint32_t pages_count, void ** pp_vlin_addr, void **pp_vpriv_data);

void fh2m_inno_set_prot_default_page_kernel(inno_pgprot *prot);

void fh2m_inno_flush_cache_range(inno_vm_area* vma, unsigned long addr_start, unsigned long addr_end);

unsigned long fh2m_inno_gen_pool_alloc(inno_gen_pool * pool, size_t size);

int fh2m_inno_gen_pool_add_virt(inno_gen_pool * pool, unsigned long virt_start, phys_addr_t phys, size_t size, int nid);

void fh2m_inno_gen_pool_free(inno_gen_pool * pool, unsigned long start, size_t size);

inno_gen_pool *fh2m_inno_gen_pool_create(int order, int nid);

phys_addr_t fh2m_inno_gen_pool_virt_to_phys(inno_gen_pool * pool, unsigned long start);

int fh2m_inno_gen_pool_add(inno_gen_pool * pool, unsigned long addr, size_t size, int nid);

void fh2m_inno_gen_pool_destroy(inno_gen_pool * pool);

unsigned long fh2m_inno_gen_pool_chunk_start_addr(inno_gen_pool_chunk * chunk);

unsigned long fh2m_inno_gen_pool_chunk_end_addr(inno_gen_pool_chunk * chunk);

unsigned long fh2m_inno_gen_pool_avail_size(inno_gen_pool * pool);

inno_page *fh2m_inno_alloc_pages_by_node(uint32_t node_id, uint32_t flags, uint32_t order);
uint64_t fh2m_inno_get_zone_normal_totalsize_by_node(uint32_t node_id);
uint64_t fh2m_inno_get_zone_normal_freesize_by_node(uint32_t node_id);

inno_page* fh2m_inno_get_first_page(struct list_head* head);
void fh2m_inno_pages_insert(inno_page* page, struct list_head* head);

void fh2m_inno_get_next_mem_pfn_range(int *idx, int nid,
				unsigned long *out_start_pfn,
				unsigned long *out_end_pfn, int *out_nid, inno_memblock* memblock);
unsigned long fh2m_inno_kallsyms_lookup_name(const char *name);
void fh2m_inno_put_user_pages(inno_page **pages, unsigned long npages);

int fh2m_inno_get_user(void *dest, const void __user *src, bool is_u32);

int fh2m_inno_put_user(const void *src, void __user *dest);
#endif
