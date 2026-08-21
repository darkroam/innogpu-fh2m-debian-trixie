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
#if defined(CONFIG_MTRR)
#include <asm/mtrr.h>
#endif
#include <asm/io.h>
#include <linux/version.h>
#include <linux/smp.h>
#include "inno_misc.h"
#include "inno_mtrr.h"

#if defined(CONFIG_X86)
uint16_t fh2m_inno_get_clflush_size(void)
{
	return boot_cpu_data.x86_clflush_size;
}
INNO_EXT_SYM(fh2m_inno_get_clflush_size);
#endif

void fh2m_inno_clflush(volatile void *__p)
{
#if defined(CONFIG_X86)
	clflush(__p);
#endif
}
INNO_EXT_SYM(fh2m_inno_clflush);

#if defined(CONFIG_MTRR)
bool fh2m_inno_pci_clear_reaource_mtrrs(unsigned long long start, unsigned long long end, int *res)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	*res = arch_io_reserve_memtype_wc(start, end - start);
	if (*res) {
		return false;
	}
#endif
	*res = arch_phys_wc_add(start, end - start);
	if (*res < 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
		arch_io_free_memtype_wc(start, end - start);
#endif
		return false;
	}

	return true;
}
INNO_EXT_SYM(fh2m_inno_pci_clear_reaource_mtrrs);
#endif

