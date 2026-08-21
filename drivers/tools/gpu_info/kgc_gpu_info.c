#include <linux/pci.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/string_helpers.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>

#include "innogpu.h"
#include "hal_interface.h"
#include "kernel_compatibility.h"
#include "kgc_gpu_info.h"
#include "inno_pci.h"
#include "hal.h"
#include "inno_misc.h"

/* gpu plugin info define  */
#define GPU_PLUGIN_CLASS_NAME "gpu_plugin_class"
#define INNO_GPU_PLUGIN_DEVICE_NAME  "gpu_plugin"
#define KGC_CASE(ucmd, pfunc)		{ucmd, pfunc}
#define GPU_INFO_VALID_RET        (0)
static void gpu_info_get_vram_clock(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	unsigned int vram_clock, mem_type;

	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	vram_clock =  gpu_info.vram_clock;
	mem_type =  gpu_info.vram_type;

	if ((mem_type - 'A' + 1) == 3 || (mem_type - 'A' + 1) == 4 || (mem_type - 'A' + 1) == 6 || (mem_type - 'A' + 1) == 7)
	{
		switch (vram_clock) {
			case 1866:
				vram_clock = 200;
				break;
			case 3200:
				vram_clock = 400;
				break;
			case 3733:
				vram_clock = 466;
				break;
			case 4266:
				vram_clock = 525;
				break;
			case 5500:
				vram_clock = 700;
				break;
			case 6400:
				vram_clock = 800;
				break;
			case 7500:
				vram_clock = 950;
				break;
			case 8533:
				vram_clock = 1050;
				break;
			default:
				vram_clock = 0;
		}
	} else {
		vram_clock = vram_clock / 2;
	}
	copy_cnt = copy_to_user((char *)args, &vram_clock, sizeof(vram_clock));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

/* Mhz */
static void gpu_info_get_max_gpu_clock(void *chip_ctx, void *args)
{
	unsigned int max_gpu_clock;
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	if (pdev_rsrc->chip_type == CHIP_G0M_SOC) {
		max_gpu_clock = 1350;
	} else if (pdev_rsrc->chip_type == CHIP_G0_SOC) {
		max_gpu_clock = 1250;
	} else
		max_gpu_clock = 1000;

	copy_cnt = copy_to_user((char *)args, &max_gpu_clock, sizeof(max_gpu_clock));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_gpu_clock(void *chip_ctx, void *args)
{
	unsigned int gpu_clock = 0;
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	fh2m_hal_bmc_read32(pdev_rsrc->dev, REG_ENTITY0036, &gpu_clock);
	gpu_clock = ((gpu_clock >> 8) & 0xff) * 10;
	copy_cnt = copy_to_user((char *)args, &gpu_clock, sizeof(gpu_clock));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_pci_link_speed(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	unsigned int link_speed;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	int index = 0;
	float speed[] = {2.5, 5.0, 8.0, 16.0, 32.0};
	pci_read_config_dword(pdev_rsrc->pdev, 0x80, &link_speed);
	link_speed = (link_speed>>16) & 0xF;
	switch (link_speed) {
		case 1:
			index = 0;
			break;
		case 2:
			index = 1;
			break;
		case 3:
			index = 2;
			break;
		case 4:
			index = 3;
			break;
		case 5:
			index = 4;
			break;
		default:
			pr_err("%s link_speed:%d is invalid",__func__, link_speed);
	}
	copy_cnt = copy_to_user((char *)args, &speed[index], sizeof(float));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_pci_link_width(void *chip_ctx, void *args)
{
	unsigned int link_width;
	unsigned long copy_cnt = 0;
	int index = 0;
	unsigned int width[] = {1, 2, 4, 8, 12, 16, 32};
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	pci_read_config_dword(pdev_rsrc->pdev, 0x80, &link_width);
	link_width = (link_width>>20) & 0x3F;
	if (link_width & 1) {
		index = 0;
	} if (link_width & 2) {
		index = 1;
	} else if (link_width & 4) {
		index = 2;
	} else if (link_width & 8) {
		index = 3;
	} else if (link_width & 12) {
		index = 4;
	} else if (link_width & 16) {
		index = 5;
	} else if (link_width & 32) {
		index = 6;
	}

	copy_cnt = copy_to_user((char *)args, &width[index], sizeof(width[index]));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}

}

static void gpu_info_get_pci_max_link_speed(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	int index = 0;
	float speed[] = {2.5, 5.0, 8.0, 16.0, 32.0};
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);

	switch(gpu_info.link_speed) {
		case 1:
			index = 0;
			break;
		case 2:
			index = 1;
			break;
		case 3:
			index = 2;
			break;
		case 4:
			index = 3;
			break;
		case 5:
			index = 4;
			break;
		default:
			pr_err("%s link_speed:%d is invalid",__func__, gpu_info.link_speed);
	}
	copy_cnt = copy_to_user((char *)args, &speed[index], sizeof(float));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}

}

static void gpu_info_get_pci_max_link_width(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	copy_cnt = copy_to_user((char *)args, &gpu_info.link_width, sizeof(gpu_info.link_width));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_pci_busid(void *chip_ctx, void *args)
{
	char buf[32] = {0};
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	struct pci_dev *pdev = (struct pci_dev *)pdev_rsrc->pdev;
	fh2m_inno_sprintf(buf, 32, "%04x:%02x:%02x.%01x\0", pci_domain_nr(pdev->bus), pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
	copy_cnt = copy_to_user((char *)args, &buf, sizeof(buf));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_pci_info(void *chip_ctx, void *args)
{
	/*vendor_id device_Id subvendor_id subdevice_id revision*/
	unsigned int busid[5] = {0};
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	busid[0] = fh2m_inno_get_pci_vendor(pdev_rsrc->pdev);
	busid[1] = fh2m_inno_get_pci_device(pdev_rsrc->pdev);
	busid[2] = fh2m_inno_get_pci_subvendor(pdev_rsrc->pdev);
	busid[3] = fh2m_inno_get_pci_subdevice(pdev_rsrc->pdev);
	busid[4] = 0;
	copy_cnt = copy_to_user((char *)args, &busid, sizeof(busid));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_manufacturer(void *chip_ctx, void *args)
{
	unsigned int busid = 0;
	char buf[32] = {0};
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	busid = fh2m_inno_get_pci_subvendor(pdev_rsrc->pdev);
	fh2m_inno_sprintf(buf, 32, "%x\0", busid);
	copy_cnt = copy_to_user((char *)args, buf, sizeof(buf));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_gpu_temp(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	unsigned int gpu_temp = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	fh2m_hal_bmc_read32(pdev_rsrc->dev, REG_ENTITY0039, &gpu_temp);
	gpu_temp = (gpu_temp >> 8) & 0xff;
	copy_cnt = copy_to_user(args, &gpu_temp, sizeof(gpu_temp));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld gpu_temp:%d \n", __func__, copy_cnt, gpu_temp);
	}
}

static void gpu_info_get_fan_speed(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	unsigned char fan_speed;

	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	fan_speed = gpu_info.fan_speed;
	copy_cnt = copy_to_user((char *)args, &fan_speed, 1);
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_vram_type(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	char* mem_types[] = {"unknown", "DDR3", "DDR4", "LPDDR4", "LPDDR4X", "DDR5", "LPDDR5", "GDDR5", "GDDR6", "GDDR6X" "LPDDR5X" };
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	if (gpu_info.vram_type == 0 || (gpu_info.vram_type - 'A' + 1) >= ARRAY_SIZE(mem_types)) {
		copy_cnt = copy_to_user((char *)args, mem_types[0], strlen(mem_types[0]) + 1);
		if (copy_cnt != GPU_INFO_VALID_RET) {
			pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
		}
	} else {
		copy_cnt = copy_to_user((char *)args, mem_types[gpu_info.vram_type - 'A' + 1], strlen(mem_types[gpu_info.vram_type - 'A' + 1]) + 1);
		if (copy_cnt != GPU_INFO_VALID_RET) {
			pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
		}
	}
}

static void gpu_info_get_vram_size(void *chip_ctx, void *args)
{
	unsigned int total;
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	total = gpu_info.mem_width * gpu_info.mem_nums * 1024;
	copy_cnt = copy_to_user((char *)args, &total, sizeof(total));
	if (copy_cnt != 0) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_vram_used(void *chip_ctx, void *args)
{
	unsigned int used;
	unsigned long copy_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	struct vram_stats gpu_stat;
	fh2m_hal_get_gpu_stat(pdev_rsrc->dev, 0, &gpu_stat);
	used = (gpu_stat.total_size - gpu_stat.free_size) >> 20;
	copy_cnt = copy_to_user((char *)args, &used, sizeof(used));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_vbios_version(void *chip_ctx, void *args)
{
	char buf[32] = {0};
	unsigned long copy_cnt = 0;
	fh2m_hal_get_mcufw_version(chip_ctx, buf, sizeof(buf));
	copy_cnt = copy_to_user((char *)args, buf, sizeof(buf));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_gpu_vddc(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	copy_cnt = copy_to_user(args, &gpu_info.gpu_voltage, sizeof(gpu_info.gpu_voltage));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}

static void gpu_info_get_gpu_power(void *chip_ctx, void *args)
{
	unsigned long copy_cnt = 0;
	struct gpu_info_hwinfo gpu_info;
	fh2m_hal_get_gpu_info_hwinfo(chip_ctx, &gpu_info);
	copy_cnt = copy_to_user(args, &gpu_info.gpu_power, sizeof(gpu_info.gpu_power));
	if (copy_cnt != GPU_INFO_VALID_RET) {
		pr_err("%s copy_cnt: %ld \n", __func__, copy_cnt);
	}
}


static struct kgc_item kgc_info[] = {
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VENDOR_NAME),     NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_CARD_NAME),       NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_NAME),        NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_CLOCK),       gpu_info_get_gpu_clock),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VRAM_CLOCK),      gpu_info_get_vram_clock),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MAX_GPU_CLOCK),   gpu_info_get_max_gpu_clock),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MAX_VRAM_CLOCK),	gpu_info_get_vram_clock),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_PCI_SPEED),       gpu_info_get_pci_link_speed),

	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_PCI_WIDTH),       gpu_info_get_pci_link_width),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MAX_LINK_SPEED),	gpu_info_get_pci_max_link_speed),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MAX_LINK_WIDTH),  gpu_info_get_pci_max_link_width),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_PCI_BUS_ID),      gpu_info_get_pci_busid),

	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_PCI_INFO),        gpu_info_get_pci_info),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_TEMP),        gpu_info_get_gpu_temp),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_FAN_SPEED),       gpu_info_get_fan_speed),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VRAM_TYPE),       gpu_info_get_vram_type),

	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VRAM_SIZE),       gpu_info_get_vram_size),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VRAM_LOAD),       NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VRAM_USED),       gpu_info_get_vram_used),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_LOAD),        NULL),

	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VIDEO_LOAD),      NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_VBIOS_VERSION),   gpu_info_get_vbios_version),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MANUFACTURER),    gpu_info_get_manufacturer),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_SHADER_NUMBER),   NULL),

	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_VDDC),        gpu_info_get_gpu_vddc),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_GPU_POWER),       gpu_info_get_gpu_power),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_MAX_GPU_POWER),   NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_POWER_MODE),	    NULL),
	KGC_CASE(INNOGPU_GPU_INFO_IOCTL_GET(GPU_BASE_INFO_POWER_MODE_LIST), NULL),
};

static struct plugin_miscdev *plugin_get_dev(struct file *filp)
{
	struct miscdevice *miscdev = (struct miscdevice *)filp->private_data;
	struct plugin_miscdev *plugin_dev =  container_of(miscdev, struct plugin_miscdev, misc_dev);
	return plugin_dev;
}

static int gpu_plugin_open (struct inode *inode, struct file *filp)
{
	return 0;
}

static int gpu_plugin_close (struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t gpu_plugin_read (struct file *filp, char __user *buff, size_t len, loff_t *ppos)
{
	return 0;
}

 static ssize_t gpu_plugin_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	return 0;
}

static long gpu_plugin_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int i = 0;
	struct plugin_miscdev *plugin_dev =  plugin_get_dev(filp);
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(plugin_dev->pdev);


	for (i = 0; i < ARRAY_SIZE(kgc_info); i++){
		if (cmd == kgc_info[i].cmd && NULL != kgc_info[i].func) {
			 kgc_info[i].func(pdev_rsrc, (void *)arg);
		}
	}

	return 0;
}


static struct file_operations gpu_plugin_fops = {
	.owner = THIS_MODULE,
	.open = gpu_plugin_open,
	.read = gpu_plugin_read,
	.write = gpu_plugin_write,
	.unlocked_ioctl = gpu_plugin_ioctl,
	.release = gpu_plugin_close,
};

int sys_gpu_plugin_init(inno_dev *pdev)
{
	int ret = 0;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(pdev);
	struct plugin_miscdev *plugin_dev = NULL;
	char *name = fh2m_inno_kmalloc_kernel(sizeof(char) * 64);
	if (!name) {
		pr_err("[%s]misc device name alloc fail!", __func__);
		return -ENOMEM;
	}
	fh2m_inno_sprintf(name, 64, "%s_%d", INNO_GPU_PLUGIN_DEVICE_NAME, pdev_rsrc->pcie_func_idx);
	plugin_dev = kzalloc(sizeof(struct plugin_miscdev), GFP_KERNEL);
	plugin_dev->misc_dev.fops = &gpu_plugin_fops;
	plugin_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	plugin_dev->misc_dev.name = name;
	plugin_dev->pdev = pdev;
	plugin_dev->open_flag = 99;
	pdev_rsrc->gpu_info_plugin = plugin_dev;

	ret = misc_register(&plugin_dev->misc_dev);
	if (ret < 0) {
		pr_err("misc_register failed ,ret = %d\n ", ret);
		fh2m_inno_kfree(name);
		name = NULL;
		return ret;
	}
	dev_set_drvdata(pdev, plugin_dev);
	return ret;
}

void sys_gpu_plugin_exit(inno_dev *pdev)
{
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(pdev);
	struct plugin_miscdev *plugin_dev = (struct plugin_miscdev *)pdev_rsrc->gpu_info_plugin;
	if (plugin_dev == NULL) {
		pr_err("%s plugin_dev is NULL \n ", __func__);
	}
	misc_deregister(&plugin_dev->misc_dev);
	if (plugin_dev->misc_dev.name) {
		fh2m_inno_kfree(plugin_dev->misc_dev.name);
		plugin_dev->misc_dev.name = NULL;
	}
	if (plugin_dev) {
		kfree(plugin_dev);
		plugin_dev = NULL;
	}

}
