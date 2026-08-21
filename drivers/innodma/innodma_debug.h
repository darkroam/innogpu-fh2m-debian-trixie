/*************************************************************************/ /*!
@File           innodma_debug.h
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

#include "inno_plat_dev.h"
#include <linux/kern_levels.h>

/* dma debug level */
#define INNODMA_LEVEL_DEBUG     (1 << 0)
#define INNODMA_LEVEL_INFO      (1 << 1)
#define INNODMA_LEVEL_NOTICE    (1 << 2)
#define INNODMA_LEVEL_WARN      (1 << 3)
#define INNODMA_LEVEL_ERROR     (1 << 4)
#define INNODMA_LEVEL_CRIT      (1 << 5)
#define INNODMA_LEVEL_ALERT     (1 << 6)
#define INNODMA_LEVEL_EMESG     (1 << 7)

/* dma debug module */
#define INNODMA_DEBUG_PCIE_DMA  (1 << 8)
#define INNODMA_DEBUG_AXI_DMA   (1 << 9)
#define INNODMA_DEBUG_DMA       (1 << 10)

extern unsigned int s_dma_debug;

void innodma_print(inno_dev *dev, char *debug_level, const char *format, ...);

#ifdef DMA_DEBUG_ENABLE
#define DMA_DEBUG
#endif

#define INNODMA "innodma"
#define pr_fmt0(fmt) "[%s][%s:%d]" fmt,INNODMA, __func__, __LINE__

#define innodma_debug(dev, debug_param, fmt, ...) \
	do { \
		if (!((debug_param) & (s_dma_debug))) { \
			break; \
		} \
		if (!((INNODMA_LEVEL_DEBUG) & (s_dma_debug))) { \
			break; \
		} \
		innodma_print(dev, KERN_DEBUG, pr_fmt0(fmt), ##__VA_ARGS__); \
	} while (0);

#define innodma_info(dev, debug_param, fmt, ...) \
	do { \
		if (!((debug_param) & (s_dma_debug))) { \
			break; \
		} \
		if (!((INNODMA_LEVEL_INFO) & (s_dma_debug))) { \
			break; \
		} \
		innodma_print(dev, KERN_INFO, pr_fmt0(fmt), ##__VA_ARGS__); \
	} while (0);

#define innodma_warn(dev, fmt, ...) \
	do { \
		innodma_print(dev, KERN_WARNING, pr_fmt0(fmt), ##__VA_ARGS__); \
	} while (0);

#define innodma_error(dev, fmt, ...) \
	do { \
		innodma_print(dev, KERN_ERR, pr_fmt0(fmt), ##__VA_ARGS__); \
	} while (0);
