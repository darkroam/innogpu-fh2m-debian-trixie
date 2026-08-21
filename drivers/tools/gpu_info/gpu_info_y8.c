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
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/proc_fs.h>

#include "gpu_info_y8.h"
#include "inno_mm.h"
#include "inno_misc.h"
#include "hal_interface.h"
#include "hal.h"
#include "kernel_compatibility.h"
#include "inno_debug.h"

#define GPUINFO_PROC_NAME    "gpuinfo"
static s32 gpuinfo_proc_open(struct inode *inode, struct file *file)
{
	if (NULL == inode || NULL == file) {
		inno_error("Invalid argument: the pointer of inode is [%px], file is [%px]!\n", (char *)inode , (char *)file);
		return -1;
	}

	return 0;
}
static ssize_t gpuinfo_proc_read(struct file *file, char __user *buffer, size_t count, loff_t *offp)
{
	char *buf = NULL;
	ssize_t idx = 0;
	int res = 0;
	struct device *pdev = NULL;

	if (*offp > 0) {
		*offp = 0;
		return 0;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0))
	pdev = (struct device *)PDE_DATA(file_inode(file));
#else
	pdev = (struct device *)pde_data(file_inode(file));
#endif
	buf = fh2m_inno_kmalloc_kernel(PAGE_SIZE);
	if (!buf)
		return -1;

	res = fh2m_hal_get_chip_gpuinfo(pdev, buf, &idx);
	if (res < 0) {
		fh2m_inno_kfree(buf);
		return 0;
	}

	if (idx >= PAGE_SIZE)
		idx = PAGE_SIZE - 1;

	if (fh2m_inno_copy_to_user(buffer, buf, idx)) {
		fh2m_inno_kfree(buf);
		return -1;
	}

	fh2m_inno_kfree(buf);

	*offp += idx;

	return idx;
}
static s32 gpuinfo_proc_release(struct inode *inode, struct file *file)
{
	if (NULL == inode || NULL == file) {
		inno_error("Invalid argument: the pointer of inode is [%px], file is [%px]!\n", (char *)inode , (char *)file);
		return -1;
	}

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
struct file_operations gpuinfo_driver = {
	.owner = THIS_MODULE,
	.open = gpuinfo_proc_open,
	.read = gpuinfo_proc_read,
	.release = gpuinfo_proc_release
};
#else
struct proc_ops gpuinfo_driver = {
	.proc_open = gpuinfo_proc_open,
	.proc_read = gpuinfo_proc_read,
	.proc_release = gpuinfo_proc_release
};
#endif

int inno_gpu_info_y8_init(inno_dev *dev_priv)
{
	struct proc_dir_entry *gpuinfo_file;
	char name[128];
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(dev_priv);

	fh2m_inno_sprintf(name, 128, "%s_%d", GPUINFO_PROC_NAME, pdev_rsrc->pcie_func_idx);

	gpuinfo_file = proc_create_data(name, 0777, NULL, &gpuinfo_driver, (void *)dev_priv);
	if (!gpuinfo_file) {
		inno_error("create proc gpuinfo_%d node failed\n", pdev_rsrc->pcie_func_idx);
		return -1;
	}

	return 0;
}

void inno_gpu_info_y8_exit(inno_dev *dev_priv)
{
	char name[128];
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(dev_priv);

	fh2m_inno_sprintf(name, 128, "%s_%d", GPUINFO_PROC_NAME, pdev_rsrc->pcie_func_idx);
	remove_proc_entry(name, NULL);
}

