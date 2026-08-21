#ifndef __KERNEL_COMPAT_H__
#define __KERNEL_COMPAT_H__

#define SUPPORT_LARGE_SG_ENTRIES
#ifdef  SUPPORT_LARGE_SG_ENTRIES
#include "inno_dma.h"
int fh2m_os_sg_alloc_table_from_pages(inno_sg_table *sgt, void **pages,
			      unsigned int n_pages, unsigned int offset,
			      unsigned long size, gfp_t gfp_mask);
int fh2m_os_sg_alloc_table(inno_sg_table *table, unsigned int nents, gfp_t gfp_mask);
void fh2m_os_sg_free_table(inno_sg_table *table);
void *fh2m_inno_cpumask_of_node(int node);
#define sg_alloc_table_from_pages  fh2m_os_sg_alloc_table_from_pages
#define sg_free_table			  fh2m_os_sg_free_table
#define sg_alloc_table			  fh2m_os_sg_alloc_table
#endif

#endif

