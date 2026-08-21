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
#ifndef __INNO_KERNEL_HOOK_H__
#define __INNO_KERNEL_HOOK_H__

typedef void inno_pt_regs;
typedef void inno_kprobe;

enum kprobe_ftrace_handler_id {
	DO_ALIGNMENT_FAULT = 0,
	PCI_USER_READ_CONFIG_BYTE,
	PCI_USER_READ_CONFIG_DWORD,
	KPROBE_FTRACE_HANDLER_ID_MAX,
};

int fh2m_inno_user_mode(inno_pt_regs *regs);

void fh2m_inno_set_pt_regs_sp(inno_pt_regs *regs, u64 sp);

void fh2m_inno_instruction_pointer_set(inno_pt_regs *regs, unsigned long val);

unsigned long fh2m_inno_instruction_pointer(inno_pt_regs *regs);

u64 fh2m_inno_regs_get_register(inno_pt_regs *regs, unsigned int offset);

void fh2m_inno_pt_regs_write_reg(inno_pt_regs *regs, int r, unsigned long val);

unsigned long fh2m_inno_pt_regs_read_reg(inno_pt_regs *regs, int val);

void fh2m_fixup_set_handler(int hook_index, void *fun);

int fh2m_inno_register_kprobe_or_ftrace(void);

void fh2m_inno_unregister_kprobe_or_ftrace(void);
#endif
