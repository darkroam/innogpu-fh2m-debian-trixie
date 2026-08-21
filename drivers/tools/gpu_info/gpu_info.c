/*************************************************************************/ /*!
@File           gpu_info.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/string_helpers.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/crc32.h>

#include "gpu_info.h"
#include "innogpu.h"
#include "hal_interface.h"
#include "kernel_compatibility.h"
#include "kgc_gpu_info.h"
#include "gpu_info_y8.h"
#include "gpu_info_innoml.h"
#include "inno_misc.h"

#define INNO_SYSDBG_DEVICE_NAME    "sysdbg"
#define SYSDBG_MAGIC 's'
#define SYSDBG_GET_SYSINFO _IOR(SYSDBG_MAGIC, 1, unsigned long)
#define SYSDBG_GET_VRAMINFO _IOR(SYSDBG_MAGIC, 2, unsigned long)

#define KBUILD_SYSDBG "sysdbg"
#define pr_fmt_sysdbg(fmt) "[%s][%s:%d]" fmt, KBUILD_SYSDBG,__func__,__LINE__
#define DRIVER_DESC "Innosilicon Technologies sysdbg Driver"

#define sysdbg_dbg(dev,fmt, ...)\
		dev_printk(KERN_DEBUG,dev,pr_fmt_sysdbg(fmt), ##__VA_ARGS__)
#define sysdbg_info(dev,fmt, ...)\
		dev_printk(KERN_INFO,dev,pr_fmt_sysdbg(fmt), ##__VA_ARGS__)
#define sysdbg_warn(dev,fmt, ...)\
		dev_printk(KERN_WARNING,dev,pr_fmt_sysdbg(fmt), ##__VA_ARGS__)
#define sysdbg_error(dev,fmt, ...)\
		dev_printk(KERN_ERR,dev,pr_fmt_sysdbg(fmt), ##__VA_ARGS__)

#define ENV_SIZE			(0x2000)
#define DEVELOP_MODE_PASSWD   "inno@123456"
#define CUSTOM_TAB         "custom"
#define HWINFO_TAB         "hwinfo"
#define HWINFO_BUF_SIZE         (2*PAGE_SIZE)

int monitor_time = 100;
module_param(monitor_time, int, 0444);
MODULE_PARM_DESC(monitor_time,"get axi bandwidth info monitor time, default is 100");

static bool develop_mode = false;
static int hwinfo_index = 0;
static ssize_t gpu_static_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	fh2m_hal_get_chip_static_info(dev->parent,buf,&count);

	return count;
}

static ssize_t gpu_dynamic_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	fh2m_hal_get_chip_dyn_info(dev->parent,buf,&count,develop_mode);

	return count;
}

static ssize_t hwinfo_idx_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	if (strncmp(buf, HWINFO_TAB, strlen(HWINFO_TAB)) == 0) {
		hwinfo_index = 1;
	} else if (strncmp(buf, CUSTOM_TAB, strlen(CUSTOM_TAB)) == 0) {
		hwinfo_index = 2;
	}
	return len;
}

static ssize_t develop_mode_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	if (strncmp(buf,DEVELOP_MODE_PASSWD,strlen(DEVELOP_MODE_PASSWD)))
		develop_mode = false;
	else
		develop_mode = true;

	return len;
}

static ssize_t hwinfo_read(struct file *filp, struct kobject *kobj, struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	ssize_t cnt = 0;
	char *buffer = fh2m_inno_kvmalloc_kernel(HWINFO_BUF_SIZE); //count最大值是4096即每次最多copy一个PAGE_SIZE(4096)到user space,copy多次直到count为0,buffer存储显示的数据以便获取上次copy的偏移地址

	if (!buffer) {
		sysdbg_error(dev, "fh2m_inno_kvmalloc_kernel 0x%lx failed", HWINFO_BUF_SIZE);
		return 0;
	}

	cnt = fh2m_hal_show_hw_info(dev->parent, buffer, HWINFO_BUF_SIZE, hwinfo_index);

	if (off >= cnt) {
		fh2m_inno_kvfree(buffer);
		return 0;
	}

	if (off < 0) {
		fh2m_inno_kvfree(buffer);
		return -EINVAL;
	}

	if (cnt - off < count)
		count = cnt - off;

	fh2m_inno_strncpy(buf , buffer + off, count);

	fh2m_inno_kvfree(buffer);

	return count;
}

static ssize_t driver_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	fh2m_hal_get_driver_info(dev->parent,buf,&count,develop_mode);

	return count;
}

static ssize_t fw_env_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0,ret;
	char *env, *nxt;
	void __iomem* env_reserved_vram;

	if (!develop_mode)
		return 0;

	fh2m_hal_init_reserved_vram(dev->parent);

	env_reserved_vram = fh2m_hal_get_env_reserved_vram(dev->parent);
	ret = fh2m_hal_trigger_mcu_intr(dev->parent,INTR_TYPE_GET_ENV);
	if (ret < 0) {
		return 0;
	}
	/*等待MCU从NorFlash中将env读取到显存中*/
	msleep(1000);
	count += sprintf(buf + count, "mcu-fw env info:\n");
	/*起始4个字节crc校验值*/
	env = (char *)(env_reserved_vram) + 4;
	for (; *env ; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= (char *)(env_reserved_vram) + ENV_SIZE) {
				sysdbg_error(dev, "## Error: ""environment not terminated\n");
				return count;
			}

		}
		count += sprintf(buf + count, "%s\n",env);
	}

	fh2m_hal_deinit_reserved_vram(dev->parent);

	return count;
}

static unsigned int uboot_env_crc32(unsigned int crc, unsigned char const *p, unsigned int len)
{
	#define CRCPOLY_LE					(0xedb88320)
	int i;
	unsigned int crc_len = crc;

	while (len--) {
		crc_len ^= *p++;
		for (i = 0; i < 8; i++){
			crc_len = (crc_len >> 1) ^ ((crc_len & 1) ? CRCPOLY_LE : 0);
		}
	}
	return crc_len;
}


/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static char *envmatch (char * s1, char * s2)
{

	while (*s1 == *s2++)
		if (*s1++ == '=')
			return s2;
	if (*s1 == '\0' && *(s2 - 1) == '=')
		return s2;
	return NULL;
}

/*
 * Set/Clear a single variable in the environment.
 * This is called in sequence to update the environment
 * in RAM without updating the copy in flash after each set
 */
static int env_write(struct device *dev,char *name, char *value)
{
	int len;
	char *env, *nxt;
	char *oldval = NULL;
	void __iomem* env_reserved_vram;

	fh2m_hal_init_reserved_vram(dev->parent);

	env_reserved_vram = fh2m_hal_get_env_reserved_vram(dev->parent);

	/*调过env区的4个字节的头部*/
	nxt = env = (char *)(env_reserved_vram) + 4;

	/*
	 * search if variable with this name already exists
	 */
	for (; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= (char *)(env_reserved_vram + ENV_SIZE)) {
				sysdbg_error(dev, "## Error: ""environment not terminated\n");
				return -1;
			}
		}
		if ((oldval = envmatch (name, env)) != NULL)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval) {
		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	/* Delete only ? */
	if (!value || !strlen(value))
		return 0;

	/*
	 * Append new definition at the end
	 */
	env = (char *)(env_reserved_vram) + 4;
	for (; *env || *(env + 1); ++env);
	if (env > (char *)(env_reserved_vram + 4))
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CONFIG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */
	len += strlen(value) + 1;

	if (len > ((char *)(env_reserved_vram + ENV_SIZE) - env)) {
		sysdbg_error(dev,"Error: environment overflow, \"%s\" deleted\n",name);
		return -1;
	}

	while ((*env = *name++) != '\0') env++;
	*env = '=';
	while ((*++env = *value++) != '\0');

	/* end is marked with double '\0' */
	*++env = '\0';

	/*update crc*/
	*((unsigned int *)(env_reserved_vram)) = uboot_env_crc32(0,  (char *)(env_reserved_vram + 4), (ENV_SIZE - 4));

	fh2m_hal_deinit_reserved_vram(dev->parent);

	return 0;
}

static ssize_t fw_env_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	char env_name[128],env_val[128];
	int ret = 0;

	if (!develop_mode)
		return 0;

	memset(env_name,0,sizeof(env_name));
	memset(env_val,0,sizeof(env_val));

	sscanf(buf,"%127s %127s",env_name,env_val);

	ret = env_write(dev,env_name,env_val);
	if (ret) {
		sysdbg_error(dev,"env write failed !\n");
		return len;
	}

	ret = fh2m_hal_trigger_mcu_intr(dev->parent,INTR_TYPE_SET_ENV);
	if (ret < 0) {
		return 0;
	}
	return len;
}


/*AXI带宽统计功能，使用该功能时，必须作为第2个驱动加载*/
static ssize_t axi_bd_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	fh2m_hal_get_axi_bandwidth_info(dev->parent,buf,&count,monitor_time,develop_mode);//100ms

	return count;
}

/*AXI 延时统计功能*/
static ssize_t axi_lt_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	fh2m_hal_get_axi_latency_info(dev->parent,buf,&count, monitor_time, develop_mode);

	return count;
}

/******出厂信息********/
static DEVICE_ATTR_RO(gpu_static_info);

/******动态调试区********/
static DEVICE_ATTR_RO(gpu_dynamic_info);
static DEVICE_ATTR_RO(driver_info);
static DEVICE_ATTR_WO(develop_mode);
static DEVICE_ATTR_WO(hwinfo_idx);
static BIN_ATTR_RO(hwinfo, HWINFO_BUF_SIZE);

static DEVICE_ATTR(fw_env,S_IRUGO|S_IWUSR,fw_env_show,fw_env_store);
//static DEVICE_ATTR_RO(ddr_info);
static DEVICE_ATTR_RO(axi_bd_info);
static DEVICE_ATTR_RO(axi_lt_info);

static struct attribute *innohw_attributes[] = {
	&dev_attr_gpu_static_info.attr,
	&dev_attr_gpu_dynamic_info.attr,
	&dev_attr_develop_mode.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_fw_env.attr,
	&dev_attr_axi_bd_info.attr,
	&dev_attr_axi_lt_info.attr,
	&dev_attr_hwinfo_idx.attr,
	//&dev_attr_ddr_info.attr,
	NULL
};

static struct bin_attribute *innohw_bin_attributes[] = {
	&bin_attr_hwinfo,
	NULL
};

static const struct attribute_group innohw_attr_group = {
	.attrs = innohw_attributes,
	.bin_attrs = innohw_bin_attributes,
};

#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
static ssize_t bind_cpu_config_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{

	int ret = fh2m_hal_bind_numa_cpu_config_store(dev->parent, buf);
	if(ret) {
		sysdbg_error(dev, "## input text is error, set cpu config failed\n");
		return ret;
	}
	return count;
}
static ssize_t bind_cpu_config_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return fh2m_hal_bind_numa_cpu_config_show(dev->parent, buf);
}
static DEVICE_ATTR_RW(bind_cpu_config);

static struct attribute *inno_numa_attributes[] = {
	&dev_attr_bind_cpu_config.attr,
	NULL
};

static const struct attribute_group inno_numa_attr_group = {
	.attrs = inno_numa_attributes,
};

static int sys_bind_numa_init(struct platform_device *pdev)
{
	int ret = 0;
	if(fh2m_bind_cpu_is_enable(pdev->dev.parent)) {
		ret = sysfs_create_group(&pdev->dev.kobj, &inno_numa_attr_group);
	}
	return ret;
}

static void sys_bind_numa_deinit(struct platform_device *pdev)
{
	if(fh2m_bind_cpu_is_enable(pdev->dev.parent)) {
		sysfs_remove_group(&pdev->dev.kobj, &inno_numa_attr_group);
	}
}
#endif //END CONFIG_NUMA __INNO_CONTAINER__


static int sys_debug_probe(struct platform_device *pdev)
{
	int ret = 0;
#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	ret = sys_bind_numa_init(pdev);
	if(ret) {
		return ret;
	}
#endif //END CONFIG_NUMA __INNO_CONTAINER__

	ret = sysfs_create_group(&pdev->dev.kobj, &innohw_attr_group);

#if defined(CONFIG_KYLINOS_DESKTOP)
	ret = sys_gpu_plugin_init(pdev->dev.parent);
	if (ret) {
		return -1;
	}
#endif
	ret = inno_gpu_info_y8_init(pdev->dev.parent);
	if (ret)
		return -1;

	ret = InnoGpuCharDevInitDevice(pdev->dev.parent);
	if (ret)
		return -1;
	return ret;
}

static int sys_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	sys_bind_numa_deinit(pdev);
#endif //END CONFIG_NUMA __INNO_CONTAINER__
	sysfs_remove_group(&dev->kobj, &innohw_attr_group);
#if defined(CONFIG_KYLINOS_DESKTOP)
	sys_gpu_plugin_exit(pdev->dev.parent);
#endif
	inno_gpu_info_y8_exit(pdev->dev.parent);

	InnoGpuCharDevDeInitDevice(pdev->dev.parent);

	return 0;
}

static int sysdbg_suspend(struct device *dev)
{
	return 0;
}

static int sysdbg_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sysdbg_pm_ops = {
	.suspend = sysdbg_suspend,
	.resume = sysdbg_resume,
};

static struct platform_device_id s_sysdbg_device_id_table[] = {
	{.name = INNO_SYSDBG_DEVICE_NAME,.driver_data = 0},
	{},
};

MODULE_DEVICE_TABLE(platform, s_sysdbg_device_id_table);

struct platform_driver sysdbg_driver = {
	.probe		= sys_debug_probe,
	.remove		= sys_debug_remove,
	.driver = {
		.name	= INNO_SYSDBG_DEVICE_NAME,
		.pm = &sysdbg_pm_ops,
	},
	.id_table = s_sysdbg_device_id_table,
};

#ifdef CONFIG_DRM_INNO_DEBUG
void inno_gpu_info_exit(void)
#else
static void __exit sys_debug_exit(void)
#endif
{
	platform_driver_unregister(&sysdbg_driver);
	InnoGpuCharDevDeInitDriver();
}

#ifdef CONFIG_DRM_INNO_DEBUG
int inno_gpu_info_init(void)
#else
static int __init sys_debug_init(void)
#endif
{
	int err = 0;
	err = InnoGpuCharDevInitDriver();
	if (err)
		return -1;
	err = platform_driver_register(&sysdbg_driver);
	if (err) {
		pr_err("failed to register sysdbg driver\n");
		return err;
	}
	return 0;
}

#ifndef CONFIG_DRM_INNO_DEBUG
module_init(sys_debug_init);
module_exit(sys_debug_exit);

MODULE_AUTHOR("Innosilicon Technologies Ltd. <support@innosilicon.com.cn>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("Dual MIT/GPL");
#endif
