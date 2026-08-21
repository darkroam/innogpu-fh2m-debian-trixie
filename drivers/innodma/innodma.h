/*************************************************************************/ /*!
@File           innodma.h
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
#ifndef __INNODMA_H__
#define __INNODMA_H__

#include "inno_plat_dev.h"
#include "inno_mm.h"
#include "hal_interface.h"

#define INNO_DMA_UNUSED_PARAM(param) ((void)(param))

enum dma_packet_size
{
	DMA_NO_PACKET   = 0,
	DMA_PACKET_4K   = 0x1000,
	DMA_PACKET_16K  = 0x4000,
	DMA_PACKET_64K  = 0x10000,
	DMA_PACKET_128K = 0x20000,
	DMA_PACKET_1M   = 0x100000,
	DMA_PACKET_4M   = 0x400000,
	DMA_PACKET_10M  = 0xA00000,
};

enum dma_xfer_direction
{
	SYS2GDDR  = DMA_MEM2DEV,
	GDDR2SYS  = DMA_DEV2MEM,
	GDDR2GDDR = DMA_DEV2DEV,
	SYS2SYS   = DMA_MEM2MEM,
};

enum innodma_error {
	INNO_DMA_OK = 0,
	INNO_DMA_ERROR_INVALID_PARAM,
	INNO_DMA_ERROR_TIMEOUT,
	INNO_DMA_ERROR_NO_MEMORY,
	INNO_DMA_ERROR_CHAN_BUSY,
	INNO_DMA_ERROR_BAD_MAPPING,
	INNO_DMA_ERROR = 999,
};

static inline void* innodma_ioremap_uncached(inno_dev *dev, uint64_t cpu_pa, int size)
{
	void *cpu_va;
	uint64_t dev_pa;

	dev_pa = fh2m_cpu_paddr_to_dev_paddr(dev, cpu_pa);
	if (!fh2m_hal_is_visible_vram(dev, dev_pa))
		return NULL;

	cpu_va = fh2m_inno_ioremap_wc_portable(cpu_pa, size);
	return cpu_va;
}

static inline void innodma_iounmap(void *cpu_va)
{
	fh2m_inno_iounmap(cpu_va);
}

#if defined(SUPPORT_DMA_TRANSFER)
/**
 * fh2m_innodma_memcpy: continous memory copy using dma
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use device address
 */
int fh2m_innodma_memcpy(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir);

/**
 * innodma_memcpy_sg: discontinous memory copy using dma
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use device address
 */
#define innodma_memcpy_sg(dev, srcs, dsts, lens, cnt, dir) \
	fh2m_innodma_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir));

/**
 * fh2m_innodma_compat_memcpy_sg: this func is used to be compatible with older funcs
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use pcie address, that's bar2 address
 */
int fh2m_innodma_compat_memcpy_sg(inno_dev *dev, void *srcs[], void *dsts[], int *lens, int cnt, int dir);

/**
 * fh2m_innodma_split_packet_and_memcpy: breaks the packet into a smaller packet than the specified packet size
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use pcie address, mostly that's bar2 address
 */
int fh2m_innodma_split_packet_and_memcpy(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir, int packet_size);

/**
 * innodma_split_packet_and_memcpy_sg: breaks the packet into a smaller packet than the specified packet size
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use pcie address, that's bar2 address
 */
#define innodma_split_packet_and_memcpy_sg(dev, srcs, dsts, lens, cnt, dir, packet_size) \
	fh2m_innodma_split_packet_and_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir), (packet_size));

#if 0
/**
 * fh2m_innodma_memcpy_for_smallbar_sg: discontinous memory copy in smallbar scenarios
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use pcie address, that's bar2 address normally
 */
#define fh2m_innodma_memcpy_for_smallbar_sg(dev, srcs, dsts, lens, cnt, dir) \
	fh2m_innodma_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir));
#else
	int fh2m_innodma_memcpy_for_smallbar_sg(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir);
#endif

/**
 * innodma_memcpy_for_smallbar: continous memory copy in smallbar scenarios
 * @dev   : pcie device
 * @srcs  : source address array
 * @dsts  : destination address array
 * @lens  : length array
 * @cnt   : transfer count
 * @dir   : transfer direction
 * return : 0 success, 1 failed
 * note   : vram address must use pcie address, that's bar2 address
 */
#define innodma_memcpy_for_smallbar(dev, srcs, dsts, lens, cnt, dir) \
	fh2m_innodma_memcpy_for_smallbar_sg((dev), (srcs), (dsts), (lens), (cnt), (dir));

/**
 * fh2m_innodma_memset: continous memory memset
 * @dests : destination address array
 * @vals  : memset value array
 * @lens  : memset size array
 * @cnt   : array size
 * @is_dev_mem : is vram or not
 * return : 0 success, 1 failed
 */
int fh2m_innodma_memset(inno_dev *dev, void **dests, int *vals, int *lens, int cnt, bool is_dev_mem);

/**
 * fh2m_innodma_memset: discontinous memory memset
 * @dests : destination address array
 * @vals  : memset value array
 * @lens  : memset size array
 * @cnt   : array size
 * @is_dev_mem : is vram or not
 * return : 0 success, 1 failed
 */
int fh2m_innodma_memset_sg(inno_dev *dev, void **dests, int *vals, int *lens, int cnt);

/**
 * innodma_transfer:new dma transfer interface.host mem must be mapped in advance.
 * @src : source address array
 * @dst : destination address array
 * @lens : xfer size array
 * @cnt : xfer count
 * @dir : xfer direction
 * return : 0 success, 1 failed
 */
int fh2m_innodma_transfer_fast(inno_dev *dev, void **src, void **dst, int *lens, int cnt, int dir);

#define innodma_memcpy2(dev, srcs, dsts, lens, cnt, dir) \
	fh2m_innodma_compat_memcpy_sg((dev), (srcs), (dsts), (lens), (cnt), (dir))

#define innodma_memcpy3(dev, srcs, dsts, lens, cnt, dir) \
	fh2m_innodma_compat_memcpy_sg((dev), (srcs), (dsts), (lens), (cnt), (dir))

#else /* SUPPORT_DMA_TRANSFER */
/* #include "osfunc_common.h" */

static int inline device_memcpy(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir, bool is_user_addr)
{
	int i = 0;
	uint64_t src_dev_pa, src_cpu_pa;
	uint64_t dst_dev_pa, dst_cpu_pa;
	void __iomem *src_cpu_va, __iomem *dst_cpu_va;

	for (i = 0; i < cnt; i++) {
		if (SYS2GDDR == dir) {
			dst_dev_pa = (uint64_t)dsts[i];
			dst_cpu_pa = fh2m_dev_paddr_to_cpu_paddr(dev, dst_dev_pa);
			dst_cpu_va = innodma_ioremap_uncached(dev, dst_cpu_pa, lens[i]);

			if (is_user_addr) {
				fh2m_inno_copy_from_user(dst_cpu_va, srcs[i], lens[i]);
			} else {
				/* OSDeviceMemCopyFromIO(dst_cpu_va, srcs[i], lens[i]); */
				fh2m_inno_memcpy_toio(dst_cpu_va, srcs[i], lens[i]);
			}
		} else if (GDDR2SYS == dir) {
			src_dev_pa = (uint64_t)srcs[i];
			src_cpu_pa = fh2m_dev_paddr_to_cpu_paddr(dev, src_dev_pa);
			src_cpu_va = innodma_ioremap_uncached(dev, src_cpu_pa, lens[i]);

			if (is_user_addr) {
				fh2m_inno_copy_to_user(dsts[i], src_cpu_va, lens[i]);
			} else {
				/* OSDeviceMemCopyFromIO(dsts[i], src_cpu_va, lens[i]); */
				fh2m_inno_memcpy_fromio(dsts[i], src_cpu_va, lens[i]);
			}
		} else if (GDDR2GDDR == dir) {
			src_dev_pa = (uint64_t)srcs[i];
			src_cpu_pa = fh2m_dev_paddr_to_cpu_paddr(dev, src_dev_pa);
			src_cpu_va = innodma_ioremap_uncached(dev, src_cpu_pa, lens[i]);

			dst_dev_pa = (uint64_t)dsts[i];
			dst_cpu_pa = fh2m_dev_paddr_to_cpu_paddr(dev, dst_dev_pa);
			dst_cpu_va = innodma_ioremap_uncached(dev, dst_cpu_pa, lens[i]);

			/* OSDeviceMemCopyFromIO(dst_cpu_va, src_cpu_va, lens[i]); */
			fh2m_inno_memcpy_toio(dst_cpu_va, src_cpu_va, lens[i]);
		} else {
			return -1;
		}
	}

	return 0;
}

static int inline device_compat_memcpy(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir, bool is_user_addr)
{
	int i = 0;
	void **dev_paddrs;

	dev_paddrs = fh2m_inno_vmalloc(cnt * sizeof(void *));
	if (!dev_paddrs) {
		return -1;
	}

	if (SYS2GDDR == dir) {
		for (i = 0; i < cnt; i++) {
			dev_paddrs[i] = (void *)fh2m_pcie_paddr_to_cpu_paddr(dev, (uint64_t)dsts[i]);
			dev_paddrs[i] = (void *)fh2m_pcie_paddr_to_cpu_paddr(dev, (uint64_t)dev_paddrs[i]);
		}

		device_memcpy(dev, srcs, dev_paddrs, lens, cnt, dir, is_user_addr);
	} else if (GDDR2SYS == dir) {
		for (i = 0; i < cnt; i++) {
			dev_paddrs[i] = (void *)fh2m_pcie_paddr_to_cpu_paddr(dev, (uint64_t)srcs[i]);
			dev_paddrs[i] = (void *)fh2m_pcie_paddr_to_cpu_paddr(dev, (uint64_t)dev_paddrs[i]);
		}

		device_memcpy(dev, dev_paddrs, dsts, lens, cnt, dir, is_user_addr);
	} else {
		fh2m_inno_vfree(dev_paddrs);
		return -1;
	}

	fh2m_inno_vfree(dev_paddrs);

	return 0;
}

static int inline unsupport_func(inno_dev *dev, void **srcs, void **dsts, int *lens, int cnt, int dir)
{
	INNO_DMA_UNUSED_PARAM(dev);
	INNO_DMA_UNUSED_PARAM(srcs);
	INNO_DMA_UNUSED_PARAM(dsts);
	INNO_DMA_UNUSED_PARAM(lens);
	INNO_DMA_UNUSED_PARAM(cnt);
	INNO_DMA_UNUSED_PARAM(dir);

	return -1;
}

#define fh2m_innodma_memcpy(dev, srcs, dsts, lens, cnt, dir) \
	device_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir), (false))

#define innodma_memcpy2(dev, srcs, dsts, lens, cnt, dir) \
	device_compat_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir), (true))

#define innodma_memcpy3(dev, srcs, dsts, lens, cnt, dir) \
	device_compat_memcpy((dev), (srcs), (dsts), (lens), (cnt), (dir), (false))

#define innodma_memcpy_for_smallbar(dev, srcs, dsts, lens, cnt, dir) \
	unsupport_func((dev), (srcs), (dsts), (lens), (cnt), (dir))

#define fh2m_innodma_memcpy_for_smallbar_sg(dev, srcs, dsts, lens, cnt, dir) \
	unsupport_func((dev), (srcs), (dsts), (lens), (cnt), (dir))

#define fh2m_innodma_transfer_fast(dev, srcs, dsts, lens, cnt, dir) \
	unsupport_func((dev), (srcs), (dsts), (lens), (cnt), (dir))

#endif /* SUPPORT_DMA_TRANSFER */

#ifdef CONFIG_DRM_INNO_DMA
int innodma_driver_register(void);
void innodma_driver_unregister(void);
#endif

#endif
