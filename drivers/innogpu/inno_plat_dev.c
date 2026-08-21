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
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/memblock.h>
#include <linux/kallsyms.h>
#include <linux/iommu.h>
#include <linux/pci.h>
#include "inno_misc.h"
#include "hal.h"
#include "inno_plat_dev.h"
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/numa.h>

inno_platform_device *fh2m_inno_platform_device_register_full(
		struct inno_plat_dev_info *_info)
{
	inno_platform_device *ret = NULL;
	struct resource *res = NULL;
	uint32_t num_res = 0;
	uint32_t i = 0;

	struct platform_device_info info = {
		.parent    = _info->parent,
		.name      = _info->name,
		.id        = _info->id,
		.data      = _info->data,
		.size_data = _info->data_size,
		.dma_mask  = _info->dma_mask,
	};

	if (_info->res) {
		if (_info->num_res == 0)
			num_res = 1;
		else
			num_res = _info->num_res;

		res = (struct resource *)vmalloc(num_res * sizeof(struct resource));
		if (!res) {
			pr_err("no memory!\n");
			return NULL;
		}
		for (i = 0; i < num_res; i++) {
			struct resource res_local = DEFINE_RES_MEM_NAMED(_info->res->start, _info->res->size, _info->res->name);
			res[i] = res_local;
        }

		info.res     = res;
		info.num_res = num_res;
	}

	ret = platform_device_register_full(&info);
	if (_info->res) {
		vfree(res);
	}

	return ret;
}
INNO_EXT_SYM(fh2m_inno_platform_device_register_full);

void fh2m_inno_platform_device_unregister(inno_platform_device *dev)
{
	platform_device_unregister((struct platform_device *)dev);
}
INNO_EXT_SYM(fh2m_inno_platform_device_unregister);

void *fh2m_inno_platform_get_parent_dev(inno_platform_device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	return pdev->dev.parent;
}
INNO_EXT_SYM(fh2m_inno_platform_get_parent_dev);

void *fh2m_inno_platform_get_dev(inno_platform_device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	return &pdev->dev;
}
INNO_EXT_SYM(fh2m_inno_platform_get_dev);

inno_dev *fh2m_inno_dev_get_parent(inno_dev *dev)
{
	return ((struct device *)dev)->parent;
}
INNO_EXT_SYM(fh2m_inno_dev_get_parent);

inno_platform_device *fh2m_inno_to_platform_device(void *dev)
{
	struct device *_dev = (struct device *)dev;
	return to_platform_device(_dev);
}
INNO_EXT_SYM(fh2m_inno_to_platform_device);

void *fh2m_inno_platform_get_data(inno_platform_device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	return pdev->dev.platform_data;
}
INNO_EXT_SYM(fh2m_inno_platform_get_data);

uint64_t fh2m_inno_platform_get_dma_mask(inno_platform_device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	return *pdev->dev.dma_mask;
}
INNO_EXT_SYM(fh2m_inno_platform_get_dma_mask);

inno_resource *fh2m_inno_platform_get_resource_byname(inno_platform_device *dev,
		unsigned int type, const char *name)
{
	return platform_get_resource_byname((struct platform_device *)dev,
		type, name);
}
INNO_EXT_SYM(fh2m_inno_platform_get_resource_byname);

inno_resource *fh2m_inno_platform_get_resource_iomem_byname(inno_platform_device *dev, const char *name)
{
	return platform_get_resource_byname((struct platform_device *)dev, IORESOURCE_MEM, name);
}
INNO_EXT_SYM(fh2m_inno_platform_get_resource_iomem_byname);

uint64_t fh2m_inno_resource_start(inno_resource *res)
{
	return ((struct resource *)res)->start;
}
INNO_EXT_SYM(fh2m_inno_resource_start);

uint64_t fh2m_inno_resource_size(inno_resource *res)
{
	return resource_size((struct resource *)res);
}
INNO_EXT_SYM(fh2m_inno_resource_size);

static void inno_rsrc_devres_release(struct device *dev, void *res)
{
	/* No extra cleanup needed */
}

inno_dev *fh2m_inno_to_dev(void *dev)
{
	struct pci_dev *pdev = (struct pci_dev *)dev;
	return (inno_dev *)&pdev->dev;
}
INNO_EXT_SYM(fh2m_inno_to_dev);

void *fh2m_inno_rsrc_devres_find(inno_dev *dev)
{
	return devres_find(dev, inno_rsrc_devres_release, NULL, NULL);
}
INNO_EXT_SYM(fh2m_inno_rsrc_devres_find);

void *inno_rsrc_devres_alloc(size_t size)
{
	return devres_alloc(inno_rsrc_devres_release, size, GFP_KERNEL);
}

void fh2m_inno_put_device(inno_dev *dev)
{
	put_device((struct device *)dev);
}
INNO_EXT_SYM(fh2m_inno_put_device);

inno_dev *fh2m_inno_get_device(inno_dev *dev)
{
	return (inno_dev *)get_device((struct device *)dev);
}
INNO_EXT_SYM(fh2m_inno_get_device);

void fh2m_inno_platform_set_drvdata(void *pdev, void *data)
{
	platform_set_drvdata((struct platform_device *)pdev, data);
}
INNO_EXT_SYM(fh2m_inno_platform_set_drvdata);

void *fh2m_inno_platform_get_drvdata(void *pdev)
{
	return platform_get_drvdata((struct platform_device *)pdev);
}
INNO_EXT_SYM(fh2m_inno_platform_get_drvdata);

void *fh2m_inno_dev_get_platdata(void *pdev)
{
	return (void *)dev_get_platdata((struct device *)pdev);
}
INNO_EXT_SYM(fh2m_inno_dev_get_platdata);

void *fh2m_inno_get_dev_ofnode(inno_dev *dev)
{
	return ((struct device *)dev)->of_node;
}
INNO_EXT_SYM(fh2m_inno_get_dev_ofnode);

void *fh2m_inno_get_platdev_drvdata(void *pdev)
{
	return dev_get_drvdata(&((struct platform_device *)pdev)->dev);
}
INNO_EXT_SYM(fh2m_inno_get_platdev_drvdata);

void *fh2m_inno_get_dev_drvdata(inno_dev *dev)
{
	return dev_get_drvdata((struct device *)dev);
}
INNO_EXT_SYM(fh2m_inno_get_dev_drvdata);

void fh2m_inno_set_dev_drvdata(inno_dev *dev, void *data)
{
	dev_set_drvdata((struct device *)dev, data);
}
INNO_EXT_SYM(fh2m_inno_set_dev_drvdata);

int fh2m_inno_platform_get_numa_node(inno_platform_device *dev)
{
	int node_node = 0;
#ifdef CONFIG_NUMA
	struct platform_device *pdev = (struct platform_device *)dev;
	node_node = pdev->dev.numa_node;
#endif
	return node_node;
}
INNO_EXT_SYM(fh2m_inno_platform_get_numa_node);

int fh2m_inno_dev_to_node(inno_dev *dev)
{
	return dev_to_node(dev);
}
INNO_EXT_SYM(fh2m_inno_dev_to_node);

void fh2m_inno_platform_suspend(inno_platform_device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	if (pdev &&
			pdev->dev.driver &&
			pdev->dev.driver->pm &&
			pdev->dev.driver->pm->suspend) {
		pdev->dev.driver->pm->suspend(&pdev->dev);
	}
}
INNO_EXT_SYM(fh2m_inno_platform_suspend);

bool fh2m_inno_get_iommu_enable(inno_dev* dev)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	struct bus_type* bus = ((struct device *)dev)->bus;
	if (iommu_present(bus)) {
		return true;
	}
#else
	struct iommu_domain * domain = iommu_get_domain_for_dev((struct device *)dev);
	if(domain != NULL)
		return true;
#endif
	return false;
}
INNO_EXT_SYM(fh2m_inno_get_iommu_enable);

static int _sys_strings(char *str_fpga_rev, size_t size_fpga_rev,
	char *str_tcf_core_rev, size_t size_tcf_core_rev,
	char *str_tcf_core_target_build_id,
	size_t size_tcf_core_target_build_id,
	char *str_pci_ver, size_t size_pci_ver,
	char *str_macro_ver, size_t size_macro_ver)
{
	u32 val;
	char temp_str[12];

	/* Read the Odin major and minor revision ID register Rx-xx */
	val = 1;

	snprintf(str_tcf_core_rev,
		 size_tcf_core_rev,
		 "%d.%d",
		 HEX2DEC((val & HW_REVISION_MAJOR_MASK)
			 >> HW_REVISION_MAJOR_SHIFT),
		 HEX2DEC((val & HW_REVISION_MINOR_MASK)
			 >> HW_REVISION_MINOR_SHIFT));

	/* Read the Odin register containing the Perforce changelist
	 * value that the FPGA build was generated from
	 */
	val = 1;

	snprintf(str_tcf_core_target_build_id,
		 size_tcf_core_target_build_id,
		 "%d",
		 (val & HW_CHANGE_SET_SET_MASK)
		 >> HW_CHANGE_SET_SET_SHIFT);

	/* Read the Odin User_ID register containing the User ID for
	 * identification of a modified build
	 */
	val = 1;

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & HW_USER_ID_ID_MASK)
			 >> HW_USER_ID_ID_SHIFT));

	/* Read the Odin User_Build register containing the User build
	 * number for identification of modified builds
	 */
	val = 1;

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & HW_USER_BUILD_BUILD_MASK)
			 >> HW_USER_BUILD_BUILD_SHIFT));

	return 0;
}

int fh2m_innogpu_sys_strings(inno_dev *dev, char *str_fpga_rev,
	size_t size_fpga_rev, char *str_tcf_core_rev, size_t size_tcf_core_rev,
	char *str_tcf_core_target_build_id, size_t size_tcf_core_target_build_id,
	char *str_pci_ver, size_t size_pci_ver, char *str_macro_ver,
	size_t size_macro_ver)
{
	if (!str_fpga_rev                  ||
		!size_fpga_rev                 ||
		!str_tcf_core_rev              ||
		!size_tcf_core_rev             ||
		!str_tcf_core_target_build_id  ||
		!size_tcf_core_target_build_id ||
		!str_pci_ver                   ||
		!size_pci_ver                  ||
		!str_macro_ver                 ||
		!size_macro_ver) {
		return -EINVAL;
	}

    /**FIXME*/
	return _sys_strings(str_fpga_rev, size_fpga_rev,
		str_tcf_core_rev, size_tcf_core_rev,
		str_tcf_core_target_build_id,
		size_tcf_core_target_build_id,
		str_pci_ver, size_pci_ver,
		str_macro_ver, size_macro_ver);
}
INNO_EXT_SYM(fh2m_innogpu_sys_strings);

int fh2m_innogpu_sys_info(inno_dev *dev, u32 *tmp, u32 *pll)
{
	return -ENODEV;
}
INNO_EXT_SYM(fh2m_innogpu_sys_info);

int fh2m_innogpu_pci_enable(inno_dev *dev)
{
	struct pci_dev *pdev = to_pci_dev((struct device *)dev);

	return pci_enable_device(pdev);
}
INNO_EXT_SYM(fh2m_innogpu_pci_enable);

void fh2m_innogpu_pci_disable(inno_dev *dev)
{
	struct pci_dev* pdev = to_pci_dev((struct device *)dev);
	pci_disable_device(pdev);
}
INNO_EXT_SYM(fh2m_innogpu_pci_disable);
