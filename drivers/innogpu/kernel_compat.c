#include <linux/version.h>
#include <asm/page.h>
#include <asm/div64.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/genalloc.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <asm/hardirq.h>
#include <asm/tlbflush.h>
#include <linux/timer.h>
#include <linux/capability.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/dmaengine.h>
#include <linux/kthread.h>
#include <linux/utsname.h>
#include <linux/scatterlist.h>
#include <linux/kmemleak.h>
#include "kernel_compat.h"
#include "inno_misc.h"
#include "inno_drm_version.h"

#ifdef CONFIG_NUMA
#include <linux/topology.h>
#endif

void *fh2m_inno_cpumask_of_node(int node)
{
	void *mask = NULL;
#ifdef CONFIG_NUMA
	mask = (void*)cpumask_of_node(node);
#endif
	return mask;
}
INNO_EXT_SYM(fh2m_inno_cpumask_of_node);

#ifdef SUPPORT_LARGE_SG_ENTRIES
static struct scatterlist *os_sg_kmalloc(unsigned int nents, gfp_t gfp_mask)
{
	void *ptr = NULL;

	ptr = kmalloc_array(nents, sizeof(struct scatterlist), gfp_mask | __GFP_NOWARN);
	if (ptr == NULL) {
		ptr = vmalloc(nents * sizeof(struct scatterlist));
	}

	return ptr;
}


static void os_sg_kfree(struct scatterlist *sg, unsigned int nents)
{
	if (is_vmalloc_addr(sg))
		vfree(sg);
	else
		kfree(sg);
}

int fh2m_os_sg_alloc_table(inno_sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	int ret;

	//ret = __sg_alloc_table(table, nents, SG_MAX_SINGLE_ALLOC,NULL, gfp_mask, os_sg_kmalloc);
#if (DRM_VERSION < KERNEL_VERSION(5, 3, 0))
	ret = __sg_alloc_table((struct sg_table *)table, nents, PAGE_SIZE,NULL, gfp_mask, os_sg_kmalloc);
#else
	ret = __sg_alloc_table((struct sg_table *)table, nents, PAGE_SIZE,NULL, 0, gfp_mask, os_sg_kmalloc);
#endif

	if (unlikely(ret))
		//__sg_free_table(table, SG_MAX_SINGLE_ALLOC, false, sg_kfree);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
		__sg_free_table((struct sg_table *)table, PAGE_SIZE, false, os_sg_kfree);
#else
		__sg_free_table((struct sg_table *)table, PAGE_SIZE, false, os_sg_kfree, ((struct sg_table *)table)->orig_nents);
#endif

	return ret;
}
INNO_EXT_SYM(fh2m_os_sg_alloc_table);

static int __os_sg_alloc_table_from_pages(struct sg_table *sgt, struct page **pages,
				unsigned int n_pages, unsigned int offset,
				unsigned long size, unsigned int max_segment,
				gfp_t gfp_mask)
{
	unsigned int chunks, cur_page, seg_len, i;
	int ret;
	struct scatterlist *s = NULL;

	if (WARN_ON(!max_segment || offset_in_page(max_segment)))
		return -EINVAL;

	/* compute number of contiguous chunks */
	chunks = 1;
	seg_len = 0;
	for (i = 1; i < n_pages; i++) {
		seg_len += PAGE_SIZE;
		if (seg_len >= max_segment ||
		    page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1) {
			chunks++;
			seg_len = 0;
		}
	}

	ret = fh2m_os_sg_alloc_table(sgt, chunks, gfp_mask);
	if (unlikely(ret))
		return ret;

	/* merging chunks and putting them into the scatterlist */
	cur_page = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		unsigned int j, chunk_size;

		/* look for the end of the current chunk */
		seg_len = 0;
		for (j = cur_page + 1; j < n_pages; j++) {
			seg_len += PAGE_SIZE;
			if (seg_len >= max_segment ||
			    page_to_pfn(pages[j]) !=
			    page_to_pfn(pages[j - 1]) + 1)
				break;
		}

		chunk_size = ((j - cur_page) << PAGE_SHIFT) - offset;
		sg_set_page(s, pages[cur_page],
			    min_t(unsigned long, size, chunk_size), offset);
		size -= chunk_size;
		offset = 0;
		cur_page = j;
	}

	return 0;
}

void fh2m_os_sg_free_table(inno_sg_table *table)
{
	//__sg_free_table(table, SG_MAX_SINGLE_ALLOC, false, sg_kfree);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	__sg_free_table((struct sg_table *)table, PAGE_SIZE, false, os_sg_kfree);
#else
	__sg_free_table((struct sg_table *)table, PAGE_SIZE, false, os_sg_kfree, ((struct sg_table *)table)->orig_nents);
#endif
}
INNO_EXT_SYM(fh2m_os_sg_free_table);


int fh2m_os_sg_alloc_table_from_pages(inno_sg_table *sgt, void **_pages,
			      unsigned int n_pages, unsigned int offset,
			      unsigned long size, gfp_t gfp_mask)
{
#ifndef SCATTERLIST_MAX_SEGMENT
	#define SCATTERLIST_MAX_SEGMENT (UINT_MAX & PAGE_MASK)
#endif
	struct page **pages = (struct page**)_pages;
	return __os_sg_alloc_table_from_pages((struct sg_table *)sgt, pages, n_pages, offset, size,
					   SCATTERLIST_MAX_SEGMENT, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_os_sg_alloc_table_from_pages);
#endif

