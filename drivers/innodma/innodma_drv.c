/*************************************************************************/ /*!
@File           innodma.c
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

#include "innodma.h"
#include "inno_misc.h"
#include "inno_debug.h"

#define DRIVER_DESC "Innosilicon Technologies DMA Driver"

unsigned int s_dma_debug = 0x0;
MODULE_PARM_DESC(s_dma_debug, "INNO-DMA debug param, default is 0x0");
module_param_named(s_dma_debug, s_dma_debug, uint, 0600);

#if defined(SUPPORT_DMA_TRANSFER)
INNO_EXT_SYM(fh2m_innodma_memcpy);
INNO_EXT_SYM(fh2m_innodma_memcpy_for_smallbar_sg);
INNO_EXT_SYM(fh2m_innodma_compat_memcpy_sg);
INNO_EXT_SYM(fh2m_innodma_split_packet_and_memcpy);
INNO_EXT_SYM(fh2m_innodma_memset);
INNO_EXT_SYM(fh2m_innodma_memset_sg);
INNO_EXT_SYM(fh2m_innodma_transfer_fast);
#endif

/*add sub drivers here*/
extern struct platform_driver axi_dma_driver;
extern struct platform_driver pcie_dma_driver;

#ifdef CONFIG_DRM_INNO_DMA
int innodma_driver_register(void)
#else
static int __init inno_dma_init(void)
#endif
{
	int ret = 0;

	ret = platform_driver_register(&axi_dma_driver);
	if (ret) {
		inno_error("failed to register axi dma driver\n");
		return ret;
	}

	ret = platform_driver_register(&pcie_dma_driver);
	if (ret) {
		inno_error("failed to register pcie dma driver\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_DRM_INNO_DMA
void innodma_driver_unregister(void)
#else
static void __exit inno_dma_exit(void)
#endif
{
	platform_driver_unregister(&axi_dma_driver);
	platform_driver_unregister(&pcie_dma_driver);
}

#ifndef CONFIG_DRM_INNO_DMA
module_init(inno_dma_init);
module_exit(inno_dma_exit);

MODULE_AUTHOR("Innosilicon Technologies Ltd. <support@innosilicon.com.cn>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("Dual MIT/GPL");
#endif
