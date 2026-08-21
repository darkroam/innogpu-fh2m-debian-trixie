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
#ifndef __INNO_PLAT_DEV_H__
#define __INNO_PLAT_DEV_H__

#include <linux/types.h>

typedef void inno_platform_device;
typedef void inno_resource;
typedef void inno_dev;
typedef void inno_misc_dev;
typedef void inno_drm;
typedef void inno_i2c_adapter;

struct inno_res {
	uint64_t start;
	uint64_t size;
	const char *name;
};

struct inno_plat_dev_info {
	inno_dev *parent;
	const char *name;
	int id;
	const void *data;
	size_t data_size;
	uint64_t dma_mask;
	struct inno_res *res;
	uint32_t num_res;
};

inno_platform_device *fh2m_inno_platform_device_register_full(struct inno_plat_dev_info *);

void fh2m_inno_platform_device_unregister(inno_platform_device *dev);

inno_dev *fh2m_inno_platform_get_parent_dev(inno_platform_device *dev);

inno_dev *fh2m_inno_platform_get_dev(inno_platform_device *dev);

inno_dev *fh2m_inno_dev_get_parent(inno_dev *dev);

inno_platform_device *fh2m_inno_to_platform_device(void *dev);

void *fh2m_inno_platform_get_data(inno_platform_device *dev);

inno_resource *fh2m_inno_platform_get_resource_byname(inno_platform_device *dev,
		unsigned int type, const char *name);

inno_resource *fh2m_inno_platform_get_resource_iomem_byname(inno_platform_device *dev, const char *name);

uint64_t fh2m_inno_resource_start(inno_resource *res);

uint64_t fh2m_inno_resource_size(inno_resource *res);

void *inno_rsrc_devres_alloc(size_t size);

void *fh2m_inno_rsrc_devres_find(inno_dev *dev);

void fh2m_inno_put_device(inno_dev *dev);

inno_dev *fh2m_inno_get_device(inno_dev *dev);

void fh2m_inno_platform_set_drvdata(void *pdev, void *data);
void *fh2m_inno_platform_get_drvdata(void *pdev);

void *fh2m_inno_dev_get_platdata(void *pdev);

void *fh2m_inno_get_dev_ofnode(inno_dev *dev);

void *fh2m_inno_get_platdev_drvdata(void *pdev);

void *fh2m_inno_get_dev_drvdata(inno_dev *dev);

void fh2m_inno_set_dev_drvdata(inno_dev *dev, void *data);

void *inno_get_drvdata(void *pdev);
int fh2m_inno_platform_get_numa_node(inno_platform_device *dev);

int fh2m_inno_dev_to_node(inno_dev *dev);

void fh2m_inno_platform_suspend(inno_platform_device *dev);

uint64_t fh2m_inno_platform_get_dma_mask(inno_platform_device *dev);

bool fh2m_inno_get_iommu_enable(inno_dev* dev);

inno_dev *fh2m_inno_to_dev(void *dev);

int fh2m_innogpu_sys_info(inno_dev *dev, u32 *tmp, u32 *pll);
int fh2m_innogpu_sys_strings(inno_dev *dev,
		   char *str_fpga_rev, size_t size_fpga_rev,
		   char *str_tcf_core_rev, size_t size_tcf_core_rev,
		   char *str_tcf_core_target_build_id,
		   size_t size_tcf_core_target_build_id,
		   char *str_pci_ver, size_t size_pci_ver,
		   char *str_macro_ver, size_t size_macro_ver);

int fh2m_innogpu_pci_enable(inno_dev *dev);
void fh2m_innogpu_pci_disable(inno_dev *dev);

#endif

