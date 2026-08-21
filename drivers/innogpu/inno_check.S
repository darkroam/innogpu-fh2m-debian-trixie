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
#if defined(CONFIG_X86)
#if !defined(CONFIG_RETPOLINE) && !defined(CONFIG_MITIGATION_RETPOLINE)
#include <linux/version.h>
#include <linux/stringify.h>
#include <linux/linkage.h>
#include <asm/dwarf2.h>
#include <asm/cpufeatures.h>
#include <asm/export.h>
.macro JMP_NOSPEC reg:req
	jmp *\reg
.endm

.macro THUNK reg
	 .section .text.__x86.indirect_thunk

ENTRY(__x86_indirect_thunk_\reg)
	CFI_STARTPROC
	JMP_NOSPEC %\reg
	CFI_ENDPROC
ENDPROC(__x86_indirect_thunk_\reg)
.endm

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0))
#include <asm/alternative-asm.h>

#define __EXPORT_THUNK(sym) _ASM_NOKPROBE(sym); EXPORT_SYMBOL(sym)
#define EXPORT_THUNK(reg) __EXPORT_THUNK(__x86_indirect_thunk_ ## reg)
#define GENERATE_THUNK(reg) THUNK reg ; EXPORT_THUNK(reg)

GENERATE_THUNK(_ASM_AX)
GENERATE_THUNK(_ASM_BX)
GENERATE_THUNK(_ASM_CX)
GENERATE_THUNK(_ASM_DX)
GENERATE_THUNK(_ASM_SI)
GENERATE_THUNK(_ASM_DI)
GENERATE_THUNK(_ASM_BP)
#ifdef CONFIG_64BIT
GENERATE_THUNK(r8)
GENERATE_THUNK(r9)
GENERATE_THUNK(r10)
GENERATE_THUNK(r11)
GENERATE_THUNK(r12)
GENERATE_THUNK(r13)
GENERATE_THUNK(r14)
GENERATE_THUNK(r15)
#endif

#else
#include <asm/alternative.h>

#define __EXPORT_THUNK(sym) _ASM_NOKPROBE(sym); EXPORT_SYMBOL(sym)
#define EXPORT_THUNK(reg)  __EXPORT_THUNK(__x86_indirect_thunk_ ## reg)

#undef GEN
#define GEN(reg) THUNK reg
#include <asm/GEN-for-each-reg.h>

#undef GEN
#define GEN(reg) EXPORT_THUNK(reg)
#include <asm/GEN-for-each-reg.h>

#ifdef CONFIG_64BIT
GEN(rax)
GEN(rbx)
GEN(rcx)
GEN(rdx)
GEN(rsi)
GEN(rdi)
GEN(rbp)
GEN(r8)
GEN(r9)
GEN(r10)
GEN(r11)
GEN(r12)
GEN(r13)
GEN(r14)
GEN(r15)
#else
GEN(eax)
GEN(ebx)
GEN(ecx)
GEN(edx)
GEN(esi)
GEN(edi)
GEN(ebp)
#endif

#endif
#endif
#endif

