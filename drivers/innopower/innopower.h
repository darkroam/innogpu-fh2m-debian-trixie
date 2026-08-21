#ifndef INNO_POWER_H
#define INNO_POWER_H

#include "hal.h"
#include <linux/version.h>

#define INNO_POWER_ERR -1
#define INNO_POWER_OK	0

#define POWER_CHIP_NAME	20

#define INNOPWR "innopwr"
#define pr_power(fmt) "[%s][%s:%d]" fmt,INNOPWR,__func__,__LINE__

#define innopwr_dbg(fmt, ...)\
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_DBG) \
		printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)
#define innopwr_info(fmt, ...)\
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) \
		printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)
#define innopwr_notice( fmt, ...) \
		printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)
#define innopwr_warn( fmt, ...) \
		printk(KERN_WARNING pr_power(fmt), ##__VA_ARGS__)
#define innopwr_err(fmt, ...)\
		printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)

struct powerchip {
	char name[POWER_CHIP_NAME];

	struct i2c_client *client;
	int (*get_gpu_voltage)(struct powerchip *pwrchip, int channel);
	int (*set_gpu_voltage)(struct powerchip *pwrchip, int channel, unsigned vol);

	struct mutex lock;
	void *priv_data;
};

struct innopower {
	struct volctrl_chip_hwinfo_t *params;
	struct i2c_adapter *adapter;

	struct device *pdev;	//pci_dev->dev
	struct device *pltdev;

	struct powerchip *chip;

	/*about gpu test*/
	int idx;

	inno_dentry* dfs_dir_pwrdir;
	inno_dentry* dfs_node_gpu_bootvol;
	inno_dentry* dfs_node_gtest_gpupll;

	int boot_vol_real;
	int boot_vol_expect;
	uint32_t gpupll;
	uint32_t gpuvol;

	unsigned int pcie_speed_max_cap;
	int pcie_speed;
	struct mutex drop_speed_mtx;
};

int innopwr_read8(struct powerchip *pwrchip, unsigned char reg);
int innopwr_write8(struct powerchip *pwrchip, unsigned char reg, unsigned char data);
int innopwr_read16(struct powerchip *pwrchip, unsigned char reg);
int innopwr_write16(struct powerchip *pwrchip, unsigned char reg, unsigned int data);

int innopwr_read8_no_repeat_start(struct powerchip *pwrchip, unsigned char reg);
int innopwr_write8_no_repeat_start(struct powerchip *pwrchip, unsigned char reg, unsigned char data);
int innopwr_read16_no_repeat_start(struct powerchip *pwrchip, unsigned char reg);
int innopwr_write16_no_repeat_start(struct powerchip *pwrchip, unsigned char reg, unsigned int data);

int innopower_driver_register(void);
void innopower_driver_unregister(void);
#include "power_hw_info.h"
#endif	/*INNO_POWER_H*/
