/*
 * * Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
 * * Dual MIT/GPLv2
 * *
 * * The contents of this file are subject to the MIT license as set out below.
 * *
 * * Permission is hereby granted, free of charge, to any person obtaining a copy
 * * of this software and associated documentation files (the "Software"), to deal
 * * in the Software without restriction, including without limitation the rights
 * * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * * copies of the Software, and to permit persons to whom the Software is
 * * furnished to do so, subject to the following conditions:
 * *
 * * The above copyright notice and this permission notice shall be included in
 * * all copies or substantial portions of the Software.
 * *
 * * Alternatively, the contents of this file may be used under the terms of
 * * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * * of GPL are applicable instead of those above.
 * *
 * * If you wish to allow use of your version of this file only under the terms of
 * * GPL, and not to allow others to use your version of this file under the terms
 * * of the MIT license, indicate your decision by deleting the provisions above
 * * and replace them with the notice and other provisions required by GPL as set
 * * out in the file called "GPL-COPYING" included in this distribution. If you do
 * * not delete the provisions above, a recipient may use your version of this file
 * * under the terms of either the MIT license or GPL.
 * *
 * * This License is also included in this distribution in the file called
 * * "MIT-COPYING".
 * *
 * * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * */
#include <linux/version.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include "inno_misc.h"
#include "inno_uaccess.h"
#include "kernel_compatibility.h"

void fh2m_inno_uaccess_enable(void)
{
#if defined(CONFIG_ARM64) && defined(CONFIG_ARM64_SW_TTBR0_PAN)
	uaccess_enable_privileged();
#endif
}
INNO_EXT_SYM(fh2m_inno_uaccess_enable);

void fh2m_inno_uaccess_disable(void)
{
#if defined(CONFIG_ARM64) && defined(CONFIG_ARM64_SW_TTBR0_PAN)
	uaccess_disable_privileged();
#endif
}
INNO_EXT_SYM(fh2m_inno_uaccess_disable);

void fh2m_inno_uaccess_clflush(const void *pvStart, const void *pvEnd)
{
#if defined(CONFIG_X86)
	uint8_t *Base;
	uint8_t *Start = (uint8_t *)pvStart;
	uint8_t *End = (uint8_t *)pvEnd;
#endif

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,168))
	__uaccess_begin();
#endif

#if defined(CONFIG_X86)
	for (Base = Start; Base < End; Base += boot_cpu_data.x86_clflush_size)
	{
		clflush(Base);
	}
#endif

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,168))
	__uaccess_end();
#endif
}
INNO_EXT_SYM(fh2m_inno_uaccess_clflush);
