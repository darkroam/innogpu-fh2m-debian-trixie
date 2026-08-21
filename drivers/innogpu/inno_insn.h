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
#ifndef __INNO_INSN_H__
#define __INNO_INSN_H__
#include "hal.h"

#define INNO_AARCH64_INSN_CLS_BR_SYS 1
#define INNO_AARCH64_INSN_CLS_LDST   2

#if defined(CONFIG_ARM64)

enum inno_aarch64_insn_register_type {
        INNO_AARCH64_INSN_REGTYPE_RT,
        INNO_AARCH64_INSN_REGTYPE_RN,
        INNO_AARCH64_INSN_REGTYPE_RT2,
        INNO_AARCH64_INSN_REGTYPE_RM,
        INNO_AARCH64_INSN_REGTYPE_RD,
        INNO_AARCH64_INSN_REGTYPE_RA,
        INNO_AARCH64_INSN_REGTYPE_RS,
};

enum inno_aarch64_insn_imm_type {
	INNO_AARCH64_INSN_IMM_ADR,
	INNO_AARCH64_INSN_IMM_26,
	INNO_AARCH64_INSN_IMM_19,
	INNO_AARCH64_INSN_IMM_16,
	INNO_AARCH64_INSN_IMM_14,
	INNO_AARCH64_INSN_IMM_12,
	INNO_AARCH64_INSN_IMM_9,
	INNO_AARCH64_INSN_IMM_7,
	INNO_AARCH64_INSN_IMM_6,
	INNO_AARCH64_INSN_IMM_S,
	INNO_AARCH64_INSN_IMM_R,
	INNO_AARCH64_INSN_IMM_N,
	INNO_AARCH64_INSN_IMM_MAX
};

int fh2m_aarch64_get_insn_class2(u32 insn);

int fh2m_inno_read_cpuid(void);

int fh2m_inno_get_aarch64_insn_size(void);
#endif

long fh2m_inno_probe_kernel_read(void *dst, const void *src, size_t size);

long long fh2m_inno_sign_extend64(__u64 value, int index);

int fh2m_inno_sign_extend32(__u32 value, int index);

#endif
