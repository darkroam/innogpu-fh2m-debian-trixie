/*************************************************************************/ /*!
@File			innovpu.h
@Title			innovpu driver header
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu driver header
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

#ifndef __INNOVPU_H__
#define __INNOVPU_H__

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kfifo.h>
//#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/suspend.h>
#include "innovpu_internal.h"
typedef struct vpu_drv_ctx {
	dev_t *cdevid;
	struct class *vpucls;
	struct dentry *vpu_dir;
	struct dentry *vpu_node;
	struct cdev vpucdev;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif
    vpu_drv_context drv_context;
}vpu_drv_ctxs;

typedef struct __vpu_drv_info vpu_drv_info;

typedef struct _vpu_timer_t {
	struct timer_list timer;
	unsigned long jiffies;
	uint64_t last_timestamp;
	vpu_drv_info *drv_info;
	bool timer_suspend;
} vpu_timer_t;

typedef struct __vpu_drv_info {
	struct dentry *vpu_dir;
	struct dentry *info_node;
	struct dentry *mem_node;
	struct dentry *debug_node;
	vpu_drv_ctxs **vpu_ctxs;
	atomic64_t vpu_num;
	vpu_timer_t *workload_timer;
}  vpu_drv_info;

#endif
