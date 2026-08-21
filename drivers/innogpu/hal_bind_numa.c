/*************************************************************************/ /*!
@File           hal_bind_numa.c
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
#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)

#include <linux/mmzone.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>

#include "hal.h"
#include "inno_misc.h"
#include "hal_interface.h"
#include "inno_debug.h"

#define KBUILD_HAL "hal_bind_numa"
#define pr_fmt_irq(fmt) "[%s][%s:%d]" fmt,KBUILD_HAL,__func__,__LINE__
#if defined(INNO_GPU_LOG)
#define hal_dbg(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_DEBUG,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_info(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_INFO,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_warn(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_WARNING,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#else
#define hal_dbg(dev,fmt, ...)
#define hal_info(dev,fmt, ...)
#define hal_warn(dev,fmt, ...)
#endif
#define hal_error(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_ERR,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_notice(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_NOTICE,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)

#define DEFAULT_BIND_CPU_NUMS 2

int enable_bind_cpu = 0;
module_param(enable_bind_cpu, int, 0644);
MODULE_PARM_DESC(enable_bind_cpu, "enable/disable bind cpu for cards on different numa nodes, default(0)");

static unsigned int *numa_pcie_nums = NULL;

int fh2m_bind_cpu_is_enable(inno_dev *pdev) {
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(pdev);
	return pdev_rsrc->bind_cpu_enable;
}

int bind_numa_config_init(struct dev_rsrc* pdev_rsrc) {
	unsigned int device_numa_id, cpu_offset, cpu_num;
	unsigned int cpu_online_num = num_online_cpus();
	int cpu_per_numa = cpu_online_num / num_online_nodes();

	pdev_rsrc->bind_cpu_enable = enable_bind_cpu;
	if (!pdev_rsrc->bind_cpu_enable) {
		return 0;
	}

	if (NULL == numa_pcie_nums) {
		numa_pcie_nums = kzalloc(nr_node_ids*sizeof(unsigned int), GFP_KERNEL);
		if (!numa_pcie_nums) {
			goto error;
		}
	}

	device_numa_id = ((struct device *)pdev_rsrc->dev)->numa_node;
	
	/*for x86, although 'lscpu' show there is 1 numa node
	 * but for the pcie device, the numa node is -1 */
	if (device_numa_id > nr_node_ids)
	{
		device_numa_id = 0;
	}

	cpu_num = DEFAULT_BIND_CPU_NUMS;
	cpu_offset = cpu_per_numa*device_numa_id + numa_pcie_nums[device_numa_id] * DEFAULT_BIND_CPU_NUMS;
	if ((cpu_offset+cpu_num) > (cpu_online_num - 1)) {
		hal_warn(pdev_rsrc->dev, "%s: numa%d has too many cards or cpu_num default value is too large, set card%d cpu_offset to 0, cpu_num to 1\n", __func__, device_numa_id, pdev_rsrc->pcie_func_idx);
		cpu_offset = 0;
		cpu_num = 1;
	}
	pdev_rsrc->bind_cpu_num = cpu_num;
	pdev_rsrc->bind_cpu_offset = cpu_offset;
	numa_pcie_nums[device_numa_id]++;
	return 0;

error:
	pdev_rsrc->bind_cpu_enable = 0;
	enable_bind_cpu = 0;
	bind_numa_config_deinit();
	return -ENOMEM;
}

void bind_numa_config_deinit() {
	if (numa_pcie_nums) {
		kfree(numa_pcie_nums);
		numa_pcie_nums = NULL;
	}
	return;
}

int fh2m_hal_bind_numa_cpu_config_store(inno_dev *dev, const char *buf) {
	int is_set_num = 0, is_set_offset = 0, ret;
	unsigned int cpu_num, cpu_offset;
	unsigned int cpu_online_num = num_online_cpus();
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(dev);

	ret = sscanf(buf,"num %u", &cpu_num);
	if(ret == 1) {
		is_set_num = 1;
	}

	ret = sscanf(buf,"offset %u", &cpu_offset);
	if(ret == 1) {
		is_set_offset = 1;
	}

	ret = -EINVAL;
	if (is_set_num && (cpu_num <= cpu_online_num - pdev_rsrc->bind_cpu_offset)) {
		pdev_rsrc->bind_cpu_num = cpu_num;
		ret = 0;
	}else if (is_set_offset && (cpu_offset <= cpu_online_num - pdev_rsrc->bind_cpu_num)) {
		pdev_rsrc->bind_cpu_offset = cpu_offset;
		ret = 0;
	}else {
		hal_warn(pdev_rsrc->dev, "%s: correct text should be like \"num/offset x\", x is a num, or your num is too large\n", __func__);
	}

	return ret;
}

int fh2m_hal_bind_numa_cpu_config_show(inno_dev *dev, char *buf) {
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(dev);
	unsigned int i;
	ssize_t count = 0;
	
	count += fh2m_inno_sprintf(buf + count, 64, "Chip id is %d, bind cpu id is ", pdev_rsrc->pcie_func_idx);
	for (i = 0; i < pdev_rsrc->bind_cpu_num; i++) {
		count += fh2m_inno_sprintf(buf + count, 4, "%u ", pdev_rsrc->bind_cpu_offset+i);
	}
	count += fh2m_inno_sprintf(buf + count, 4, " \n");
	return count;
}

#endif //END CONFIG_NUMA __INNO_CONTAINER__
