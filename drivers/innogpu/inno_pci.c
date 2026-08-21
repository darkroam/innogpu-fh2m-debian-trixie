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
#include <generated/utsrelease.h>
#include <linux/pci.h>
#include <uapi/linux/pci.h>
#include <uapi/linux/pci_regs.h>
#include <linux/version.h>
#include "inno_misc.h"
#include "inno_pci.h"
#include "inno_debug.h"
#include "inno_timer.h"
#define KBUILD_INNO_PCI "inno_pci"
#define pr_fmt_inno_pci(fmt) "[%s][%s:%d]" fmt,KBUILD_INNO_PCI,__func__,__LINE__

#define inno_pci_notice(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_NOTICE,dev,pr_fmt_inno_pci(fmt), ##__VA_ARGS__)
#define inno_pci_error(dev,fmt, ...)\
		fh2m_inno_dev_printk(KERN_ERR,dev,pr_fmt_inno_pci(fmt), ##__VA_ARGS__)

/*
 * Note:
 * Some kernel versions lack the following definitions of resize bar regs
 * */

#ifndef PCI_REBAR_CAP
#define PCI_REBAR_CAP	4	/* capability register */
#endif

#ifndef PCI_REBAR_CTRL_BAR_IDX
#define PCI_REBAR_CTRL_BAR_IDX	0x00000007  /* BAR index */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SIZE
#define PCI_REBAR_CTRL_BAR_SIZE	0x00001F00  /* BAR size */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SHIFT
#define PCI_REBAR_CTRL_BAR_SHIFT	8	    /* shift for BAR size */
#endif

#define DELAY_2_SECONDS	2000000

int INNO_IRQ_MODE_LEGACY = PCI_IRQ_LEGACY;
int INNO_IRQ_MODE_MSI = PCI_IRQ_MSI;

int fh2m_inno_pci_enable_device(inno_pci_dev *dev)
{
	return pci_enable_device((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_enable_device);

void fh2m_inno_pci_set_master(inno_pci_dev *dev)
{
	pci_set_master((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_set_master);

int fh2m_inno_pci_enable_msi(inno_pci_dev *dev)
{
	return pci_enable_msi((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_enable_msi);

inno_pci_dev * fh2m_inno_pci_get_device(unsigned int vendor, unsigned int device)
{
	struct pci_dev *dev;
	dev = pci_get_device(vendor, device, NULL);
	return dev;
}
INNO_EXT_SYM(fh2m_inno_pci_get_device);

unsigned long long fh2m_inno_pci_resource_len(inno_pci_dev *dev, int bar)
{

#if	defined(__G3_NE__) && defined(NE_VARIANT)
	unsigned long long  bar_len_fix = 0;
	if(bar == 2)
	{
#if (NE_VARIANT == 1)
		/* xilinx NE platform only has 2G DDR size */
		bar_len_fix = 2ULL * 1024 * 1024 * 1024; /* 2G */
#elif (NE_VARIANT == 0)
		/* 19p NE platform only has 4G DDR size */
		bar_len_fix = 4ULL * 1024 * 1024 * 1024; /* 4G */
#endif
		return bar_len_fix;
	}
#endif /* #if defined(__G3_NE__) && defined(NE_VARIANT) */

#if	defined(__G3_PAL__)
	unsigned long long  bar_len_fix = 0;
	if(bar == 2)
	{
		/* paladium for g3 just supported 28G ddr */
		bar_len_fix = 28ULL * 1024 * 1024 * 1024;
		return bar_len_fix;
	}
#endif

	return pci_resource_len((struct pci_dev *)dev, bar);
}
INNO_EXT_SYM(fh2m_inno_pci_resource_len);

unsigned long long fh2m_inno_pci_resource_start(inno_pci_dev *dev, int bar)
{
	return pci_resource_start((struct pci_dev *)dev, bar);
}
INNO_EXT_SYM(fh2m_inno_pci_resource_start);

unsigned long long fh2m_inno_pci_resource_end(inno_pci_dev *dev, int bar)
{
	return pci_resource_end((struct pci_dev *)dev, bar);
}
INNO_EXT_SYM(fh2m_inno_pci_resource_end);

unsigned long fh2m_inno_pci_resource_flags(inno_pci_dev *dev, int bar)
{
	return pci_resource_flags((struct pci_dev *)dev, bar);
}
INNO_EXT_SYM(fh2m_inno_pci_resource_flags);

int fh2m_inno_pci_request_region(inno_pci_dev *dev, int bar, const char *res_name)
{
	return pci_request_region((struct pci_dev *)dev, bar, res_name);
}
INNO_EXT_SYM(fh2m_inno_pci_request_region);

void fh2m_inno_pci_release_region(inno_pci_dev *dev, int bar)
{
	pci_release_region((struct pci_dev *)dev, bar);
}
INNO_EXT_SYM(fh2m_inno_pci_release_region);

void fh2m_inno_pci_disable_msi(inno_pci_dev *dev)
{
	pci_disable_msi((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_disable_msi);

void fh2m_inno_pci_clear_master(inno_pci_dev *dev)
{
	pci_clear_master((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_clear_master);

void fh2m_inno_pci_restore_state(inno_pci_dev *dev)
{
	pci_restore_state((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_restore_state);

void fh2m_inno_pci_disable_device(inno_pci_dev *dev)
{
	pci_disable_device((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_disable_device);

int fh2m_inno_pci_save_state(inno_pci_dev *dev)
{
	return pci_save_state((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_save_state);

int fh2m_inno_pci_set_power_state(inno_pci_dev *dev, enum inno_pmsg_state state)
{
	switch (state) {
	case INNO_PMSG_ON:
		return pci_set_power_state((struct pci_dev *)dev, pci_choose_state((struct pci_dev *)dev, PMSG_ON));
	case INNO_PMSG_SUSPEND:
		return pci_set_power_state((struct pci_dev *)dev, pci_choose_state((struct pci_dev *)dev, PMSG_SUSPEND));
	default:
		return INNO_PMSG_NONE;
	}
}
INNO_EXT_SYM(fh2m_inno_pci_set_power_state);

bool fh2m_inno_pci_is_busmaster(inno_pci_dev *dev)
{
	return ((struct pci_dev*)dev)->is_busmaster;
}
INNO_EXT_SYM(fh2m_inno_pci_is_busmaster);

unsigned int fh2m_inno_get_pci_irq(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->irq;
}
INNO_EXT_SYM(fh2m_inno_get_pci_irq);

unsigned short fh2m_inno_get_pci_vendor(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->vendor;
}
INNO_EXT_SYM(fh2m_inno_get_pci_vendor);

unsigned short fh2m_inno_get_pci_device(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->device;
}
INNO_EXT_SYM(fh2m_inno_get_pci_device);

unsigned short fh2m_inno_get_pci_subvendor(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->subsystem_vendor;
}
INNO_EXT_SYM(fh2m_inno_get_pci_subvendor);

unsigned short fh2m_inno_get_pci_subdevice(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->subsystem_device;
}
INNO_EXT_SYM(fh2m_inno_get_pci_subdevice);

unsigned short fh2m_inno_get_pci_baseclass(inno_pci_dev *dev)
{
	return (((struct pci_dev *)dev)->class >> 16) & 0xFF;
}
INNO_EXT_SYM(fh2m_inno_get_pci_baseclass);

unsigned short fh2m_inno_get_pci_subclass(inno_pci_dev *dev)
{
	return (((struct pci_dev *)dev)->class >> 8) & 0xFF;
}
INNO_EXT_SYM(fh2m_inno_get_pci_subclass);

void fh2m_inno_arch_phys_wc_del(int handle)
{
	arch_phys_wc_del(handle);
}
INNO_EXT_SYM(fh2m_inno_arch_phys_wc_del);

void fh2m_inno_arch_io_free_memtype_wc(unsigned long long start, unsigned long long size)
{
	arch_io_free_memtype_wc(start, size);
}
INNO_EXT_SYM(fh2m_inno_arch_io_free_memtype_wc);

void * fh2m_inno_request_region(unsigned long long start, unsigned long long n, const char * name)
{
	return request_region(start, n, name);
}
INNO_EXT_SYM(fh2m_inno_request_region);

void * fh2m_inno_request_mem_region(unsigned long long start, unsigned long long n, const char *name)
{
	return request_mem_region(start, n, name);
}
INNO_EXT_SYM(fh2m_inno_request_mem_region);

void fh2m_inno_release_region(unsigned long long start, unsigned long long n)
{
	release_region(start, n);
}
INNO_EXT_SYM(fh2m_inno_release_region);

void fh2m_inno_release_mem_region(unsigned long long start, unsigned long long n)
{
	release_mem_region(start, n);
}
INNO_EXT_SYM(fh2m_inno_release_mem_region);

int fh2m_inno_pci_irq_vector(inno_pci_dev *dev, unsigned int nr)
{
	return pci_irq_vector((struct pci_dev *)dev, nr);
}
INNO_EXT_SYM(fh2m_inno_pci_irq_vector);

inno_pci_dev *fh2m_inno_to_pci_dev(void *dev)
{
	return (inno_pci_dev *)to_pci_dev((struct device *)dev);
}
INNO_EXT_SYM(fh2m_inno_to_pci_dev);

uint8_t fh2m_inno_pci_revision_id(inno_pci_dev *dev)
{
	return ((struct pci_dev *)dev)->revision;
}
INNO_EXT_SYM(fh2m_inno_pci_revision_id);

int fh2m_inno_pci_alloc_irq_vectors(inno_pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, int mode)
{
	return pci_alloc_irq_vectors((struct pci_dev *)dev, min_vecs, max_vecs, mode);
}
INNO_EXT_SYM(fh2m_inno_pci_alloc_irq_vectors);

void fh2m_inno_pci_free_irq_vectors(inno_pci_dev *dev)
{
	return pci_free_irq_vectors((struct pci_dev *)dev);
}
INNO_EXT_SYM(fh2m_inno_pci_free_irq_vectors);

/*
 * When detect KylinOS2203-55 kernel and loongson 3a5000/7a2000 pcie bridge, return true (meaning pcie intx irq).
 * */
bool fh2m_inno_loongarch_kylin2203_55_intx_irq(inno_pci_dev *dev)
{
#if defined(CONFIG_LOONGARCH) && defined(CONFIG_KYLINOS_DESKTOP)
#if (LINUX_VERSION_CODE == KERNEL_VERSION(5, 4, 18))
	/* Note: UTS_KYLINOS_RELEASE_ABI maybe: 55, 87.76, ky10, 77.66.sm2 etc for KylinOS release/debug version */
	if (strcmp(STRINGIFY(UTS_KYLINOS_RELEASE_ABI), "55") == 0) {
		struct pci_dev *pdev = (struct pci_dev *)dev;
		struct pci_bus *root = pdev->bus;

		while (root && root->self) {
			/*
			 * when root is pcie bridge, root->parent and root->self is non-null
			 * when root is pcie rc, root->parent and root->self is null
			 * 7a2000 pcie bridge vendorid: 0x14 deviceid: 0x7a59
			 * 7a1000 pcie bridge vendorid: 0x14 deviceid: 0x7a29
			 */
			if (root->self->vendor == 0x14 && root->self->device == 0x7a59) {
				return true;
			}

			root = root->parent;
		}
	}
#endif
#endif

	return false;
}
INNO_EXT_SYM(fh2m_inno_loongarch_kylin2203_55_intx_irq);

/*
 * When detect loongson 7a1000 pcie bridge, return true
 * */
bool fh2m_inno_loongson_pcie_bridge_7a1000(inno_pci_dev *dev)
{
#if defined(CONFIG_LOONGARCH)
	struct pci_dev *pdev = (struct pci_dev *)dev;
	struct pci_bus *root = pdev->bus;

	while (root && root->self) {
		/*
		 * when root is pcie bridge, root->parent and root->self is non-null
		 * when root is pcie rc, root->parent and root->self is null
		 * 7a2000 pcie bridge vendorid: 0x14 deviceid: 0x7a59
		 * 7a1000 pcie bridge vendorid: 0x14 deviceid: 0x7a29
		 */
		if (root->self->vendor == 0x14 && root->self->device == 0x7a29) {
			return true;
		}

		root = root->parent;
	}
#endif

	return false;
}
INNO_EXT_SYM(fh2m_inno_loongson_pcie_bridge_7a1000);

extern struct resource *pci_bus_resource_n(const struct pci_bus *bus, int n);
#define inno_pci_bus_for_each_resource(bus, res, i)				\
	for (i = 0;							\
		(res = pci_bus_resource_n(bus, i)) || i < PCI_BRIDGE_RESOURCE_NUM; \
		i++)

static void inno_pci_release_resource(struct pci_dev *dev, int resno)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	pci_release_resource(dev, resno);
#else
	// null
#endif

}

static int inno_pci_resize_resource(struct pci_dev *dev, int resno, int size)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	return pci_resize_resource(dev, resno, size);
#else
	return 1;
#endif
}

int fh2m_inno_check_rc_size(inno_pci_dev *dev)
{
	struct pci_bus *root;
	struct resource *res;
	struct pci_dev *pdev;
	unsigned i;

	if (dev == NULL)
		return -1;

	pdev = (struct pci_dev *)dev;
	/* Check if the root BUS has 64bit memory resources */
	root = pdev->bus;
	while (root->parent)
		root = root->parent;

	inno_pci_bus_for_each_resource(root, res, i) {
		if (res && res->flags & (IORESOURCE_MEM | IORESOURCE_MEM_64) &&
			res->start > 0x100000000ull)
			break;
	}

	/* Trying to resize is pointless without a root hub window above 4GB */
	if (!res)
		return 1;

	return 0;
}

INNO_EXT_SYM(fh2m_inno_check_rc_size);

int fh2m_inno_check_resize_version(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	return 0;
#else
	return 1;
#endif
}
INNO_EXT_SYM(fh2m_inno_check_resize_version);

#define INNO_RESIZE_NUMS (0x1)
static int inno_resize_bar(struct pci_dev *pdev, int resno, int rbar_size)
{
	u16 cmd;
	int ret = 0;
	u32 i;

	/* Disable memory decoding while we change the BAR addresses and size */
	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	pci_write_config_word(pdev, PCI_COMMAND,
				cmd & ~PCI_COMMAND_MEMORY);

	inno_pci_release_resource(pdev, resno);
	inno_pci_release_resource(pdev, 0);
	//inno_pci_error(&pdev->dev, " -ENOTSUPP:%d, -EBUSY:%d, -EINVAL:%d \n", (-ENOTSUPP), (-EBUSY), (-EINVAL) );
	for (i = 0; i < INNO_RESIZE_NUMS; i++)
	{

		ret = inno_pci_resize_resource(pdev, resno, rbar_size);
		if(ret == -ENOSPC)
		{
			inno_pci_notice(&pdev->dev, "Not enough PCI address space for a large BAR.\n");
		}else if (ret && ret != -ENOTSUPP) {
				inno_pci_notice(&pdev->dev, "Problem resizing BAR2 (%d).retry:%d\n", ret, i);
		}else{
			inno_pci_notice(&pdev->dev, "pci_resize_resource cfg success \n");
			break;
		}

	}

	pci_assign_unassigned_bus_resources(pdev->bus);
	pci_write_config_word(pdev, PCI_COMMAND, cmd);

	return ret;
}

int fh2m_inno_gpu_device_resize_fb_bar(inno_pci_dev *dev, int resno, int rbar_size, int *repo)
{
	struct pci_dev *pdev;
	int ret;

	if ((dev == NULL) || (repo == NULL))
		return -1;

	pdev = (struct pci_dev *)dev;
	ret = inno_resize_bar(pdev, resno, rbar_size);
	*repo = ret;

	return 0;
}

INNO_EXT_SYM(fh2m_inno_gpu_device_resize_fb_bar);

int fh2m_inno_pci_fixup_rebar_state(inno_pci_dev *pci_dev)
{
	struct pci_dev *pdev = (struct pci_dev *)pci_dev;
	unsigned int pos = 0, nbars = 0, i = 0;
	int ret = 0;
	u32 ctrl0 = 0;
	u32 cap = 0, ctrlx = 0, ctrlx2 = 0;
	int sw_size = 0, hw_size = 0;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_REBAR);
	if (!pos)
		return -ENXIO;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl0);
	nbars = (ctrl0 & PCI_REBAR_CTRL_NBAR_MASK) >> PCI_REBAR_CTRL_NBAR_SHIFT;

	for (i = 0; i < nbars; i++, pos += 8) {
		struct resource *res = NULL;
		int bar_idx = 0;
		char *result = NULL;

		pci_read_config_dword(pdev, pos + PCI_REBAR_CAP, &cap);
		pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrlx);
		bar_idx = ctrlx & PCI_REBAR_CTRL_BAR_IDX;
		res = pdev->resource + bar_idx;
		sw_size = ilog2(resource_size(res)) - 20;
		hw_size = (ctrlx & PCI_REBAR_CTRL_BAR_SIZE) >> PCI_REBAR_CTRL_BAR_SHIFT;

		/*
		 * Note:
		 * when check sw bar_size is different with hw bar_size, rewrite sw bar_size to rebar_ctrl register.
		 * */
		if (sw_size != hw_size) {
			ctrlx &= ~PCI_REBAR_CTRL_BAR_SIZE;
			ctrlx |= sw_size << PCI_REBAR_CTRL_BAR_SHIFT;
			pci_write_config_dword(pdev, pos + PCI_REBAR_CTRL, ctrlx);
			pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrlx2);

			if (ctrlx != ctrlx2) {
				result = "fail";
				ret = -EBUSY;
			} else {
				result = "success";
			}

			inno_pci_notice(&pdev->dev,
							"fixup rebar size %s. pos: %#x, ctrl0: %#x, ctrlx: %#x, ctrlx2: %#x, cap: %#x, sw_size: %#x MB, hw_size: %#x MB\n",
							result, pos, ctrl0, ctrlx, ctrlx2, cap, 2 << sw_size, 2 << hw_size);
		}
	}

	return ret;
}
INNO_EXT_SYM(fh2m_inno_pci_fixup_rebar_state);

static void* inno_device_under_switch(inno_pci_dev *dev)
{
	struct pci_dev *pdev;
	int dev_num;
	struct list_head *device;
	struct pci_bus *bus;

	if (dev == NULL)
		return NULL;

	pdev = (struct pci_dev *)dev;
	dev_num = 0;
	bus = pdev->bus;

	while (!pci_is_root_bus(bus))
	{
		list_for_each(device, &bus->devices)
		{
			++dev_num;
    	}
		if (dev_num > 1)
		{
			return bus->self;
		}
		else
		{
			bus = bus->parent;
		}
	}
	return NULL;
}

bool fh2m_inno_devices_under_same_switch(inno_pci_dev *dev1, inno_pci_dev *dev2)
{
	inno_pci_dev *bridge1 = inno_device_under_switch(dev1);
	inno_pci_dev *bridge2 = inno_device_under_switch(dev2);
	if ((bridge1 != NULL) && (bridge2 != NULL) && (bridge1 == bridge2))
	{
		return true;
	}

	return false;
}
INNO_EXT_SYM(fh2m_inno_devices_under_same_switch);

bool fh2m_inno_devices_under_same_RC(inno_pci_dev *pci_dev1, inno_pci_dev *pci_dev2)
{
	struct pci_bus *bus1;
	struct pci_bus *bus2;
	struct pci_dev *pdev1, *pdev2;
	if ((pci_dev1 == NULL) || (pci_dev2 == NULL))
		return NULL;

	pdev1 = (struct pci_dev *)pci_dev1;
	pdev2 = (struct pci_dev *)pci_dev2;

	bus1 = pdev1->bus;
	while (bus1->parent != NULL)
	{
		bus1 = bus1->parent;
	}

	bus2 = pdev2->bus;
	while (bus2->parent != NULL)
	{
		bus2 = bus2->parent;
	}

	if (bus1 == bus2)
	{
		return true;
	}
	return false;
}
INNO_EXT_SYM(fh2m_inno_devices_under_same_RC);

DEFINE_RAW_SPINLOCK(pci_lock);
#define PCI_byte_BAD 0
#define PCI_dword_BAD (pos & 3)

static DECLARE_WAIT_QUEUE_HEAD(pci_cfg_wait);

static noinline void pci_wait_cfg(struct pci_dev *dev)
	__must_hold(&pci_lock)
{
	do {
		raw_spin_unlock_irq(&pci_lock);
		wait_event(pci_cfg_wait, !dev->block_cfg_access);
		raw_spin_lock_irq(&pci_lock);
	} while (dev->block_cfg_access);
}

/* Returns 0 on success, negative values indicate error. */
#define PCI_USER_READ_CONFIG(size, type)					\
int fh2m_inno_pci_user_read_config_##size						\
	(inno_pci_dev *pdev, int pos, type *val)			\
{									\
	int ret = PCIBIOS_SUCCESSFUL;					\
	u32 data = -1;							\
	struct pci_dev *dev = (struct pci_dev *)pdev;		\
	if (PCI_##size##_BAD)						\
		return -EINVAL;						\
	raw_spin_lock_irq(&pci_lock);				\
	if (unlikely(dev->block_cfg_access))				\
		pci_wait_cfg(dev);					\
	ret = dev->bus->ops->read(dev->bus, dev->devfn,			\
					pos, sizeof(type), &data);	\
	raw_spin_unlock_irq(&pci_lock);				\
	*val = (type)data;						\
	return pcibios_err_to_errno(ret);				\
}									\
INNO_EXT_SYM(fh2m_inno_pci_user_read_config_##size);

PCI_USER_READ_CONFIG(byte, u8)
PCI_USER_READ_CONFIG(dword, u32)

void fh2m_inno_pci_read_config_dword(inno_pci_dev *dev, int offset, u32 *rval)
{
	struct pci_dev *pdev = (struct pci_dev *)dev;
	u32 val;

	if (dev == NULL)
		return;

	pdev = (struct pci_dev *)dev;
	pci_read_config_dword(pdev, offset, &val);
	*rval = val;
}
INNO_EXT_SYM(fh2m_inno_pci_read_config_dword);

void fh2m_inno_pci_write_config_dword(inno_pci_dev *dev, int offset, u32 wval)
{
	struct pci_dev *pdev;

	if (dev == NULL)
		return;

	pdev = (struct pci_dev *)dev;
	pci_write_config_dword(pdev, offset, wval);
}
INNO_EXT_SYM(fh2m_inno_pci_write_config_dword);

void inno_pci_cfgspace_dump(inno_pci_dev *pdev, int reg_dump_num)
{
	int reg_count;
	u32 val[4] = {0};
	if(reg_dump_num > 256)
		reg_dump_num = 256;

	for (reg_count = 0; reg_count < reg_dump_num; reg_count++) {
		fh2m_inno_pci_read_config_dword(pdev, reg_count * 16, &val[0]);
		fh2m_inno_pci_read_config_dword(pdev, reg_count * 16 + 4, &val[1]);
		fh2m_inno_pci_read_config_dword(pdev, reg_count * 16 + 8, &val[2]);
		fh2m_inno_pci_read_config_dword(pdev, reg_count * 16 + 12, &val[3]);
		fh2m_inno_printk("[cfg space %#x]: %#x %#x %#x %#x\n", reg_count * 16, val[0], val[1], val[2], val[3]);
	};
}
/**
 * innogpu_pci_dump_cfgspace_regs - A acpi debug function to debug bug15777
 * @pci_dev: pointer to pci device
 * @mask: acpi_debug_mask
 *
 * note: In order to debug bug15777. When using Hygon CPU, PCIe speed drops from GEN3 to GEN2, it may crash during the shutdown process.
 * The specific functions are controlled by the module parameter acpi_debug_mask.
 *	0x0 - disable,
 *	0x1 - read 1024 pci cfgspace regs, executed 5 times
 *	0x2 - delay 2s, executed 5 times,
 *	0x3 - execute 1&2,
 *	default is 0
 */
void innogpu_pci_dump_cfgspace_regs(inno_pci_dev *pci_dev, unsigned int mask)
{
	if (mask & 3) {
		int reg_dump_num, loop;
		struct pci_dev *pdev = (struct pci_dev *)pci_dev;

		for (loop = 1; loop < 6; loop++) {
			if (mask & 2) {
				inno_pci_notice(&pdev->dev, "%s: start sleep 2s, acpi_debug_mask: %d, loop: %d\n", __func__, mask, loop);
				fh2m_inno_usleep_range(DELAY_2_SECONDS, DELAY_2_SECONDS);
			}
			if (mask & 1) {
				reg_dump_num = 256;
				inno_pci_notice(&pdev->dev, "%s: start read 1024 pcie cfgspace regs, acpi_debug_mask: %d, loop: %d\n", __func__, mask, loop);
				inno_pci_cfgspace_dump(&pdev->dev, reg_dump_num);
			}
		}
	}
}