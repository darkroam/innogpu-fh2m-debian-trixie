/*************************************************************************/ /*!
@File           innosmmu_drv.c
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include "hal.h"
#include "hal_interface.h"
#include "innosmmu_drv.h"
#ifndef CONFIG_DRM_INNO_SMMU
	#include "inno_ra.h"
	#include "innosmmu_func.h"
#endif

unsigned int s_smmu_debug = 0x7;
module_param(s_smmu_debug, uint, 0600);
MODULE_PARM_DESC(s_smmu_debug, " innosmmu loglevel. bit0: ERROR, bit1:WRAN, bit2:NOTIC, bit3: INFO, bit4: DEBUG");

static ssize_t pm_show_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	uint32_t en;
	if(!kstrtouint(buf, 10, &en)) {
		if(en) {
			innosmmu_pm_enable(dev->driver_data);
		}
		else {
			innosmmu_pm_disable(dev->driver_data);
		}
	}
	else {
		len = 0;
	}
	return len;
}

static ssize_t pm_show_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	innosmmu_pm_read(dev->driver_data, buf, &count);
	return count;
}

static DEVICE_ATTR(pm_show, S_IRUGO | S_IWUSR, pm_show_show, pm_show_store);

static struct attribute *innosmmu_pm_attributes[] = {
	&dev_attr_pm_show.attr,
	NULL
};
static const struct attribute_group innosmmu_pm_attr_group = {
	.attrs = innosmmu_pm_attributes,
};

int innosmmu_pm_init(inno_dev *dev)
{
	if(sysfs_create_group(&((struct device *)dev)->kobj, &innosmmu_pm_attr_group)) {
		dev_err(dev, "sysfs_create_group failed\n");
		return -1;
	}
	return 0;
}

void innosmmu_pm_deinit(inno_dev *dev)
{
	sysfs_remove_group(&((struct device *)dev)->kobj, &innosmmu_pm_attr_group);
}

static struct platform_device_id inno_smmu_platform_device_id_table[] = {
	{ .name = INNO_SMMU_DEVICE_NAME, .driver_data = 0 },
	{ },
};

struct platform_driver inno_smmu_driver = {
	.probe      = (int (*)(struct platform_device *))inno_smmu_device_probe,
	.remove     = (void (*)(struct platform_device *))inno_smmu_device_remove,
	.driver = {
		.name = INNO_SMMU_DEVICE_NAME,
	},
	.id_table = inno_smmu_platform_device_id_table,
};


#ifdef CONFIG_DRM_INNO_SMMU
	int innosmmu_driver_register(void)
#else
	static int __init inno_smmu_init(void)
#endif
{
	int ret = 0;

	ret = platform_driver_register(&inno_smmu_driver);
	if(ret) {
		pr_err("failed to register innosmmu driver\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_DRM_INNO_SMMU
	void innosmmu_driver_unregister(void)
#else
	static void __exit inno_smmu_exit(void)
#endif
{
	platform_driver_unregister(&inno_smmu_driver);
}

#ifndef CONFIG_DRM_INNO_SMMU
	INNO_EXT_SYM(fh2m_INNO_RA_Alloc);
	INNO_EXT_SYM(fh2m_INNO_RA_Free);
	INNO_EXT_SYM(fh2m_innosmmu_pages_map);
	INNO_EXT_SYM(fh2m_innosmmu_pages_unmap);
	INNO_EXT_SYM(fh2m_innosmmu_va_alloc);
	INNO_EXT_SYM(fh2m_innosmmu_va_free);
	INNO_EXT_SYM(fh2m_innosmmu_va_alloc_and_map_pages);
	INNO_EXT_SYM(fh2m_innosmmu_va_free_and_unmap_pages);
	INNO_EXT_SYM(fh2m_innosmmu_va_alloc_and_map_sg);
	INNO_EXT_SYM(fh2m_innosmmu_va_free_and_unmap_sg);
	INNO_EXT_SYM(fh2m_gtt_memory_sg_table_init);
	INNO_EXT_SYM(fh2m_gtt_memory_sg_table_free);

	module_init(inno_smmu_init);
	module_exit(inno_smmu_exit);

	MODULE_AUTHOR("Innosilicon Technologies Ltd. <support@innosilicon.com.cn>");
	MODULE_DESCRIPTION("Innosilicon Technologies smmu Driver");
	MODULE_LICENSE("Dual MIT/GPL");
#endif
