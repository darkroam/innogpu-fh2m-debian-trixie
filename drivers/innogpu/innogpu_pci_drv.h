/*************************************************************************/ /*!
@File           innogpu_pci_drv.h
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

#ifndef __INNOGPU_PCI_DRV_H__
#define __INNOGPU_PCI_DRV_H__

#include "hal.h"
#include "hal_interface.h"
#include "inno_debug.h"

/********************** NE PCIE ID******************/
#define PCI_VENDOR_ID_NE		(0x10EE)
#define PCI_DEVICE_ID_NE		(0x8034)

/****************paladium PCIE ID*******************/
#define PCI_VENDOR_ID_PAL		(0x1EC8)// (0x10DE)
#define PCI_DEVICE_ID_PAL		(0x1EB8)//(0x1EB8)

/**********************VF PCIE ID*******************/
#define PCI_VENDOR_ID_VF		(0x1EC8)// (0x10DE)
#define PCI_DEVICE_ID_VF		(0x1EA8)//(0x1EB8)
#define PCI_DEVICE_ID_G1P_PAL_VF		(0x8811)

/*
 * the pci device id and pci vendor id macro names for desensitized gpu must follow this format:
 * PCI_VENDOR_ID_.*_ALIAS_[0-9]/PCI_DEVICE_ID_.*_ALIAS_[0-9]
 * delete the desensitized pci id when execu rename script,refer to submission 57926
 * */
/**********************Alias_1 Compatible PCIE ID***********/
#if defined(DESENSITIZED) && (DESENSITIZED == 1)
/**********************Alias_2 Compatible PCIE ID***********/
#elif defined(DESENSITIZED) && (DESENSITIZED == 2)
/**********************Compatible PCIE ID***********/
#else
#define PCI_VENDOR_ID_INNOSILICON	(0x1EC8)
#define PCI_DEVICE_ID_INNOSILICON	(0x1EB8)
#define PCI_DEVICE_ID_G1_SOC		(0x8800)
#define PCI_DEVICE_ID_G1_PAL		(0x8800)
#define PCI_DEVICE_ID_G1_NE			(0x8800)
#define PCI_DEVICE_ID_G0_SOC		(0x9800)
#define PCI_DEVICE_ID_G0_PAL		(0x9800)
#define PCI_DEVICE_ID_G0_NE			(0x9800)
#define PCI_DEVICE_ID_G1P_SOC		(0x8810)
#define PCI_DEVICE_ID_G1P_PAL		(0x8810)
#define PCI_DEVICE_ID_G1P_NE		PCI_DEVICE_ID_NE
#define PCI_DEVICE_ID_G0M_SOC		(0x9810)
#define PCI_DEVICE_ID_G0M_PAL		(0x9810)
#define PCI_DEVICE_ID_G0M_NE		PCI_DEVICE_ID_NE

/* Begain: for G3 VID PID */
#define PCI_DEVICE_ID_G3_SOC			(0x8900)
#define PCI_DEVICE_ID_G3_PAL			(0x8900)
#define PCI_DEVICE_ID_G3_PAL_VF			(0x8901)
#define PCI_DEVICE_ID_G3_AUDIO_SOC		(0x8902)
#define PCI_DEVICE_ID_G3_AUDIO_PAL		(0x8902)

#if defined(NE_VARIANT) && (NE_VARIANT==0)
/* our self pcie ASIC design */
#define PCI_VENDOR_ID_G3_NE				(0x1EC8)
#define PCI_DEVICE_ID_G3_NE				(0x8900)
#define PCI_DEVICE_ID_G3_AUDIO_NE		(0x8902)
#else
/* xilinx pcie use vid:pid 0x10ee 0x8034*/
#define PCI_VENDOR_ID_G3_NE				(PCI_VENDOR_ID_NE)
#define PCI_DEVICE_ID_G3_NE				(PCI_DEVICE_ID_NE)
#define PCI_DEVICE_ID_G3_AUDIO_NE		(0x8902)
#endif
/* End: for G3 VID PID */

#endif

#define INNO_GPU_DEBUGFS_NAME		"innogpu"

#define KBUILD_PCIE "pcie"
#define pr_fmt_pcie(fmt) "[%s][%s:%d]" fmt,KBUILD_PCIE,__func__,__LINE__
#if defined(INNO_GPU_LOG)
#define pcie_dbg(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_DEBUG,dev,pr_fmt_pcie(fmt), ##__VA_ARGS__)
#define pcie_info(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_INFO,dev,pr_fmt_pcie(fmt), ##__VA_ARGS__)
#else
#define pcie_dbg(dev,fmt, ...)
#define pcie_info(dev,fmt, ...)
#endif

#define pcie_notice(dev, fmt, ...) \
		fh2m_inno_dev_printk(KERN_NOTICE, dev, pr_fmt_pcie(fmt), ##__VA_ARGS__)
#define pcie_warn(dev, fmt, ...) \
		fh2m_inno_dev_printk(KERN_WARNING, dev, pr_fmt_pcie(fmt), ##__VA_ARGS__)
#define pcie_error(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_ERR,dev,pr_fmt_pcie(fmt), ##__VA_ARGS__)

int dev_quick_test(struct dev_rsrc* pdev_rsrc);

#endif // __INNOGPU_PCI_DRV_H__

