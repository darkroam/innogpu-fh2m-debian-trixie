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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#if !defined(CONFIG_LOONGARCH)
#include <asm/insn.h>
#endif
#include "inno_insn.h"

#if defined(CONFIG_ARM64)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
static const int aarch64_insn_encoding_class2[] = {
	AARCH64_INSN_CLS_UNKNOWN,
	AARCH64_INSN_CLS_UNKNOWN,
	AARCH64_INSN_CLS_UNKNOWN,
	AARCH64_INSN_CLS_UNKNOWN,
	AARCH64_INSN_CLS_LDST,
	AARCH64_INSN_CLS_DP_REG,
	AARCH64_INSN_CLS_LDST,
	AARCH64_INSN_CLS_DP_FPSIMD,
	AARCH64_INSN_CLS_DP_IMM,
	AARCH64_INSN_CLS_DP_IMM,
	AARCH64_INSN_CLS_BR_SYS,
	AARCH64_INSN_CLS_BR_SYS,
	AARCH64_INSN_CLS_LDST,
	AARCH64_INSN_CLS_DP_REG,
	AARCH64_INSN_CLS_LDST,
	AARCH64_INSN_CLS_DP_FPSIMD,
};
#endif

int fh2m_aarch64_get_insn_class2(u32 insn)
{
	int res = -1;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
	switch (aarch64_insn_encoding_class2[(insn >> 25) & 0xf]) {
		case AARCH64_INSN_CLS_BR_SYS:
			res = INNO_AARCH64_INSN_CLS_BR_SYS;
			break;
		case AARCH64_INSN_CLS_LDST:
			res = INNO_AARCH64_INSN_CLS_LDST;
			break;
		default:
				res = -1;
	}
#endif
	return res;
}
INNO_EXT_SYM(fh2m_aarch64_get_insn_class2);

int fh2m_inno_read_cpuid(void)
{
	return read_cpuid(DCZID_EL0);
}
INNO_EXT_SYM(fh2m_inno_read_cpuid);

int fh2m_inno_get_aarch64_insn_size(void)
{
	return AARCH64_INSN_SIZE;
}
INNO_EXT_SYM(fh2m_inno_get_aarch64_insn_size);
#endif

long fh2m_inno_probe_kernel_read(void *dst, const void *src, size_t size)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
	return probe_kernel_read(dst, src, size);
#else
	return copy_from_kernel_nofault(dst, src, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_probe_kernel_read);

long long fh2m_inno_sign_extend64(__u64 value, int index)
{
	return sign_extend64(value, index);
}
INNO_EXT_SYM(fh2m_inno_sign_extend64);

int fh2m_inno_sign_extend32(__u32 value, int index)
{
	return sign_extend32(value, index);
}
INNO_EXT_SYM(fh2m_inno_sign_extend32);
