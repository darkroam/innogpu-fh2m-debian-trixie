/*************************************************************************/ /*!
@File           utils.h
@Title
@Copyright      Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
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
#ifndef _DMA_UTILS_H
#define _DMA_UTILS_H
#include "inno_mm.h"
#include "inno_plat_dev.h"

#if defined(CONFIG_X86) || defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)
#define INNO_DMA_MEMSET(a,b,c) fh2m_inno_memset((a), (b), (c))
#define INNO_DMA_MEMCPY_TO_IO(a,b,c) fh2m_inno_memcpy((a), (b), (c))
#else
#define INNO_DMA_MEMSET(a,b,c) fh2m_inno_memset_io((a), (b), (c))
#define INNO_DMA_MEMCPY_TO_IO(a,b,c) fh2m_inno_memcpy_toio((a), (b), (c))
#endif

/*! Performs a 32 bit word read from the device memory. */
#define INNO_DMA_DEVMEM_RD(addr)        (*((volatile uint32_t __force *)((void*)addr)))
/*! Performs a 32 bit word write to the device memory. */
#define INNO_DMA_DEVMEM_WR(addr, val)   (*((volatile uint32_t __force *)((void*)addr)) = (uint32_t)(val))

#define INNO_DMA_WAIT_LLI_CFG_DONE(lli, val)     \
	do {                                         \
		int try_cnt = 0;                         \
		fh2m_inno_smp_mb();                           \
		(void)INNO_DMA_DEVMEM_RD((void *)&lli);  \
		while (lli != val) {                     \
			fh2m_inno_cpu_relax();                         \
			if (try_cnt++ > 100)                 \
				break;                           \
		}                                        \
	} while (0);                                 \

enum dma_chip_type_e {
	DMA_CHIP_INVALID = 0,
	DMA_CHIP_G1,
	DMA_CHIP_G0,
	DMA_CHIP_G1P,
	DMA_CHIP_G0M,
	DMA_CHIP_G3,
	DMA_CHIP_MAX,
};

/* vram alloc and free */
u64 dma_alloc_vram(void *dev, int zone_id, uint64_t size);
void dma_free_vram(void *dev, int zone_id, u64 dev_addr);

enum dma_chip_type_e __maybe_unused dma_get_chip_type(void *dev);

/**lli pool manage*/
struct lli_pool {
	void* base;//dev addr
	void* cpu_vbase;//cpu virt addr
	void* bitmap;
	size_t map_size;
	size_t size;	//size of per block;
	void *lock;
};

struct lli_pool * lli_pool_create(inno_dev *dev, int zone_id, size_t pool_size, size_t size, size_t align);
void lli_pool_release(inno_dev *dev, int zone_id, struct lli_pool *pool);
void* lli_pool_alloc(struct lli_pool *pool);
void lli_pool_free(struct lli_pool *pool, void *addr);
void* lli_pool_get_va(struct lli_pool *pool, void *addr);


/* lli fast pool manage */
struct lli_chunk {
	int chunk_size;
	int chunk_cnt;
};

struct lli_fast_pool_cfg {
	/*
	 * chunk_16k simply means that this chunk can transfer at least 16KB of data,
	 * and possibly more if there are consecutive pages;
	 * In some cases, lli also has a maximum transfer size limit, like axi-dma;
	 */

	struct lli_chunk chunk_16k;
	struct lli_chunk chunk_32k;
	struct lli_chunk chunk_128k;
	struct lli_chunk chunk_512k;

	struct lli_chunk chunk_1m;
	struct lli_chunk chunk_8m;
	struct lli_chunk chunk_16m;
	struct lli_chunk chunk_32m;
	struct lli_chunk chunk_128m;
	struct lli_chunk chunk_256m;
	struct lli_chunk chunk_512m;
};

void* lli_fast_pool_init(inno_dev *dev, struct lli_fast_pool_cfg *cfg, size_t lli_size);
void lli_fast_pool_deinit(inno_dev *dev, void *lli_fast_pool);
uint64_t lli_fast_pool_alloc(void *lli_pool, size_t size, void **cpu_vaddr);
void lli_fast_pool_free(void *lli_pool, uint64_t dev_paddr);

#endif

