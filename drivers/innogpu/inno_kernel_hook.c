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
#include <linux/pci.h>
#include <linux/kprobes.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <asm/ptrace.h>
#include "inno_misc.h"
#include "inno_mm.h"
#include "hal.h"
#include "inno_debug.h"
#include "inno_kernel_hook.h"

#define KBUILD_HAL "kprobe"
#define pr_fmt_irq(fmt) "[%s][%s:%d]" fmt,KBUILD_HAL,__func__,__LINE__
#if defined(INNO_GPU_LOG)
#define hal_dbg(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_DEBUG,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_info(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_INFO,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#else
#define hal_dbg(dev,fmt, ...)
#define hal_info(dev,fmt, ...)
#endif
#define hal_warn(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_WARNING,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_error(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_ERR,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)
#define hal_notice(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_NOTICE,dev,pr_fmt_irq(fmt), ##__VA_ARGS__)

int fh2m_inno_user_mode(inno_pt_regs *regs)
{
	return user_mode((struct pt_regs *)regs);
}
INNO_EXT_SYM(fh2m_inno_user_mode);

void fh2m_inno_set_pt_regs_sp(inno_pt_regs *regs, u64 sp)
{
#ifdef CONFIG_ARM64
	struct pt_regs *pt_regs = (struct pt_regs *)regs;
	pt_regs->sp = sp;
#endif
}
INNO_EXT_SYM(fh2m_inno_set_pt_regs_sp);

void fh2m_inno_instruction_pointer_set(inno_pt_regs *regs, unsigned long val)
{
	instruction_pointer_set((struct pt_regs *)regs, val);
}
INNO_EXT_SYM(fh2m_inno_instruction_pointer_set);

unsigned long fh2m_inno_instruction_pointer(inno_pt_regs *regs)
{
	return instruction_pointer((struct pt_regs *)regs);
}
INNO_EXT_SYM(fh2m_inno_instruction_pointer);

u64 fh2m_inno_regs_get_register(inno_pt_regs *regs, unsigned int offset)
{
	return regs_get_register((struct pt_regs *)regs, offset);
}
INNO_EXT_SYM(fh2m_inno_regs_get_register);

void fh2m_inno_pt_regs_write_reg(inno_pt_regs *regs, int r, unsigned long val)
{
#ifdef CONFIG_ARM64
	pt_regs_write_reg((struct pt_regs *)regs, r, val);
#endif
}
INNO_EXT_SYM(fh2m_inno_pt_regs_write_reg);

unsigned long fh2m_inno_pt_regs_read_reg(inno_pt_regs *regs, int r)
{
#ifdef CONFIG_ARM64
	return pt_regs_read_reg((struct pt_regs *)regs, r);
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_pt_regs_read_reg);

union kprobe_ftrace_fun {
	int (*fixup_alignment)(unsigned long addr, inno_pt_regs *regs);
	int (*fixup_pci_user_read_config_byte)(void *dev, int pos, u8 *val);
	int (*fixup_pci_user_read_config_dword)(void *dev, int pos, u32 *val);
};

struct kprobe_ftrace_hook {
#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
	struct kprobe *kp;
#elif defined(CONFIG_FTRACE)
	struct ftrace_ops *ops;
#endif
	char *symbol_name;
	unsigned int registered;
	unsigned int enable;
	void *pre_handler;
	union kprobe_ftrace_fun func_obj;
};

#if defined(CONFIG_KPROBES) || defined(CONFIG_FTRACE)
#define KP_ITEM(symbol_name)      {NULL, #symbol_name, 0, 1, _inno_##symbol_name, {.fixup_alignment = NULL}}
#else
#define KP_ITEM(symbol_name)      {#symbol_name, 0, 1, _inno_##symbol_name, {.fixup_alignment = NULL}}
#endif

#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
static int _inno_pci_user_read_config_byte(struct kprobe *p, struct pt_regs *regs);
static int _inno_pci_user_read_config_dword(struct kprobe *p, struct pt_regs *regs);
static int _inno_do_alignment_fault(struct kprobe *p, struct pt_regs *regs);
#elif defined(CONFIG_FTRACE)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void notrace _inno_pci_user_read_config_byte(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs);
static void notrace _inno_pci_user_read_config_dword(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs);
static void notrace _inno_do_alignment_fault(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs);
#else
static void notrace _inno_pci_user_read_config_byte(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs);
static void notrace _inno_pci_user_read_config_dword(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs);
static void notrace _inno_do_alignment_fault(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs);
#endif
#else
int _inno_pci_user_read_config_byte(struct kprobe *p, struct pt_regs *regs);
int _inno_pci_user_read_config_dword(struct kprobe *p, struct pt_regs *regs);
int _inno_do_alignment_fault(struct kprobe *p, struct pt_regs *regs);
#endif

static struct kprobe_ftrace_hook kp_hook[] = {
	KP_ITEM(do_alignment_fault),
	KP_ITEM(pci_user_read_config_byte),
	KP_ITEM(pci_user_read_config_dword),
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,86)
static __maybe_unused int _inno_fixup_alignment_fun(unsigned long addr, unsigned int esr, struct pt_regs *regs)
#else
static __maybe_unused int _inno_fixup_alignment_fun(unsigned long addr, unsigned long esr, struct pt_regs *regs)
#endif
{
	return kp_hook[DO_ALIGNMENT_FAULT].func_obj.fixup_alignment(addr, regs);
}

#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
static int _inno_pci_user_read_config_byte(struct kprobe *p, struct pt_regs *regs) {
	instruction_pointer_set(regs, (unsigned long)(kp_hook[PCI_USER_READ_CONFIG_BYTE].func_obj.fixup_pci_user_read_config_byte));
	return 1;
}

static int _inno_pci_user_read_config_dword(struct kprobe *p, struct pt_regs *regs) {
	instruction_pointer_set(regs, (unsigned long)(kp_hook[PCI_USER_READ_CONFIG_DWORD].func_obj.fixup_pci_user_read_config_dword));
	return 1;
}

static int _inno_do_alignment_fault(struct kprobe *p, struct pt_regs *regs) {
	instruction_pointer_set(regs, (unsigned long)_inno_fixup_alignment_fun);
	return 1;
}
#elif defined(CONFIG_FTRACE)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void notrace _inno_pci_user_read_config_byte(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs) {
#else
static void notrace _inno_pci_user_read_config_byte(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs) {
	struct pt_regs *regs = ftrace_get_regs(fregs);
#endif
	instruction_pointer_set(regs, (unsigned long)(kp_hook[PCI_USER_READ_CONFIG_BYTE].func_obj.fixup_pci_user_read_config_byte));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void notrace _inno_pci_user_read_config_dword(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs) {
#else
static void notrace _inno_pci_user_read_config_dword(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs) {
	struct pt_regs *regs = ftrace_get_regs(fregs);
#endif
	instruction_pointer_set(regs, (unsigned long)(kp_hook[PCI_USER_READ_CONFIG_DWORD].func_obj.fixup_pci_user_read_config_dword));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void notrace _inno_do_alignment_fault(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct pt_regs *regs) {
#else
static void notrace _inno_do_alignment_fault(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *ops, struct ftrace_regs *fregs) {
	struct pt_regs *regs = ftrace_get_regs(fregs);
#endif
	instruction_pointer_set(regs, (unsigned long)_inno_fixup_alignment_fun);
}
#else
int _inno_pci_user_read_config_byte(struct kprobe *p, struct pt_regs *regs) {
	return 1;
}

int _inno_pci_user_read_config_dword(struct kprobe *p, struct pt_regs *regs) {
	return 1;
}

int _inno_do_alignment_fault(struct kprobe *p, struct pt_regs *regs) {
	return 1;
}
#endif

void fh2m_fixup_set_handler(int hook_index, void *fun)
{
	switch (hook_index) {
	case DO_ALIGNMENT_FAULT:
		kp_hook[DO_ALIGNMENT_FAULT].func_obj.fixup_alignment = fun;
		break;
	case PCI_USER_READ_CONFIG_BYTE:
		kp_hook[PCI_USER_READ_CONFIG_BYTE].func_obj.fixup_pci_user_read_config_byte = fun;
		break;
	case PCI_USER_READ_CONFIG_DWORD:
		kp_hook[PCI_USER_READ_CONFIG_DWORD].func_obj.fixup_pci_user_read_config_dword = fun;
		break;
	default:
		inno_error("[%s] hook id %d is not found\n", __func__, hook_index);
	}
}
INNO_EXT_SYM(fh2m_fixup_set_handler);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
#define FTRACE_OPS_FL_RECURSION FTRACE_OPS_FL_RECURSION_SAFE
#endif

int fh2m_inno_register_kprobe_or_ftrace(void)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i< INNO_ARRAY_SIZE(kp_hook); i++) {
		if (0 == kp_hook[i].registered && kp_hook[i].func_obj.fixup_alignment) {
#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
			kp_hook[i].kp = fh2m_inno_kzalloc_kernel(sizeof(struct kprobe));
			if (!kp_hook[i].kp) {
				inno_error("fixup [%s] kp alloc fail!\n", kp_hook[i].symbol_name);
				return -ENOMEM;
			}
			kp_hook[i].kp->symbol_name = kp_hook[i].symbol_name;
			kp_hook[i].kp->pre_handler = kp_hook[i].pre_handler;
			ret = register_kprobe(kp_hook[i].kp);
			if (ret < 0) {
				inno_error("register_kprobe failed, return %d,fun:%s\n", ret, kp_hook[i].symbol_name);
				fh2m_inno_kfree(kp_hook[i].kp);
				kp_hook[i].kp = NULL;
				goto fail;
			}
#elif defined(CONFIG_FTRACE)
			kp_hook[i].ops = fh2m_inno_kzalloc_kernel(sizeof(struct ftrace_ops));
			if (!kp_hook[i].ops) {
				inno_error("fixup [%s] ops alloc fail!\n", kp_hook[i].symbol_name);
				return -ENOMEM;
			}
			kp_hook[i].ops->func = kp_hook[i].pre_handler;
			kp_hook[i].ops->flags = FTRACE_OPS_FL_SAVE_REGS| FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY;
			ret = ftrace_set_filter(kp_hook[i].ops, kp_hook[i].symbol_name, fh2m_inno_strlen(kp_hook[i].symbol_name), 0);
			ret = register_ftrace_function(kp_hook[i].ops);
			if (ret < 0) {
				inno_error("register_ftrace_function failed, return %d,fun:%s\n", ret, kp_hook[i].symbol_name);
				fh2m_inno_kfree(kp_hook[i].ops);
				kp_hook[i].ops = NULL;
				goto fail;
			}
#else
			goto fail;
#endif
			kp_hook[i].registered = 1;
		}
	}
	return 0;

fail:
	for (i = 0; i< INNO_ARRAY_SIZE(kp_hook); i++) {
		if (kp_hook[i].registered)
		{
#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
			unregister_kprobe(kp_hook[i].kp);
			if (kp_hook[i].kp) {
				fh2m_inno_kfree(kp_hook[i].kp);
				kp_hook[i].kp = NULL;
			}
#elif defined(CONFIG_FTRACE)
			unregister_ftrace_function(kp_hook[i].ops);
			if (kp_hook[i].ops) {
				fh2m_inno_kfree(kp_hook[i].ops);
				kp_hook[i].ops = NULL;
			}
#endif
			kp_hook[i].registered = 0;
		}
	}

	return ret;
}
INNO_EXT_SYM(fh2m_inno_register_kprobe_or_ftrace);

void fh2m_inno_unregister_kprobe_or_ftrace(void)
{
	int i;

	for (i = 0; i< INNO_ARRAY_SIZE(kp_hook); i++) {
		if (kp_hook[i].registered)
		{
#if defined(CONFIG_KPROBES) && (LINUX_VERSION_CODE > KERNEL_VERSION(4,14,48))
			unregister_kprobe(kp_hook[i].kp);
			if (kp_hook[i].kp) {
				fh2m_inno_kfree(kp_hook[i].kp);
				kp_hook[i].kp = NULL;
			}
#elif defined(CONFIG_FTRACE)
			unregister_ftrace_function(kp_hook[i].ops);
			if (kp_hook[i].ops) {
				fh2m_inno_kfree(kp_hook[i].ops);
				kp_hook[i].ops = NULL;
			}
#endif
			kp_hook[i].registered = 0;
		}
	}
}
INNO_EXT_SYM(fh2m_inno_unregister_kprobe_or_ftrace);
