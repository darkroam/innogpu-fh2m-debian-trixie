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
#ifndef __INNO_PCI_H__
#define __INNO_PCI_H__

#include <linux/types.h>
enum inno_pmsg_state{
	INNO_PMSG_ON,
	INNO_PMSG_SUSPEND,
	INNO_PMSG_NONE,
};

extern int INNO_IRQ_MODE_LEGACY;
extern int INNO_IRQ_MODE_MSI;

typedef void inno_pci_dev;

int fh2m_inno_pci_enable_device(inno_pci_dev *dev);

void fh2m_inno_pci_set_master(inno_pci_dev *dev);

int fh2m_inno_pci_enable_msi(inno_pci_dev *dev);

inno_pci_dev * fh2m_inno_pci_get_device(unsigned int vendor, unsigned int device);

int fh2m_inno_check_resize_version(void);

int fh2m_inno_check_rc_size(inno_pci_dev *dev);

int fh2m_inno_gpu_device_resize_fb_bar(inno_pci_dev *dev, int resno, int rbar_size, int *repo);

int fh2m_inno_pci_fixup_rebar_state(inno_pci_dev *pci_dev);

unsigned long long fh2m_inno_pci_resource_len(inno_pci_dev *dev, int bar);

unsigned long long fh2m_inno_pci_resource_start(inno_pci_dev *dev, int bar);

unsigned long long fh2m_inno_pci_resource_end(inno_pci_dev *dev, int bar);

unsigned long fh2m_inno_pci_resource_flags(inno_pci_dev *dev, int bar);

int fh2m_inno_pci_request_region(inno_pci_dev *dev, int bar, const char *res_name);

void fh2m_inno_pci_release_region(inno_pci_dev *dev, int bar);

void fh2m_inno_pci_disable_msi(inno_pci_dev *dev);

void fh2m_inno_pci_clear_master(inno_pci_dev *dev);

void fh2m_inno_pci_restore_state(inno_pci_dev *dev);

void fh2m_inno_pci_disable_device(inno_pci_dev *dev);

int fh2m_inno_pci_save_state(inno_pci_dev *dev);

int fh2m_inno_pci_set_power_state(inno_pci_dev *dev, enum inno_pmsg_state state);

bool fh2m_inno_pci_is_busmaster(inno_pci_dev *dev);

unsigned int fh2m_inno_get_pci_irq(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_vendor(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_subvendor(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_device(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_subdevice(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_baseclass(inno_pci_dev *dev);

unsigned short fh2m_inno_get_pci_subclass(inno_pci_dev *dev);

int fh2m_inno_pci_irq_vector(inno_pci_dev *dev, unsigned int nr);

void fh2m_inno_arch_phys_wc_del(int handle);

void fh2m_inno_arch_io_free_memtype_wc(unsigned long long start, unsigned long long size);

void * fh2m_inno_request_region(unsigned long long start, unsigned long long n, const char * name);

void * fh2m_inno_request_mem_region(unsigned long long start, unsigned long long n, const char *name);

void fh2m_inno_release_region(unsigned long long start, unsigned long long n);

void fh2m_inno_release_mem_region(unsigned long long start, unsigned long long n);

inno_pci_dev *fh2m_inno_to_pci_dev(void *dev);

uint8_t fh2m_inno_pci_revision_id(inno_pci_dev *dev);

bool fh2m_inno_loongarch_kylin2203_55_intx_irq(inno_pci_dev *dev);

bool fh2m_inno_loongson_pcie_bridge_7a1000(inno_pci_dev *dev);

int fh2m_inno_pci_alloc_irq_vectors(inno_pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, int mode);

void fh2m_inno_pci_free_irq_vectors(inno_pci_dev *dev);

bool fh2m_inno_devices_under_same_switch(inno_pci_dev *dev1, inno_pci_dev *dev2);

bool fh2m_inno_devices_under_same_RC(inno_pci_dev *pci_dev1, inno_pci_dev *pci_dev2);

int fh2m_inno_pci_user_read_config_byte(inno_pci_dev *dev, int pos, u8 *val);

int fh2m_inno_pci_user_read_config_dword(inno_pci_dev *dev, int pos, u32 *val);

void fh2m_inno_pci_read_config_dword(inno_pci_dev *dev, int offset, u32 *rval);

void fh2m_inno_pci_write_config_dword(inno_pci_dev *dev, int offset, u32 wval);

void inno_pci_cfgspace_dump(inno_pci_dev *pdev, int reg_dump_num);

void innogpu_pci_dump_cfgspace_regs(inno_pci_dev *pci_dev, unsigned int mask);
#endif
