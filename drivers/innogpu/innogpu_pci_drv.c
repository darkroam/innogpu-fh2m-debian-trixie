/*************************************************************************/ /*!
@File           innogpu_pci_drv.c
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sizes.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include "inno_plat_dev.h"
#include "inno_misc.h"
#include "inno_timer.h"
#include "inno_task.h"
#include "inno_drm.h"
#include "inno_pci.h"

#include "innogpu_pci_drv.h"
#include "inno_kernel_hook.h"
#include "fixup_alignment.h"
#include "fixup_pcie.h"
#ifdef CONFIG_DRM_INNO_SRVKM
#include "pvr_drv.h"
#include "innogpu_drm.h"
#endif
#include "kernel_compatibility.h"

#ifdef CONFIG_DRM_INNO_SMMU
#include "innosmmu_drv.h"
#endif

#ifdef CONFIG_DRM_INNO_DMA
#include "innodma.h"
#endif

#ifdef CONFIG_DRM_INNO_PMBUS
#include "innopmbus_drv.h"
#endif

#ifdef CONFIG_DRM_INNO_VPU
#include "innovpu_drv.h"
#endif

#ifdef CONFIG_DRM_INNO_DEBUG
#include "gpu_info.h"
#endif

#ifdef CONFIG_DRM_INNO_SR
#include "innosr_drv.h"
#endif

#if defined(CONFIG_MTRR)
#include <asm/mtrr.h>
#endif

#ifdef CONFIG_DRM_INNO_POWER
#include "innopower.h"
#endif

MODULE_DESCRIPTION("Innogpu framework driver");

static DEFINE_IDA(pcie_func_ida);

int card_cnt = 8;
module_param(card_cnt, int, 0444);
MODULE_PARM_DESC(card_cnt,"pcie card count, default is 8");

ulong dpu_mem_total_size = 0x20000000;
module_param(dpu_mem_total_size, ulong, 0444);
MODULE_PARM_DESC(dpu_mem_total_size,"DPU_MEM_TOTAL_SIZE, default is 512M");

ulong vpu_mem_total_size = 0x20000000;
module_param(vpu_mem_total_size, ulong, 0444);
MODULE_PARM_DESC(vpu_mem_total_size,"VPU_MEM_TOTAL_SIZE, default is 512M");

ulong limited_vpu_and_dpu_total_size = 0x80000000;
module_param(limited_vpu_and_dpu_total_size, ulong, 0444);
MODULE_PARM_DESC(limited_vpu_and_dpu_total_size,"vpu and dpu total size in case of limiting vpu access range, default is 2G, max is 4G");

ulong apu_mem_total_size = 0x20000000;
module_param(apu_mem_total_size, ulong, 0444);
MODULE_PARM_DESC(apu_mem_total_size,"APU_MEM_TOTAL_SIZE, default is 512M");

ulong mixed_mem_size = 0x21400000;
module_param(mixed_mem_size, ulong, 0444);
MODULE_PARM_DESC(mixed_mem_size,"mixed mem size reserved memory size in bytes (audio+dma+apu), default is 532M");

ulong dma_mem_block_size = 0x1000000;
module_param(dma_mem_block_size, ulong, 0444);
MODULE_PARM_DESC(dma_mem_block_size, "dma mem block size alloc from mixed zone, default is 16M");

ulong vram_reserved_size = 0x1400000;
module_param(vram_reserved_size, ulong, 0444);
MODULE_PARM_DESC(vram_reserved_size,"vram reserved size for mcufw/vbios/mcufw_comm etc. default is 20M, should between 14MB~64MB , default is 20M");

bool memsize_en = false;
module_param(memsize_en, bool, 0444);
MODULE_PARM_DESC(memsize_en,"enable/disable above memory size, default is disabled");

bool is_need_limit_vpu_and_dpu = true;
module_param(is_need_limit_vpu_and_dpu, bool, 0444);
MODULE_PARM_DESC(is_need_limit_vpu_and_dpu,"enable/disable limiting vpu and dpu in 4G range, default is true");

static unsigned long system_work_mode = HAL_SYSTEM_WORK_MODE_HOST;
module_param(system_work_mode, ulong, 0444);
MODULE_PARM_DESC(system_work_mode,"system_work_mode host/pf/vf/dock");

#if defined(__INNO_DESKTOP__) && defined(__G1P_SOC__)
int gpu_mc_mode = HAL_GPU_WORK_MODE_MC2;
#else
int gpu_mc_mode = HAL_GPU_WORK_MODE_MC1;
#endif
module_param(gpu_mc_mode, int, 0444);
MODULE_PARM_DESC(gpu_mc_mode,"gpu_mc_mode MC1/MC2/MC4");

#if defined(__G0M_SOC__)
int prohibit_umd_gtt_alloc = 0;
module_param(prohibit_umd_gtt_alloc, int, 0600);
MODULE_PARM_DESC(prohibit_umd_gtt_alloc,"enable/disable user gtt alloc, default is enabled");
#endif

bool pci_config_hook_enable = true;
module_param(pci_config_hook_enable, bool, 0444);
MODULE_PARM_DESC(pci_config_hook_enable,"enable/disable pci config read hook enable, default is enabled");

int gpu_core_index = 0;
module_param(gpu_core_index, int, 0444);
MODULE_PARM_DESC(gpu_core_index,"default gpu0 core (default:0)");

//2xmc1 define second core index
int gpu_core_index2 = 2;
module_param(gpu_core_index2, int, 0444);
MODULE_PARM_DESC(gpu_core_index2,"default gpu2 core (default:2)");

int dpu_mem_right = 0;
module_param(dpu_mem_right, int, 0444);
MODULE_PARM_DESC(dpu_mem_right,"dpu_mem_right 0:left, 1:right (default:0)");

int gpu_core_num = 1;
module_param(gpu_core_num, int, 0444);
MODULE_PARM_DESC(gpu_core_num,"gpu_core_num 1/2/4 (default:1)");

int gpu_vram_num = 4;
module_param(gpu_vram_num, int, 0444);
MODULE_PARM_DESC(gpu_vram_num,"gpu_vram_num 1/2/4 (default:4)");

unsigned long gpu_vram_unit_size= 2;
module_param(gpu_vram_unit_size, ulong, 0444);
MODULE_PARM_DESC(gpu_vram_unit_size,"gpu_vram_unit_size in Gbytes");

int pmbus_nums = 3;
module_param(pmbus_nums, int, 0444);
MODULE_PARM_DESC(pmbus_nums,"pmbus nums, default(3)");

int nulldisplay_drm_pipe_num = 1;
module_param(nulldisplay_drm_pipe_num, int, 0444);
MODULE_PARM_DESC(nulldisplay_drm_pipe_num, "nulldisplay_drm_pipe_num 1/2 (default:1)");

bool dpu_hw_support_gtt = false;
module_param(dpu_hw_support_gtt, bool, 0400);
MODULE_PARM_DESC(dpu_hw_support_gtt, "dpu_hw_support_gtt");

bool gpu_nulldisplay = false;
module_param(gpu_nulldisplay, bool, 0400);
MODULE_PARM_DESC(gpu_nulldisplay, "nulldisplay");

int snd_card_num = 1;
module_param(snd_card_num, int, 0400);
MODULE_PARM_DESC(snd_card_num, "sound card number,default 1");

#if defined(SUPPORT_PCIE_SRIOV)&&defined(SRIOV_VF_MODE)
int vpu_nums = 2;
#else
int vpu_nums = 6;
#endif
module_param(vpu_nums, int, 0444);
MODULE_PARM_DESC(vpu_nums,"vpu nums, default(4)");

int apu_nums= 0;
module_param(apu_nums, int, 0444);
MODULE_PARM_DESC(apu_nums,"apu nums 0/1/2, default(0)");

int sr_nums= 1;
module_param(sr_nums, int, 0444);
MODULE_PARM_DESC(sr_nums,"sr nums, default(1)");

int itlv = 0;
module_param(itlv, int, 0444);
MODULE_PARM_DESC(itlv,"interleave mode, 1:lv1 4x1G, 2:lv1 2+2G, 3:lv2 4x1G, default(1)");

int g_param_bar_len = 0;
module_param_named(bar_len, g_param_bar_len, int, 0444);
MODULE_PARM_DESC(bar_len, "bar len for inv debug, default(0) uints [M]");

int g_param_resize_len = 0;
module_param_named(resize_len, g_param_resize_len, int, 0444);
MODULE_PARM_DESC(resize_len, "resize len for  debug, default(0) uints [M]");

bool g0m_axi_lt_info_en = false;
module_param(g0m_axi_lt_info_en, bool, 0444);
MODULE_PARM_DESC(g0m_axi_lt_info_en,"axi_lt_info_en , default(false)");

bool g0m_gtt_dma_mask_256G = true;
module_param(g0m_gtt_dma_mask_256G, bool, 0444);
MODULE_PARM_DESC(g0m_gtt_dma_mask_256G,"g0m_gtt_dma_mask_256G , default(true)");

/*special module param: used to limit test for hwtest*/
bool unlimit_freq = false;
module_param(unlimit_freq, bool, 0444);
MODULE_PARM_DESC(unlimit_freq,"unlimit_freq_test for hw limit test , default(false)");

bool mod_update_voltage_enable = true;
module_param(mod_update_voltage_enable, bool, 0444);
MODULE_PARM_DESC(mod_update_voltage_enable,"mod_update_voltage_enable, default(true)");

bool adj_voltage_perstep = true;
module_param(adj_voltage_perstep, bool, 0444);
MODULE_PARM_DESC(adj_voltage_perstep,"adj_voltage_perstep , default(true)");

int idle_voltage = 770;
module_param(idle_voltage, int, 0644);
MODULE_PARM_DESC(idle_voltage, "idle_voltage, default(770) and invalid");

int pwrd_l = 0;
module_param(pwrd_l, int, 0644);
MODULE_PARM_DESC(pwrd_l, "pwrd_l, default(0)");

bool enable_dyn_freq = true;
module_param(enable_dyn_freq, bool, 0444);
MODULE_PARM_DESC(enable_dyn_freq, "enable_dyn_freq for test, default(false)");

int mod_pcie_drop_timeout = 10;
module_param(mod_pcie_drop_timeout, int, 0644);
MODULE_PARM_DESC(mod_pcie_drop_timeout, "mod_pcie_drop_timeout, default(10ms)");

int vkms_nums = 0;

#define PCI_DEV_ATTR_NAME ("gpu-info")

bool g0_is_g0c = false;
module_param(g0_is_g0c, bool, 0600);
MODULE_PARM_DESC(g0_is_g0c,"G0 is G0C");

bool g0m_raw_enable = false;
module_param(g0m_raw_enable, bool, 0600);
MODULE_PARM_DESC(g0m_raw_enable,"G0M RAW ENABLE");

int firmware_en = 0;
module_param(firmware_en, int, 0644);
MODULE_PARM_DESC(firmware_en, "enable/disable firmware analysis hwinfo first");

int card_work_mode = 0;
module_param(card_work_mode, int, 0644);
MODULE_PARM_DESC(card_work_mode, "card_work_mode is used to validation default(0)");

int s_max_width = 16384;
int s_max_height = 16384;
module_param(s_max_width, int, 0444);
MODULE_PARM_DESC(s_max_width, "INNODPU_WIDTH_MAX (default: 16384)");
module_param(s_max_height, int, 0444);
MODULE_PARM_DESC(s_max_height, "INNODPU_HEIGHT_MAX (default: 16384)");

#if defined(SUPPORT_VGPU_GUEST)
unsigned int s_vkms_width = 1920;
unsigned int s_vkms_height = 1080;
#else
unsigned int s_vkms_width = 3840;
module_param(s_vkms_width, uint, 0600);
MODULE_PARM_DESC(s_vkms_width, "vkms default width (default: 3840)");

unsigned int s_vkms_height = 2160;
module_param(s_vkms_height, uint, 0600);
MODULE_PARM_DESC(s_vkms_height, "vkms default height (default: 2160)");
#endif

#if defined(__INNO_DESKTOP__) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
int inno_s4_imgsize_en = 1;
module_param(inno_s4_imgsize_en, int, 0644);
MODULE_PARM_DESC(inno_s4_imgsize_en, "enable/disable to set /sys/power/image_size, 0: disable, non-zero: enable, default(1)");

unsigned long inno_s4_imgsize = 0;
int inno_s4_imgsize_changed = 0;
#endif

/* TODO ====================== g3 add ===================== */
//#if defined(__G3_NE__) || defined(__G3_PAL__) || defined(__G3_SOC__)
unsigned int gpu_mode = 0;
module_param(gpu_mode, uint, 0600);
MODULE_PARM_DESC(gpu_mode, "gpu mode configure: default 0");

int monitor_mode = 0;
module_param(monitor_mode, int, 0444);
MODULE_PARM_DESC(monitor_mode, "monitor mode, 0:close all_monitor, 1:open all_monitor, 2:open monitor except for gpu_monitor, default(0)");

bool raw_sync_enable = true;
module_param(raw_sync_enable, bool, 0600);
MODULE_PARM_DESC(raw_sync_enable, "RAW SYNC ENABLE");

bool read_after_write_bypass = false;
module_param(read_after_write_bypass, bool, 0400);
MODULE_PARM_DESC(read_after_write_bypass, "pcie read after write in order bypass disable");

unsigned int read_after_write_timeout = 65535;
module_param(read_after_write_timeout, uint, 0400);
MODULE_PARM_DESC(read_after_write_timeout, "pcie read after write in order fence max timeout");

unsigned int hash_type = 0;
module_param(hash_type, uint, 0600);
MODULE_PARM_DESC(hash_type, "hash type configure: default 0");

unsigned int hash_version = 0;
module_param(hash_version, uint, 0600);
MODULE_PARM_DESC(hash_version, "hash version configure: default 0");

unsigned int div_addr_en = 0;
module_param(div_addr_en, uint, 0600);
MODULE_PARM_DESC(div_addr_en, "div addr en configure: default 0");

bool use_real_ddr = true;
module_param(use_real_ddr, bool, 0600);
MODULE_PARM_DESC(use_real_ddr, "paladin use real ddr or not");

bool do_deep_test = false;
module_param(do_deep_test, bool, 0600);
MODULE_PARM_DESC(do_deep_test, "do deep test such as ddr for chip");

bool sparse_ddr_test = true;
module_param(sparse_ddr_test, bool, 0600);
MODULE_PARM_DESC(sparse_ddr_test, "true:test ddr space with sparce mode, false: full ddr space test");

int sparse_ddr_stride = 1;
module_param(sparse_ddr_stride, int, 0600);
MODULE_PARM_DESC(sparse_ddr_stride, "ddr deep test sparse stride, default(1) uints [KByte]");

int pciedma_nums = 1;
module_param(pciedma_nums, int, 0600);
MODULE_PARM_DESC(pciedma_nums, "pciedma nums, default(1)");

int axidma_nums = 1;
module_param(axidma_nums, int, 0600);
MODULE_PARM_DESC(daxidma_nums, "axidma nums, default(1)");

int innolink_enable = 0;
module_param(innolink_enable, int, 0600);
MODULE_PARM_DESC(innolink_enable, "innolink enable or disable, use 1 or 0");

int gpu_core_clock = 5; // defualt to 5M
module_param(gpu_core_clock, int, 0600);
MODULE_PARM_DESC(gpu_core_clock, "set gpu core clock for fpga, default is 2M");
//#endif

int interrupt_mode = 2;
module_param(interrupt_mode, int, 0644);
MODULE_PARM_DESC(interrupt_mode, "choose interrupt mode, 0-INTX 1-MSI 2-default");

int gtt_sort_enable = 0;
module_param(gtt_sort_enable, int, 0644);
MODULE_PARM_DESC(gtt_sort_enable, "enable gtt pages sort, improve dma performance. 0-disable, 1-enable, default is 0");

int multi_memory_regions_en = 0;
module_param(multi_memory_regions_en, int, 0444);
MODULE_PARM_DESC(multi_memory_regions_en, "enable/disable support allocation of multiple memory regions, 0-disable 1-enable");

unsigned int acpi_debug_mask = 0;
module_param(acpi_debug_mask, uint, 0644);
MODULE_PARM_DESC(acpi_debug_mask, 	"acpi debug mask control. "
									"bit0: disable/enable read pci cfgspace regs 5 times; "
									"bit1: disable/enable delay 2s 5times; "
									"bit2 - disable/enable read cfgspace、ec-bar0、ec-bar2 at the brginning of wait_mcufw_ready. " 
									"default is 0.");

ssize_t gpu_info_show(struct device *dev, struct device_attribute *attr, char *buf);

static struct device_attribute dev_attr_gpu_info =
	__ATTR(gpu-info, S_IRUGO, gpu_info_show, NULL);

#define INNO_RESIZE_SUCCESS       (0)
#define INNO_RESIZE_NULL_POINTER  (-1)
#define INNO_RESIZE_NO_SUPPORT    (1)

#define DDR_SIZE_128M       (0x8000000ULL)
#define DDR_SIZE_256M       (0x10000000ULL)
#define DDR_SIZE_512M       (0x20000000ULL)
#define DDR_SIZE_1G         (0x40000000ULL)
#define DDR_SIZE_1_5G       (0x60000000ULL)
#define DDR_SIZE_2G         (0x80000000ULL)
#define DDR_SIZE_2_5G       (0xa0000000ULL)
#define DDR_SIZE_3G         (0xc0000000ULL)
#define DDR_SIZE_3_5G       (0xe0000000ULL)
#define DDR_SIZE_4G         (0x100000000ULL)
#define DDR_SIZE_4_5G       (0x120000000ULL)
#define DDR_SIZE_5G         (0x140000000ULL)
#define DDR_SIZE_5_5G       (0x160000000ULL)
#define DDR_SIZE_6G         (0x180000000ULL)
#define DDR_SIZE_6_5G       (0x1a0000000ULL)
#define DDR_SIZE_7G         (0x1c0000000ULL)
#define DDR_SIZE_7_5G       (0x1e0000000ULL)
#define DDR_SIZE_8G         (0x200000000ULL)
#define DDR_SIZE_16G        (0x400000000ULL)
#define DDR_SIZE_32G        (0x800000000ULL)
#define DDR_SIZE_64G        (0x1000000000ULL)
#define SIZE_128M_INDEX     (7)
#define SIZE_256M_INDEX     (8)
#define SIZE_512M_INDEX     (9)
#define SIZE_1G_INDEX       (10)
#define SIZE_2G_INDEX       (11)
#define SIZE_4G_INDEX       (12)
#define SIZE_8G_INDEX       (13)
#define SIZE_16G_INDEX      (14)
#define SIZE_32G_INDEX      (15)
#define SIZE_64G_INDEX      (16)

#define SIZE_INVALID_INDEX  (0xff)
#define FLAG_INVALID        (0)
#define FLAG_VALID          (1)


static int revert_size_to_ddr_index(struct dev_rsrc *pdev_rsrc, uint64_t size)
{
	int ret = SIZE_INVALID_INDEX;

	switch (size) {
	case 0:
		ret = 0;
		pcie_error(pdev_rsrc->dev, "not support mem bar hardware info in current fimware version, size: 0x%llx \n", size);
		break;
	case DDR_SIZE_128M:
		ret = SIZE_128M_INDEX;
		break;
	case DDR_SIZE_256M:
		ret = SIZE_256M_INDEX;
		break;
	case DDR_SIZE_512M:
		ret = SIZE_512M_INDEX;
		break;
	case DDR_SIZE_1G:
		ret = SIZE_1G_INDEX;
		break;
	case DDR_SIZE_1_5G:
	case DDR_SIZE_2G:
		ret = SIZE_2G_INDEX;
		break;
	case DDR_SIZE_2_5G:
	case DDR_SIZE_3G:
	case DDR_SIZE_3_5G:
	case DDR_SIZE_4G:
		ret = SIZE_4G_INDEX;
		break;
	case DDR_SIZE_4_5G:
	case DDR_SIZE_5G:
	case DDR_SIZE_5_5G:
	case DDR_SIZE_6G:
	case DDR_SIZE_6_5G:
	case DDR_SIZE_7G:
	case DDR_SIZE_7_5G:
	case DDR_SIZE_8G:
		ret = SIZE_8G_INDEX;
		break;
	case DDR_SIZE_16G:
		ret = SIZE_16G_INDEX;
		break;
	case DDR_SIZE_32G:
		ret = SIZE_32G_INDEX;
		break;
	default:
		ret = SIZE_INVALID_INDEX;
		break;
	}

	return ret;
}

static int inno_pci_get_hardware_bar_index(struct dev_rsrc* pdev_rsrc)
{
	extern int g_param_resize_len;
	uint64_t size = 0;
	int size_index;
	uint64_t param_resize_len;

	param_resize_len = (uint64_t)((uint64_t)g_param_resize_len * (0x100000ULL));
	if(pdev_rsrc->chip.get_hw_size)
	{
		if(hal_init_sys_bar(pdev_rsrc))
		{
			return SIZE_INVALID_INDEX;
		}

		size = pdev_rsrc->chip.get_hw_size(pdev_rsrc);
		hal_deinit_sys_bar(pdev_rsrc);

	} else {
		size = 0;
	}

	size = (g_param_resize_len != 0) ? param_resize_len : size;
	size_index = revert_size_to_ddr_index(pdev_rsrc, size);

	pcie_dbg(pdev_rsrc->dev, "[hw] size_index:%d, size:0x%llx \n", size_index, size);
	return size_index;
}

static int inno_pci_get_fireware_bar_index(struct dev_rsrc* pdev_rsrc)
{
	uint64_t fw_size;
	int size_index;

	if(pdev_rsrc->chip.get_fw_size)
	{
		fw_size = pdev_rsrc->chip.get_fw_size(pdev_rsrc);
	} else {
		fw_size = 0;
	}

	size_index = revert_size_to_ddr_index(pdev_rsrc, fw_size);

	return size_index;
}

static int inno_pci_check_resize(struct dev_rsrc* pdev_rsrc, int hardware_bar_index, int fireware_bar_index)
{
	int size_index = 0;
	int fireware_flag = FLAG_VALID;
	int hardware_flag = FLAG_VALID;

	if ((0 == fireware_bar_index) || (SIZE_INVALID_INDEX == fireware_bar_index)) {
		pcie_error(pdev_rsrc->dev, ", get fireware_bar_index:%d fail !!!  \n", fireware_bar_index);
		fireware_flag = FLAG_INVALID;
	}

	if ((0 == hardware_bar_index) || (SIZE_INVALID_INDEX == hardware_bar_index)){
		pcie_error(pdev_rsrc->dev, "get hardware_bar_index:%d fail !!!  \n", hardware_bar_index);
		hardware_flag = FLAG_INVALID;
	}

	if ((FLAG_VALID == fireware_flag) && (FLAG_INVALID == hardware_flag)) {
		size_index = fireware_bar_index;
	} else if ((FLAG_INVALID == fireware_flag) && (FLAG_VALID == hardware_flag)) {
		size_index = hardware_bar_index;
	} else if ((FLAG_VALID == fireware_flag) && (FLAG_VALID == hardware_flag)) {
		/* 1 hardware_bar_index ==	fireware_bar_index*/
		/* 2 hardware_bar_index !=	fireware_bar_index*/
		size_index = hardware_bar_index;
	} else {
		/* (FLAG_INVALID == fireware_flag) && (FLAG_INVALID == hardware_flag) */
		size_index = FLAG_INVALID;
	}

	pcie_dbg(pdev_rsrc->dev, ", size_index:%d fireware_flag:%d, hardware_flag:%d \n", size_index, fireware_flag, hardware_flag);
	return size_index;
}

static ssize_t inno_pll_read_file(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	ssize_t pos = 0;
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	char *buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (!dev)
		return -EINVAL;

	pos = hal_show_pll(dev, buf, 0);
	pos = simple_read_from_buffer(user_buf, count, ppos, buf, pos);

	free_page((unsigned long)buf);
	return pos;
}

static ssize_t inno_pll_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	uint32_t pll;
	char *buf;
	char name[10] = { 0 };
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	if (!dev)
		return -EINVAL;

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (fh2m_inno_copy_from_user(buf, user_buf, min(count, PAGE_SIZE))) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	sscanf(buf, "%s%d", name, &pll);
	ret = fh2m_hal_set_pll_by_name(dev, name, pll);
	free_page((unsigned long)buf);

	return (ret == 0) ? count : -EINVAL;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static const struct file_operations inno_pll_fops = {
	.open = simple_open,
	.read = inno_pll_read_file,
	.write = inno_pll_write_file,
};
#else
static const struct proc_ops inno_pll_fops = {
	.proc_open = simple_open,
	.proc_read = inno_pll_read_file,
	.proc_write = inno_pll_write_file,
};
#endif

static void set_module_params(struct pci_dev *pdev) {
	unsigned long long len;

	len = fh2m_inno_pci_resource_len(pdev, 2);
	if (card_work_mode == 1) { /*1:factory mode  0:release mode*/
		s_vkms_width = 1920;
		s_vkms_height = 1080;
#if defined(__G1P_SOC__)
		if (len <= 0x20000000)
			gpu_mc_mode = 2;
		else
			gpu_core_num = 4;
#endif
		unlimit_freq = true;
		enable_dyn_freq = false;
	}

	#if defined (__INNO_DESKTOP__)
		gpu_core_num = 1;
		pcie_info(&pdev->dev, " __INNO_DESKTOP__(Desktop)  gpu_core_num=%d", gpu_core_num);
	#elif defined (__INNO_SERVER__)
		if(gpu_mc_mode == HAL_GPU_WORK_MODE_MC1){
			if (gpu_core_num > 4)
				gpu_core_num = 4;
		}else if(gpu_mc_mode == HAL_GPU_WORK_MODE_MC2){
			if (gpu_core_num > 2)
				gpu_core_num = 2;
		}if(gpu_mc_mode == HAL_GPU_WORK_MODE_MC4){
			if (gpu_core_num > 1)
				gpu_core_num = 1;
		}
		pcie_info(&pdev->dev, " __INNO_SERVER__(Server)  gpu_core_num=%d", gpu_core_num);
	#else
		gpu_core_num = 1;
		pcie_info(&pdev->dev, " __INNO_DESKTOP__(Default)  gpu_core_num=%d", gpu_core_num);
	#endif
}

static void print_module_params(struct pci_dev *pdev) {
	pcie_info(&pdev->dev, "--------module_param--------");
	pcie_info(&pdev->dev, "card count   =%d", card_cnt);
	pcie_info(&pdev->dev, "dpu_mem_total_size   =0x%lx", dpu_mem_total_size);
	pcie_info(&pdev->dev, "vpu_mem_total_size   =0x%lx", vpu_mem_total_size);
	pcie_info(&pdev->dev, "limited_vpu_and_dpu_total_size   =0x%lx", limited_vpu_and_dpu_total_size);
	pcie_info(&pdev->dev, "apu_mem_total_size   =0x%lx", apu_mem_total_size);
	pcie_info(&pdev->dev, "mixed_mem_size       =0x%lx (audio+dma+apu)", mixed_mem_size);
	pcie_info(&pdev->dev, "memsize_en       	=%d", memsize_en);
	pcie_info(&pdev->dev, "is_need_limit_vpu_and_dpu =%d", is_need_limit_vpu_and_dpu);
	pcie_info(&pdev->dev, "system_work_mode     =%lu", system_work_mode);
	pcie_info(&pdev->dev, "gpu_mc_mode          =%d", gpu_mc_mode);
	pcie_info(&pdev->dev, "gpu_core_index       =%d (Only MC1 valid)", gpu_core_index);
	pcie_info(&pdev->dev, "gpu_core_num         =%d", gpu_core_num);
	pcie_info(&pdev->dev, "gpu_vram_num         =%d", gpu_vram_num);
	pcie_info(&pdev->dev, "gpu_vram_unit_size   =%lu", gpu_vram_unit_size);
	pcie_info(&pdev->dev, "pmbus_nums           =%d", pmbus_nums);
	pcie_info(&pdev->dev, "vpu_nums             =%d", vpu_nums);
	pcie_info(&pdev->dev, "apu_nums             =%d", apu_nums);
#if defined(__INNO_DESKTOP__) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	pcie_info(&pdev->dev, "inno_s4_imgsize_en   =%d", inno_s4_imgsize_en);
#endif
	pcie_info(&pdev->dev, "---------------------------");
}

#if defined(CONFIG_MTRR) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0))
/*
 * A return value of:
 *      0 or more means success
 *     -1 means we were unable to add an mtrr but we should continue
 *     -2 means we were unable to add an mtrr but we shouldn't continue
 */
static int mtrr_setup(struct pci_dev *pdev,
		      resource_size_t mem_start,
		      resource_size_t mem_size)
{
	int err;
	int mtrr;

	/* Reset MTRR */
	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_UNCACHABLE, 0);
	if (mtrr < 0) {
		pcie_error(&pdev->dev, "mtrr_add failed (%d)\n", mtrr);
		mtrr = -2;
		goto err_out;
	}

	err = mtrr_del(mtrr, mem_start, mem_size);
	if (err < 0) {
		pcie_error(&pdev->dev, "mtrr_del failed (%d)\n", err);
		mtrr = -2;
		goto err_out;
	}

	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_WRBACK, 0);
	if (mtrr < 0) {
		/* Stop, but not an error as this may be already be setup */
		pcie_dbg(&pdev->dev, "mtrr_add failed (%d) - probably means the mtrr is already setup\n", mtrr);
		mtrr = -1;
		goto err_out;
	}

	err = mtrr_del(mtrr, mem_start, mem_size);
	if (err < 0) {
 		pcie_error(&pdev->dev, "mtrr_del failed (%d)\n", err);
		mtrr = -2;
		goto err_out;
	}

	if (mtrr == 0) {
		/* Replace 0 with a non-overlapping WRBACK mtrr */
		err = mtrr_add(0, mem_start, MTRR_TYPE_WRBACK, 0);
		if (err < 0) {
			pcie_error(&pdev->dev, "mtrr_add failed (%d)\n", err);
			mtrr = -2;
			goto err_out;
		}
	}

	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_WRCOMB, 0);
	if (mtrr < 0) {
		pcie_error(&pdev->dev, "mtrr_add failed (%d)\n", mtrr);
		mtrr = -1;
	}

err_out:
	return mtrr;
}
#endif /* defined(CONFIG_MTRR) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) */

static __maybe_unused int dr_mtrr_setup(struct dev_rsrc *pdev_rsrc)
{
	struct pci_dev *pdev = pdev_rsrc->pdev;
	resource_size_t bar_start, bar_len;
	int err = 0;

	bar_start = pci_resource_start(pdev, pdev_rsrc->chip.ddr_bar_num);
	bar_len = pdev_rsrc->ddr_bar_len;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	/* Register the LMA as write combined */
	err = arch_io_reserve_memtype_wc(bar_start, bar_len);
	if (err)
		return -ENODEV;
#endif
	/* Enable write combining */
	pdev_rsrc->mtrr = arch_phys_wc_add(bar_start, bar_len);
	if (pdev_rsrc->mtrr < 0) {
		err = -ENODEV;
		goto err_out;
	}

#elif defined(CONFIG_MTRR)
	/* Enable mtrr region caching */
	pdev_rsrc->mtrr = mtrr_setup(pdev, bar_start, bar_len);
	if (pdev_rsrc->mtrr == -2) {
		err = -ENODEV;
		goto err_out;
	}
#endif
	return err;

err_out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	arch_io_free_memtype_wc(bar_start, bar_len);
#endif
	return err;
}

static __maybe_unused void dr_mtrr_cleanup(struct dev_rsrc *pdev_rsrc)
{
	struct pci_dev *pdev = pdev_rsrc->pdev;
	if (pdev_rsrc->mtrr >= 0) {
		resource_size_t bar_start, bar_len;

		bar_start = pci_resource_start(pdev, pdev_rsrc->chip.ddr_bar_num);
		bar_len = pdev_rsrc->ddr_bar_len;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
		arch_phys_wc_del(pdev_rsrc->mtrr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
		arch_io_free_memtype_wc(bar_start, bar_len);
#endif
#elif defined(CONFIG_MTRR)
		int err;

		err = mtrr_del(pdev_rsrc->mtrr, bar_start, bar_len);
		if (err < 0)
			pcie_error(pdev_rsrc->dev, "mtrr_del failed (%d)\n", err);
#endif
	}
}

static int dev_rsrc_deinit(struct pci_dev* pdev) {

	struct dev_rsrc* pdev_rsrc = fh2m_inno_rsrc_devres_find(&pdev->dev);
	char temp_str[128];
	int err = 0;

	if (!pdev_rsrc) {
		pcie_error(&pdev->dev, "[%s:%d] dev_rsrc is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

#if defined(SUPPORT_DMA_TRANSFER)
	fh2m_hal_vram_dma_mem_pool_deinit(pdev_rsrc->dev);
	fh2m_hal_dma_deinit(pdev_rsrc);
#endif

	hal_efuse_deinit(pdev_rsrc);

	hal_bmc_deinit(pdev_rsrc);

	fh2m_hal_deinit_mcufw_comm(pdev_rsrc);

#if !defined(CONFIG_LOONGARCH) && !defined(CONFIG_MIPS)
	dr_mtrr_cleanup(pdev_rsrc);
#endif

#if defined(SUPPORT_ION)
#define	INNOGPU_DDR_BAR 2
	innogpu_ion_deinit(pdev_rsrc, INNOGPU_DDR_BAR);
#endif

	iounmap(pdev_rsrc->sys_reg);

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	hal_hwinfo_deinit(pdev_rsrc);
#endif

	snprintf(temp_str, sizeof(temp_str), "%s%d", "test_temp", pdev_rsrc->pcie_func_idx);
	if (pdev_rsrc->test_temp_debugfs)
		fh2m_inno_debugfs_or_procfs_remove_file(pdev_rsrc->test_temp_debugfs, temp_str, pdev_rsrc->debugfs_hwdir);
	snprintf(temp_str, sizeof(temp_str), "%s%d", "syspll", pdev_rsrc->pcie_func_idx);
	if (pdev_rsrc->syspll_debugfs)
		fh2m_inno_debugfs_or_procfs_remove_file(pdev_rsrc->syspll_debugfs, temp_str, pdev_rsrc->debugfs_hwdir);

	if (pdev_rsrc->debugfs_hwdir)
		fh2m_inno_debugfs_or_procfs_remove_dir(pdev_rsrc->debugfs_hwdir);

	if (pdev_rsrc->debugfs_dir)
		fh2m_inno_debugfs_or_procfs_remove_dir(pdev_rsrc->debugfs_dir);

	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_gpu_info.attr);

	return err;
}

static int innogpu_resize_resume(struct dev_rsrc* pdev_rsrc)
{
	int ddr_index;
	int ret;

	ddr_index = pdev_rsrc->chip.resize_ddr_index;

	if (ddr_index) {
		/*
		 * Workaround: mcufw maybe 4times reset(normal is 3times) low probability when s3 wakeup on 3a5000+7a1000 + t-g0c.
		 * In this scenario, pcie restore rebar_ctrl 0xd22 fail to 0x922, because mcufw set rebar_ctrl 0x922 too later or
		 * host pci_pm_resume_noirq write rebar_ctrl 0xd22 donot take effect. so check and rewrite again.
		 * */
		if (fh2m_inno_loongson_pcie_bridge_7a1000(pdev_rsrc->pdev))
			fh2m_inno_pci_fixup_rebar_state(pdev_rsrc->pdev);

		if (pdev_rsrc->chip.set_resize_control) {
			ret = pdev_rsrc->chip.set_resize_control(pdev_rsrc, ddr_index);
			if(ret){
				return -EINVAL;
			}

		}
	}

	return 0;
}

static int innogpu_resize_init(struct dev_rsrc* pdev_rsrc)
{
	int hardware_bar_index;
	int fireware_bar_index;
	int ddr_index;
	int old_ddr_index;
	int ret;
	int repo_value = 0xff;
	struct chip_obj *chip = &pdev_rsrc->chip;

	hardware_bar_index = inno_pci_get_hardware_bar_index(pdev_rsrc);
	fireware_bar_index = inno_pci_get_fireware_bar_index(pdev_rsrc);
	ddr_index = inno_pci_check_resize(pdev_rsrc, hardware_bar_index, fireware_bar_index);
	old_ddr_index = revert_size_to_ddr_index(pdev_rsrc, fh2m_inno_pci_resource_len(pdev_rsrc->pdev, chip->ddr_bar_num));

	if (ddr_index){
		ret = fh2m_inno_check_resize_version();
		if(INNO_RESIZE_NO_SUPPORT == ret) {
			pcie_notice(pdev_rsrc->dev, "linux version is lower than 4.15.0, not support resize func \n");
			return 0;
		}

		ret = fh2m_inno_check_rc_size(pdev_rsrc->pdev);
		if (INNO_RESIZE_NULL_POINTER == ret) {
			pcie_error(pdev_rsrc->dev, "Resize fail. null poninter \n");
			return 0;
		}else if (INNO_RESIZE_NO_SUPPORT == ret) {
			pcie_notice(pdev_rsrc->dev, "Resize fail. Trying to resize is pointless without a root hub window above 4GB \n");
			return 0;
		}

		if (pdev_rsrc->chip.set_resize_control) {
			ret = pdev_rsrc->chip.set_resize_control(pdev_rsrc, ddr_index);
			if(ret){
				return -EINVAL;
			}
		}

		ret = fh2m_inno_gpu_device_resize_fb_bar(pdev_rsrc->pdev, pdev_rsrc->chip.ddr_bar_num, ddr_index, &repo_value);
		if ((INNO_RESIZE_SUCCESS == ret) && (INNO_RESIZE_SUCCESS == repo_value)) {
			pdev_rsrc->chip.is_support_resize = true;
			pdev_rsrc->chip.resize_ddr_index = ddr_index;
			pcie_notice(pdev_rsrc->dev, "Resize success size:%d [MB] \n", (1 << ddr_index));
		} else if (INNO_RESIZE_NULL_POINTER == ret) {
			pcie_error(pdev_rsrc->dev, "Resize fail. null poninter \n");
		} else {
			if (pdev_rsrc->chip.set_resize_control) {
				ret = pdev_rsrc->chip.set_resize_control(pdev_rsrc, old_ddr_index);
				if(ret){
					pcie_notice(pdev_rsrc->dev, "revert to the original bar fail \n");
					return -EINVAL;
				}
				pdev_rsrc->chip.is_support_resize = true;
				pdev_rsrc->chip.resize_ddr_index = old_ddr_index;

				pcie_notice(pdev_rsrc->dev, "revert to the original bar size \n");
			}
		}
	}else{
		pcie_notice(pdev_rsrc->dev, "not support Resize !!! get fireware_bar_index and fireware_bar_index fail ddr_index:%d \n", ddr_index);
	}

	return 0;
}

static int innogpu_init_bars(struct dev_rsrc* pdev_rsrc)
{
	int ret;

	if (pdev_rsrc->chip.has_resize) {
		ret = innogpu_resize_init(pdev_rsrc);
		if(ret) {
			return ret;
		}
	}

	ret = hal_init_all_bars(pdev_rsrc);
	if (ret != 0)
		return ret;

	return fh2m_hal_init_vram_total_size(pdev_rsrc->dev);
}

ssize_t gpu_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return hal_gpu_info_show(dev, buf);
}

static ssize_t inno_test_temp_read_file(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	ssize_t pos = 0;
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	struct dev_rsrc* pdev_rsrc = NULL;
	char *buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (!dev)
		return -EINVAL;
	pdev_rsrc = fh2m_inno_rsrc_devres_find(dev);
	pos = fh2m_inno_sprintf(buf, 128, "test_temp[%d]\n", pdev_rsrc->test_temp);
	pos = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	free_page((unsigned long)buf);
	return pos;
}
static ssize_t inno_test_temp_write_file(struct file *file,
				const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	char *buf;
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	struct dev_rsrc* pdev_rsrc = NULL;

	if (!dev)
		return -EINVAL;
	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (fh2m_inno_copy_from_user(buf, user_buf, min(count, PAGE_SIZE))) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}
	pdev_rsrc = fh2m_inno_rsrc_devres_find(dev);
	sscanf(buf, "%d", &pdev_rsrc->test_temp);
	free_page((unsigned long)buf);
	ret = 0;
	pcie_notice(pdev_rsrc->dev, "pcidevid %d change temp to %d\n", pdev_rsrc->pcie_func_idx, pdev_rsrc->test_temp);

	return count;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static const struct file_operations inno_test_temp_fops = {
	.open = simple_open,
	.read = inno_test_temp_read_file,
	.write = inno_test_temp_write_file,
};
#else
static const struct proc_ops inno_test_temp_fops = {
	.proc_open = simple_open,
	.proc_read = inno_test_temp_read_file,
	.proc_write = inno_test_temp_write_file,
};
#endif

int get_test_temp(struct dev_rsrc* pdev_rsrc)
{
	return pdev_rsrc->test_temp;
}

bool is_unlimit_freq(void)
{
	return unlimit_freq;
}

void fh2m_mod_update_voltage_disable(void)
{
	mod_update_voltage_enable = false;
}

bool fh2m_is_mod_update_voltage_enable()
{
	return mod_update_voltage_enable;
}

bool support_adj_voltage_perstep(void)
{
	return adj_voltage_perstep;
}

bool is_enable_dyn_freq(void)
{
	return enable_dyn_freq;
}

int get_pwrd_l(void)
{
	return pwrd_l;
}

int get_mod_pcie_drop_timeout(void)
{
	return mod_pcie_drop_timeout;
}

int get_idle_voltage(void)
{
	return idle_voltage;
}

static void hw_check_notice(struct dev_rsrc* pdev_rsrc)
{
	uint32_t pcie_stat = 0, ddr_stat = 0, dbg_reserved_6 = 0;

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	fh2m_hal_bmc_read32(pdev_rsrc->dev, REG_ENTITY0035, &pcie_stat);
	fh2m_hal_bmc_read32(pdev_rsrc->dev, REG_ENTITY0037, &ddr_stat);
	fh2m_hal_bmc_read32(pdev_rsrc->dev, REG_ENTITY0055, &dbg_reserved_6);
#endif

	pcie_notice(pdev_rsrc->dev,
			"pcie_stat: 0x%x, ddr_stat: 0x%x, dbg_reserved_6: 0x%x\n",
			pcie_stat, ddr_stat, dbg_reserved_6);
}

static void dev_rsrc_reinit_nulldisp_by_hwinfo(struct dev_rsrc *pdev_rsrc)
{
	int hal_version = 0;
	unsigned short output_en = 0;
	unsigned short hdmi_dp_en = 0;
	unsigned short vga_lvds_en = 0;

	hal_version = fh2m_hal_hwinfo_version(pdev_rsrc->dev);
	if (hal_version == -1) {
	} else if (hal_version >= 0x7) {
		if (pdev_rsrc->chip.get_output_en) {
			fh2m_hal_get_output_en(pdev_rsrc->dev,&output_en);
			if (!output_en)
				gpu_nulldisplay = 1;
		}
	} else {
		if (pdev_rsrc->chip.get_hdmi_dp_en_status && pdev_rsrc->chip.get_lvds_vga_misc_en) {
			hdmi_dp_en = fh2m_hal_get_hdmi_dp_en_status(pdev_rsrc->dev);
			vga_lvds_en = fh2m_hal_get_lvds_vga_misc_en(pdev_rsrc->dev);
			if (!output_en && !hdmi_dp_en && !vga_lvds_en)
				gpu_nulldisplay = 1;
		}
	}

	if (gpu_nulldisplay) {
		pdev_rsrc->chip.rsrc_nums.drm_nums = gpu_core_num;
		pdev_rsrc->chip.rsrc_nums.dpu_nums = pdev_rsrc->chip.rsrc_nums.drm_nums * nulldisplay_drm_pipe_num;
		pdev_rsrc->chip.rsrc_nums.vkms_nums = pdev_rsrc->chip.rsrc_nums.drm_nums * nulldisplay_drm_pipe_num;
	}
}

static int dev_rsrc_init(struct dev_rsrc* pdev_rsrc, struct pci_dev* pdev)
{
	int err = 0;
	char temp_str[128];

	snprintf(temp_str, sizeof(temp_str), "%s%d", INNO_GPU_DEBUGFS_NAME, pdev_rsrc->pcie_func_idx);

	// 创建debugfs目录
	pdev_rsrc->debugfs_dir = fh2m_inno_debugfs_or_procfs_create_dir(temp_str, NULL);
	if (IS_ERR_OR_NULL(pdev_rsrc->debugfs_dir)) {
		return -1;
	}

	snprintf(temp_str, sizeof(temp_str), "%s%d", "hwinfo", pdev_rsrc->pcie_func_idx);
	pdev_rsrc->debugfs_hwdir = fh2m_inno_debugfs_or_procfs_create_dir(temp_str, pdev_rsrc->debugfs_dir);
	if (IS_ERR_OR_NULL(pdev_rsrc->debugfs_hwdir)) {
		fh2m_inno_debugfs_or_procfs_remove_dir(pdev_rsrc->debugfs_dir);
		pdev_rsrc->debugfs_dir = NULL;
		return -1;
	}

	err = sysfs_create_file(&pdev->dev.kobj, &dev_attr_gpu_info.attr);

	snprintf(temp_str, sizeof(temp_str), "%s%d", "syspll", pdev_rsrc->pcie_func_idx);
	pdev_rsrc->syspll_debugfs = fh2m_inno_debugfs_or_procfs_create_file(pdev_rsrc->dev, temp_str, 0644, pdev_rsrc->debugfs_hwdir, &inno_pll_fops);
	/*add for stress test, use emulate temp in shell*/
	snprintf(temp_str, sizeof(temp_str), "%s%d", "test_temp", pdev_rsrc->pcie_func_idx);
	pdev_rsrc->test_temp_debugfs = fh2m_inno_debugfs_or_procfs_create_file(pdev_rsrc->dev, temp_str, 0644, pdev_rsrc->debugfs_hwdir, &inno_test_temp_fops);

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	hal_hwinfo_init(pdev_rsrc);

	dev_rsrc_reinit_nulldisp_by_hwinfo(pdev_rsrc);
#endif

	if (!fh2m_hal_get_nulldisplay()) {
		pcie_info(&pdev->dev, "remove efifb...");
		fh2m_inno_drm_fb_kick_off_efifb();
	}

	hal_powercustom_init(pdev_rsrc);

	err = innogpu_init_bars(pdev_rsrc);
	if (err) {
		return err;
	}

	err = hal_vram_init(&pdev_rsrc->vram_cfg, HAL_SYSTEM_WORK_MODE_HOST, HAL_GPU_WORK_MODE_MC1, gpu_core_num);
	if (err) {
		return err;
	}

	/* fix: the check of pcie, ddr, vbios dbg is moved to post function of hw_check_notice. the log will be notice:
	 * "conflicting memory types 4000000000-4020000000 write-combining<->uncached-minus" during excuting  dr_mtrr_setup
	 */

#if defined(SUPPORT_ION)
#define INNOGPU_DDR_BAR 2
	err = innogpu_ion_init(pdev_rsrc, INNOGPU_DDR_BAR);
	if (err) {
	      pcie_error(pdev_rsrc->dev, "Failed to initialise ION\n");
	      return err;
	} else {
		pcie_error(pdev_rsrc->dev,
			"device[%s] dma_map_ops success to initialize ION\n",
			dev_name(&pdev->dev));
	}
#endif
	// 设置mtrr开启缓存
#if !defined(CONFIG_LOONGARCH) && !defined(CONFIG_MIPS)
	/*fix:loongarch must not set MTRR!!!*/
	err = dr_mtrr_setup(pdev_rsrc);
	if (err) {
		pcie_error(pdev_rsrc->dev,
			"device[%s] dr_mtrr_setup failed\n", dev_name(&pdev->dev));

		return err;
	}
#endif

	fh2m_hal_init_mcufw_comm(pdev_rsrc);

	hal_bmc_init(pdev_rsrc);

	hal_efuse_init(pdev_rsrc);

	hw_check_notice(pdev_rsrc);

#if defined(SUPPORT_DMA_TRANSFER)
	err = fh2m_hal_dma_init(pdev_rsrc);
	if (err) {
		pcie_error(pdev_rsrc->dev,
			"device[%s] fh2m_hal_dma_init failed\n", dev_name(&pdev->dev));

		return err;
	}

	err = fh2m_hal_vram_dma_mem_pool_init(pdev_rsrc->dev);
	if (err) {
		pcie_error(pdev_rsrc->dev,
			"device[%s] fh2m_hal_vram_dma_mem_pool_init failed\n", dev_name(&pdev->dev));

		return err;
	}
#endif
	return err;
}

static void hal_interface_test(struct device* dev)
{
	uint64_t maxsize=0;

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_GPU);
	pcie_info(dev, "GPU maxsize: 0x%llx", maxsize);

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_DPU);
	pcie_info(dev, "DPU maxsize: 0x%llx", maxsize);

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_VPU);
	pcie_info(dev, "VPU maxsize: 0x%llx", maxsize);

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_DMA);
	pcie_info(dev, "DMA maxsize: 0x%llx", maxsize);

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_AUDIO);
	pcie_info(dev, "AUDIO maxsize: 0x%llx", maxsize);

	maxsize = fh2m_hal_get_maxsize_by_role(dev, HAL_VRAM_ROLE_APU);
	pcie_info(dev, "APU maxsize: 0x%llx", maxsize);

}

static int innogpu_drivers_register(void)
{

	int ret = 0;
#ifdef CONFIG_DRM_INNO_PMBUS
	ret = innopmbus_driver_register();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_POWER
	ret = innopower_driver_register();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_SMMU
	ret = innosmmu_driver_register();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_DMA
	ret = innodma_driver_register();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_SRVKM
	ret = innogpu_drm_init();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_VPU
	ret = innovpu_init();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_DEBUG
	ret = inno_gpu_info_init();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_DRM_INNO_SR
	ret = innosr_init();
	if (ret)
		return ret;
#endif

	return ret;
}

static void innogpu_drivers_unregister(void)
{
#ifdef CONFIG_DRM_INNO_SR
	innosr_exit();
#endif

#ifdef CONFIG_DRM_INNO_DEBUG
	inno_gpu_info_exit();
#endif

#ifdef CONFIG_DRM_INNO_VPU
	innovpu_exit();
#endif

#ifdef CONFIG_DRM_INNO_SRVKM
	innogpu_drm_exit();
#endif

#ifdef CONFIG_DRM_INNO_DMA
	innodma_driver_unregister();
#endif

#ifdef CONFIG_DRM_INNO_SMMU
	innosmmu_driver_unregister();
#endif

#ifdef CONFIG_DRM_INNO_POWER
	innopower_driver_unregister();
#endif

#ifdef CONFIG_DRM_INNO_PMBUS
	innopmbus_driver_unregister();
#endif
}

static int innogpu_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {

	struct dev_rsrc* pdev_rsrc;
	int err=0, idx=0;
	ktime_t time;

	time = ktime_get();

	pcie_info(&pdev->dev, "innogpu_pci_probe enter. dev:%s ", dev_name(&pdev->dev));
	pcie_notice(&pdev->dev, "vendor: %x, device: %x\n", id->vendor, id->device);

	fh2m_get_kallsyms_lookup_name_address();

	idx = ida_simple_get(&pcie_func_ida, 0, 0, GFP_KERNEL);
	if (idx < 0) {
		pcie_error(&pdev->dev, "idx(%d) < 0!", idx);
		return -EINVAL;
	}

	if ((idx+1) > card_cnt) {
		pcie_error(&pdev->dev, "idx(%d) exceeds card count(%d)!Exit!", (idx+1), card_cnt);
		return -EINVAL;
	}

	set_module_params(pdev);
	print_module_params(pdev);

	if(!devres_open_group(&pdev->dev, NULL, GFP_KERNEL)) {
		pcie_error(&pdev->dev, "devres_open_group failed");
		return -ENOMEM;
	}

	pdev_rsrc = inno_rsrc_devres_alloc(sizeof(struct dev_rsrc));
	if(!pdev_rsrc) {
		pcie_error(&pdev->dev, "devres_alloc failed!");
		err = -ENOMEM;
		goto err_release;
	}

	devres_add(&pdev->dev, pdev_rsrc);

	pdev_rsrc->pcie_func_idx = idx;

	hal_module_loadtime_init(&pdev->dev, ktime_to_us(time));

	pcie_info(&pdev->dev, "pdev:0x%px pdev_rsrc:0x%px %s pcie_func_idx:%d\n", pdev, pdev_rsrc, __func__, pdev_rsrc->pcie_func_idx);

	err = fh2m_innogpu_pci_enable(&pdev->dev);
	if(err) {
		pcie_error(&pdev->dev, "fh2m_innogpu_pci_enable failed %d", err);
		goto err_release;
	}

	pdev_rsrc->pdev = pdev;
	pdev_rsrc->dev = &pdev->dev;
	pdev_rsrc->chip_type = (id->driver_data)&INNO_ASIC_MASK;
	if(CHIP_TYPE_MAX == pdev_rsrc->chip_type) {
		pcie_warn(&pdev->dev, "Unknown chip type!!!");
	}
	else if(CHIP_INNO == pdev_rsrc->chip_type) {
		//compatible with current HW_PLATFORM macro
#if (defined __G1_SOC__)
		pdev_rsrc->chip_type = CHIP_G1_SOC;
#elif defined(__G0_SOC__)
		pdev_rsrc->chip_type = CHIP_G0_SOC;
#elif defined(__G0_PAL__)
		pdev_rsrc->chip_type = CHIP_G0_PAL;
#elif defined(__G0M_SOC__)
		pdev_rsrc->chip_type = CHIP_G0M_SOC;
#elif defined(__G3_SOC__)
		pdev_rsrc->chip_type = CHIP_G3_SOC;
#elif defined(__G3_PAL__)
		pdev_rsrc->chip_type = CHIP_G3_PAL;
#elif defined(__G3_NE__)
		pdev_rsrc->chip_type = CHIP_G3_NE;
#endif
	}
	pcie_info(&pdev->dev, "current chip type is %d", pdev_rsrc->chip_type);

	hal_chip_init(pdev_rsrc);

#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	err = bind_numa_config_init(pdev_rsrc);
	if(err) {
		pcie_error(&pdev->dev, "numa_cpus_bind_array or numa_cpus_offset_array alloc failed, disable bind cpu");
	}
#endif //END CONFIG_NUMA __INNO_CONTAINER__

	err = dev_rsrc_init(pdev_rsrc, pdev);
	if(err) {
		pcie_error(&pdev->dev, "dev_rsrc_init failed!");
		goto err_dev_cleanup;
	}

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	hal_mcufw_status_check(pdev_rsrc);
#endif

	err = hal_hw_init(pdev_rsrc);
	if(err) {
		pcie_error(&pdev->dev, "hw_init failed!");
		err = -EINVAL;
		goto err_dev_cleanup;
	}

	if(do_deep_test)
	{
		hal_deep_test(pdev_rsrc);
	}
	else
	{
		hal_quick_test(pdev_rsrc);
	}

	hal_interface_test(&pdev->dev);

	// 注册中断
	err = hal_pci_irq_setup(pdev_rsrc);
	if (err) {
		pcie_error(&pdev->dev, "hal_pci_irq_setup failed, ret = %d!", err);
		goto err_dev_cleanup;
	}

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	fh2m_hal_power_init(pdev_rsrc);
#endif

	hal_check_reg_accessiable(pdev_rsrc);

	pcie_info(&pdev->dev, "mem size:%dGB DP2VGA:%d HDMI2DVI:%d",
		fh2m_hal_get_memsize(&pdev->dev), fh2m_hal_getflag_dp2vga(&pdev->dev, 0), fh2m_hal_getflag_hdmi2dvi(&pdev->dev, 0));

#if !defined(__G3_NE__) && !defined(__G3_PAL__)
	err = hal_devices_register(pdev_rsrc);
	if(err) {
		pcie_error(&pdev->dev, "device register failed!");
		goto err_dev_cleanup;
	}
#endif

	if (pci_config_hook_enable) {
		fixup_pcie_pre(pdev_rsrc);
	}

	return err;

err_dev_cleanup:
	dev_rsrc_deinit(pdev);
	fh2m_innogpu_pci_disable(&pdev->dev);

err_release:
#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	bind_numa_config_deinit();
#endif //END CONFIG_NUMA __INNO_CONTAINER__
	devres_release_group(&pdev->dev, NULL);
	if(err) {
		pcie_error(&pdev->dev, "failed to load driver");
	}
	return err;

}

static void innogpu_pci_remove(struct pci_dev* pdev) {

	struct dev_rsrc* pdev_rsrc = fh2m_inno_rsrc_devres_find(&pdev->dev);

	if(!pdev_rsrc) {
		pcie_error(&pdev->dev, "[%s:%d] dev_rsrc is NULL\n", __func__, __LINE__);
		return;
	}

	hal_devices_unregister(pdev_rsrc);

	fh2m_hal_power_deinit(pdev_rsrc);

	hal_pci_irq_free(pdev_rsrc);

	hal_chip_hw_deinit(pdev_rsrc);

	dev_rsrc_deinit(pdev);

#if	defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
	bind_numa_config_deinit();
#endif //END CONFIG_NUMA __INNO_CONTAINER__

  	hal_chip_deinit(pdev_rsrc);

	fh2m_innogpu_pci_disable(&pdev->dev);

	hal_module_loadtime_unregister_all(pdev_rsrc->dev);

	ida_simple_remove(&pcie_func_ida, pdev_rsrc->pcie_func_idx);
	pcie_info(&pdev->dev, "innogpu_pci_remove");
}

static struct pci_device_id s_inno_pci_tbl[] = {
#if defined(DESENSITIZED) && (DESENSITIZED == 1)
#if defined(__G1_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_1, PCI_DEVICE_ID_G1_SOC_ALIAS_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1_SOC},
#elif defined(__G0_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_1, PCI_DEVICE_ID_G0_SOC_ALIAS_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0_SOC},
#elif defined(__G1P_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_1, PCI_DEVICE_ID_G1P_SOC_ALIAS_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1P_SOC},
#elif defined(__G0M_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_1, PCI_DEVICE_ID_G0M_SOC_ALIAS_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0M_SOC},
#endif
#elif defined(DESENSITIZED) && (DESENSITIZED == 2)
#if defined(__G0_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_2, PCI_DEVICE_ID_G0_SOC_ALIAS_2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0_SOC},
#elif defined(__G0M_SOC__)
	{PCI_VENDOR_ID_INNOSILICON_ALIAS_2, PCI_DEVICE_ID_G0M_SOC_ALIAS_2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0M_SOC},
#endif
#else
	{PCI_VENDOR_ID_VF, PCI_DEVICE_ID_VF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TYPE_MAX},
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_INNOSILICON, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_INNO},
#if defined(__G1_SOC__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1_SOC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1_SOC},
#elif defined(__G1_PAL__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1_PAL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1_PAL},
#elif defined(__G1_NE__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1_NE},
#elif defined(__G0_SOC__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0_SOC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0_SOC},
#elif defined(__G0_PAL__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0_PAL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0_PAL},
#elif defined(__G0_NE__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0_NE},
#elif defined(__G1P_SOC__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1P_SOC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1P_SOC},
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1P_PAL_VF, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1P_SOC},
#elif defined(__G1P_PAL__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1P_PAL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1P_PAL},
#elif defined(__G1P_NE__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G1P_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G1P_NE},
#elif defined(__G0M_SOC__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0M_SOC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0M_SOC},
#elif defined(__G0M_PAL__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0M_PAL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0M_PAL},
#elif defined(__G0M_NE__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G0M_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G0M_NE},
#elif defined(__G3_SOC__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G3_SOC, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G3_SOC},
#elif defined(__G3_PAL__)
	{PCI_VENDOR_ID_INNOSILICON, PCI_DEVICE_ID_G3_PAL, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G3_PAL},
#elif defined(__G3_NE__)
	{PCI_VENDOR_ID_G3_NE, PCI_DEVICE_ID_G3_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_G3_NE},
#endif
	{PCI_VENDOR_ID_NE, PCI_DEVICE_ID_NE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_TYPE_MAX},
#endif
	{},
};

static int innogpu_device_suspend(struct device *dev, bool suspend)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dev_rsrc* pdev_rsrc = NULL;
	chip_type_e chip_type;
	int unset_power_state = 0;

	pdev_rsrc = fh2m_inno_rsrc_devres_find(&pdev->dev);
	if (!pdev_rsrc) {
        pcie_error(&pdev->dev, "[innogpu_pci_drv][%s:%d] dev_rsrc is NULL\n", __func__, __LINE__);
        return -1;
    }

	/* dma channel should be released before pcie suspend */
	fh2m_hal_dma_suspend(pdev_rsrc);

	/*cancle and stop high temperature monitor work*/
	hal_power_sleep(pdev_rsrc);

	hal_check_reg_accessiable(pdev_rsrc);

	/*
	 * Note:
	 * neede use enable_irq/disable_irq
	 * the kernel Oops when use pci_disable_msi/pci_enable_msi at interrupt MSI mode
	 */
	disable_irq(pdev->irq);

	if (suspend) {
		hal_pdp_restore_default_cfg(dev);

		pci_save_state(pdev);
		pci_disable_device(pdev);

		/*
		 * Note:
		 * workaround that g0 card pcie link failed 0x2088 when s3 stability test on phytium d2000/8 and loongson
		 * 3a5000/7a1000 cpu platform. there is a hardware bug that g0 card's pcie power state changed from d0 to d3.
		 * */
		chip_type = fh2m_hal_get_chiptype(dev);
#if defined(CONFIG_ARM64) || defined(CONFIG_LOONGARCH) || defined(CONFIG_MIPS)
			unset_power_state = 1;
#endif

		if (!unset_power_state)
			pci_set_power_state(pdev, PCI_D3hot);
	}

	pcie_info(&pdev->dev, "suspend: %s:%d. save/disable pci and disable irq:%d\n", __func__, __LINE__, pdev->irq);

	innogpu_pci_dump_cfgspace_regs(pdev, acpi_debug_mask);

	return 0;
}

static int innogpu_device_resume(struct device *dev, bool resume, bool is_suspend)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dev_rsrc* pdev_rsrc = NULL;

	pdev_rsrc = fh2m_inno_rsrc_devres_find(&pdev->dev);
	if (!pdev_rsrc) {
        pcie_error(&pdev->dev, "[innogpu_pci_drv][%s:%d] dev_rsrc is NULL\n", __func__, __LINE__);
        return -1;
    }

	if (resume) {
		pci_set_power_state(pdev, PCI_D0);
		pci_restore_state(pdev);
		ret = pci_enable_device(pdev);
		if (ret) {
			pcie_error(&pdev->dev, "enable device fail\n");
		}

		if (pdev_rsrc->chip.has_resize && pdev_rsrc->chip.is_support_resize) {
			ret = innogpu_resize_resume(pdev_rsrc);
			if(ret) {
				pcie_error(&pdev->dev, "resize resume fail\n");
			}
		}
	}

	hal_pci_irq_resume(pdev_rsrc);

	/*
	 * Note:
	 * neede use enable_irq/disable_irq
	 * the kernel Oops when use pci_disable_msi/pci_enable_msi at interrupt MSI mode
	 */
	enable_irq(pdev->irq);

	pcie_info(&pdev->dev, "resume: %s:%d. restore/enable pci and enable irq:%d\n", __func__, __LINE__, pdev->irq);

	if (pdev_rsrc->chip.pm_resume)
		pdev_rsrc->chip.pm_resume(pdev_rsrc, is_suspend);
	else
		pcie_info(&pdev->dev, "pm_resume callback is NULL\n");

	/*restart high temperature monitor work*/
	hal_power_wakeup(pdev_rsrc);

	return 0;
}

static int innogpu_pmops_suspend(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_suspend(dev, true);
}

static int innogpu_pmops_resume(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_resume(dev, true, true);
}

static int innogpu_pmops_freeze(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_suspend(dev, false);
}

static int innogpu_pmops_thaw(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_resume(dev, true, false);
}

static int innogpu_pmops_poweroff(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_suspend(dev, true);
}

static int innogpu_pmops_restore(struct device *dev)
{
	pcie_info(dev, "%s:%d\n", __func__, __LINE__);
	return innogpu_device_resume(dev, true, false);
}

static const struct dev_pm_ops innogpu_pci_pm_ops = {
	.suspend = innogpu_pmops_suspend,
	.resume = innogpu_pmops_resume,
	.freeze = innogpu_pmops_freeze,
	.thaw = innogpu_pmops_thaw,
	.poweroff = innogpu_pmops_poweroff,
	.restore = innogpu_pmops_restore,
};

static void innogpu_pci_shutdown(struct pci_dev *pdev)
{
	innogpu_device_suspend(&pdev->dev, true);
}

static struct pci_driver s_inno_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= s_inno_pci_tbl,
	.probe		= innogpu_pci_probe,
	.remove		= innogpu_pci_remove,
	.shutdown	= innogpu_pci_shutdown,
	.driver     = {
		.name = "innogpu_pci_drv",
		.pm = &innogpu_pci_pm_ops,
	},
};

static int __init innogpu_pci_driver_init(void)
{
	int ret = 0;
	int i = 0;
	struct pci_dev *pdev = NULL;
#if defined(__INNO_DESKTOP__) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	uint64_t total_ram, free_ram;
	unsigned long *ptr_image_size;
#endif
	/* same platform device name may cause multi driver attempts to load,filter before pci_register_driver
	 * pci_get_device arg3 uses NULL instead of &pdev, as multi cards of the same model only the first card needs to be detected
	 * */
	for(i = 0; i < INNO_ARRAY_SIZE(s_inno_pci_tbl); i++) {
		pdev = pci_get_device(s_inno_pci_tbl[i].vendor, s_inno_pci_tbl[i].device, NULL);
		if (pdev)
			break;
	}

	if (!pdev)
		return -ENODEV;

	ret = pci_register_driver(&s_inno_pci_driver);
	if (ret < 0) {
		fh2m_inno_printk("register s_inno_pci_driver failed, error %d\n", ret);
		goto err_pci;
	}

	ret = innogpu_drivers_register();
	if (ret) {
		fh2m_inno_printk("innogpu_drivers_register failed, error %d\n", ret);
		goto err_drivers;
	}

#if defined(__INNO_DESKTOP__) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	fh2m_inno_get_os_ram_stats(&total_ram, &free_ram);
	if (inno_s4_imgsize_en) {
		ptr_image_size = (unsigned long *)kallsyms_lookup_name("image_size");
		if (ptr_image_size != NULL) {
			inno_s4_imgsize = *ptr_image_size;
			*ptr_image_size = total_ram / 5;
			inno_s4_imgsize_changed = 1;
		}
	}
#endif

#if defined(CONFIG_CPU_RK3588) || defined(CONFIG_ARCH_EMEISWORD)
	pcie_error(&pdev->dev,"fixup alignment init\n");
	fixup_alignment_init();
#endif

	if (pci_config_hook_enable) {
		fixup_pcie_init(&pdev->dev);
	}

	ret = fh2m_inno_register_kprobe_or_ftrace();
	if(ret) {
		pcie_error(&pdev->dev, "kprobe or ftrace register failed!");
		goto err_drivers;
	}
	return 0;

err_drivers:
	pci_unregister_driver(&s_inno_pci_driver);
err_pci:
	return ret;
}

static void __exit innogpu_pci_driver_exit(void)
{
#if defined(__INNO_DESKTOP__) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	unsigned long *ptr_image_size;

	if (inno_s4_imgsize_changed) {
		ptr_image_size = (unsigned long *)kallsyms_lookup_name("image_size");
		if (ptr_image_size != NULL) {
			*ptr_image_size = inno_s4_imgsize;
			inno_s4_imgsize_changed = 0;
		}
	}
#endif
	innogpu_drivers_unregister();
	pci_unregister_driver(&s_inno_pci_driver);

	fh2m_inno_unregister_kprobe_or_ftrace();

	fh2m_inno_printk("innogpu_pci_driver_exit done!\n");
}

module_init(innogpu_pci_driver_init);
module_exit(innogpu_pci_driver_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DEVICE_TABLE(pci, s_inno_pci_tbl);


