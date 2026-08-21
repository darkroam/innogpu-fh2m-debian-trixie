/*
	innopmbus_drv.c: SMBus (i2c) adapter for inno pmbus

	Copyright (C) Innosilicon Technology Ltd. All Rights Reserved
				Derived from CHIPS&MEDIA DDK
	COPYRIGHT (C) 2020 CHIPS&MEDIA INC. ALL RIGHTS RESERVED

	This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
	SPDX License Identifier: BSD-3-Clause
	SPDX License Identifier: LGPL-2.1-only
*/

#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include "inno_debug.h"

#define PMBUS_CFG                 0x00
#define PMBUS_SLV_CTRL            0x04
#define PMBUS_CMD                 0x08
#define PMBUS_ERR_STA             0x0C
#define PMBUS_STA                 0x10
#define PMBUS_WR_FIFO             0x14
#define PMBUS_RD_FIFO             0x18
#define PMBUS_PEC                 0x1c
#define PMBUS_FIFO_CTRL           0x20
#define PMBUS_INTR_MASK           0x24
#define PMBUS_INTR_CLR            0x28
#define PMBUS_INTR                0x2c

#define INNO_PMBUS_FIFO_MAX       64
#define INNO_PMBUS_WRITE          0
#define INNO_PMBUS_READ           1
#define INNO_PMBUS_STOP           0
#define INNO_PMBUS_START          1

#define PMBUS_ERR_RD_FIFO				0
#define PMBUS_ERR_WR_FIFO				1
#define PMBUS_ERR_SLAVE_NOACK 			2
#define PMBUS_ERR_TIMEOUT_CLOCKSTRETCH 	3
#define PMBUS_ERR_TIMEOUT_XFR 			4
#define PMBUS_ERR_TIMEOUT_START 		5
#define PMBUS_ERR_WHERE					6
#define PMBUS_ERR_PEC					14

#define DDC_SEGMENT_ADDR 0x30
#define DDC_CI_ADDR      0x37
#define DDC_EDID_ADDR    0x50

#define INNO_PMBUS_TIMEOUT        (msecs_to_jiffies(1000))
#define DATA_SET(data, val, pos)  ((data) |= ((val) << (((pos++) % 4) * 8)))
#define DATA_GET(data, buf, pos)  (buf[(pos)] = (((data) >> (((pos) % 4) * 8)) & 0xFF))
#define DATA_FULL(pos)            ((pos) % 4 == 0)
#define DATA_CLR(data)            ((data) = 0)

#define KBUILD_PMBUS "pmbus"
#define pr_fmt_pmbus(fmt) "[%s][%s:%d]" fmt, KBUILD_PMBUS, __func__, __LINE__
#define pmbus_err(dev, fmt, ...) \
		fh2m_inno_dev_printk(KERN_ERR, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define pmbus_notice(dev, fmt, ...) \
		fh2m_inno_dev_printk(KERN_NOTICE, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)

#if defined(INNO_GPU_LOG)
#define pmbus_info(dev,fmt, ...) \
		fh2m_inno_dev_printk(KERN_INFO, dev,pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#else
#define pmbus_info(dev,fmt, ...)
#endif

#if defined(DEBUG)
#define pmbus_fifo_dbg(dev, reg, val) \
	do { \
		if ((reg) == PMBUS_WR_FIFO) \
			pmbus_info(dev, "WRITE FIFO: 0x%x\n", val); \
		else if ((reg) == PMBUS_RD_FIFO) \
			pmbus_info(dev, "READ  FIFO: 0x%x\n", val); \
	} while (0)
#else
#define pmbus_fifo_dbg(dev, reg, val)
#endif

static void pmbus_writel(struct inno_pmbus_dev *pmbus_dev, u32 offset, u32 val)
{
	pmbus_fifo_dbg(pmbus_dev->dev, offset, val);
	writel(val, pmbus_dev->base + offset);
}

static u32 pmbus_readl(struct inno_pmbus_dev *pmbus_dev, u32 offset)
{
	u32 val = readl(pmbus_dev->base + offset);
	pmbus_fifo_dbg(pmbus_dev->dev, offset, val);
	return val;
}

static void pmbus_init(struct inno_pmbus_dev *dev, u32 baudrate)
{

	dev->enable = true;
	pmbus_writel(dev, PMBUS_CFG, pmbus_readl(dev, PMBUS_CFG) & ~BIT(0));
	pmbus_writel(dev, PMBUS_CFG, ((0xffff << 16) | (baudrate << 2) | 0x01));
	pmbus_writel(dev, PMBUS_FIFO_CTRL, (0x03));
	pmbus_writel(dev, PMBUS_INTR_MASK, (0x07));
}

static void set_pmbus_freq(inno_i2c_adapter *adap, uint32_t baudrate)
{
	struct inno_pmbus_dev *i2c_dev = i2c_get_adapdata((struct i2c_adapter *)adap);

	pmbus_init(i2c_dev, baudrate);
}

enum pmbus_idle_m {
	pmbus_m_idle = 0,
	pmbus_mb_idle = 1,
	pmbus_wmb_idle = 2,
};

static bool pmbus_idle(struct inno_pmbus_dev *pmbus_dev, enum pmbus_idle_m idle)
{
	u32 rdata = 0;
	bool ret = false;

#define PMBUS_MACHINE_IDLE(val) (val & BIT(15))
#define PMBUS_BUS_IDLE(val)     (!(val & BIT(0)))
#define PMBUS_WRITE_ENTRY(val)  ((val >> 6) & 0xff)

	rdata = pmbus_readl(pmbus_dev, PMBUS_STA);

	switch (idle) {
	case pmbus_m_idle:
		ret = PMBUS_MACHINE_IDLE(rdata);
		break;
	case pmbus_mb_idle:
		ret = (PMBUS_MACHINE_IDLE(rdata) && PMBUS_BUS_IDLE(rdata));
		break;
	case pmbus_wmb_idle:
		ret = (PMBUS_MACHINE_IDLE(rdata) && PMBUS_BUS_IDLE(rdata) && !PMBUS_WRITE_ENTRY(rdata));
		break;
	default:
		pmbus_err(pmbus_dev->dev, "Invalid argument\n");
		break;
	}

#undef PMBUS_MACHINE_IDLE
#undef PMBUS_BUS_IDLE
#undef PMBUS_WRITE_ENTRY

	return ret;
}

static int pmbus_wait_idle(struct inno_pmbus_dev *pmbus_dev, enum pmbus_idle_m idle)
{
	unsigned long timeout = jiffies + INNO_PMBUS_TIMEOUT;

	smp_wmb();

	while (1) {

		udelay(2);

		if (pmbus_idle(pmbus_dev, idle))
			return 0;

		if (time_after(jiffies, timeout)) {
			return -EFAULT;
		}
	}

	if (pmbus_dev->idle_timeout_count++ <= 10)
		pmbus_notice(pmbus_dev->dev, "pmbus wait idle timeout\n");

	return -EFAULT;
}

static void pmbus_reinit(struct inno_pmbus_dev *pmbus_dev)
{
	u32 rdata = pmbus_readl(pmbus_dev, PMBUS_CFG);
	pmbus_writel(pmbus_dev, PMBUS_FIFO_CTRL, 0x1);
	pmbus_writel(pmbus_dev, PMBUS_CFG, rdata & 0xFFFFFFFC);
	pmbus_writel(pmbus_dev, PMBUS_CFG, rdata | 0x1);
}

static int pmbus_check_status(struct inno_pmbus_dev *pmbus_dev, u8 dev_addr)
{
	u32 rdata = pmbus_readl(pmbus_dev, PMBUS_ERR_STA);
	const char * const msg_err[] = {
		"read firo was full",
		"write fifo was empty",
		"slave no ack",
		"timeout during clock stretch",
		"timeout during transfer",
		"timeout during start",
	};
	unsigned long bitval = rdata;
	int ret = 0, fault = 0;

	if (rdata != 0) {
		ret = -EFAULT;
		for_each_set_bit(fault, &bitval, ARRAY_SIZE(msg_err)) {
			if (fault != PMBUS_ERR_SLAVE_NOACK) {
				pmbus_reinit(pmbus_dev);
				if (pmbus_dev->checkerr_count++ <= 10)
					pmbus_notice(pmbus_dev->dev, "[PMBUS]%s %#.8x\n", msg_err[fault], rdata);
			}
			if (fault == PMBUS_ERR_TIMEOUT_START ||
				fault == PMBUS_ERR_TIMEOUT_XFR ||
				fault == PMBUS_ERR_TIMEOUT_CLOCKSTRETCH)
				ret = -ETIMEDOUT;
		}
	}

	return ret;
}

static void pmbus_cmd_exec(struct inno_pmbus_dev *dev, u8 dev_addr, u8 rw,
	u8 repeat, u8 len)
{
	u32 wdata = (1u << 31) | (dev_addr << 17) | (rw << 16) | (repeat << 9) | len;
	pmbus_writel(dev, PMBUS_CMD, wdata);
}

static void pmbus_clr_fifo(struct inno_pmbus_dev *dev, u8 rw)
{
	if (rw == INNO_PMBUS_READ) {
		pmbus_writel(dev, PMBUS_FIFO_CTRL, 0x2);
	} else {
		pmbus_writel(dev, PMBUS_FIFO_CTRL, 0x1);
	}
}

/* 判断rd fifo是否为空，如果为空不能读，读了会卡死 */
static bool pmbus_is_rd_fifo_empty(struct inno_pmbus_dev *dev)
{
	u32 data = pmbus_readl(dev, PMBUS_STA);
	return ((data >> 16) & 0xFF) == 0;
}

static int inno_pmbus_write(struct inno_pmbus_dev *dev, u8 dev_addr, u8 cmd,
	u8 *buf, u16 len, u8 repeat)
{
	int ret;
	u32 data = 0;
	u16 pos = 0, i = 0;
	enum pmbus_idle_m wait_idle = (repeat == INNO_PMBUS_START) ? pmbus_m_idle : pmbus_mb_idle;

	if (len > INNO_PMBUS_FIFO_MAX) {
		pmbus_notice(dev->dev, "invalid param");
		return -EINVAL;
	}

	if ((ret = pmbus_wait_idle(dev, pmbus_mb_idle)) != 0)
		return ret;

	DATA_SET(data, cmd, pos);
	for (i = 0; i < len; i++) {
		DATA_SET(data, buf[i], pos);
		if (DATA_FULL(pos)) {
			pmbus_writel(dev, PMBUS_WR_FIFO, data);
			DATA_CLR(data);
		}
	}
	if (!DATA_FULL(pos))
		pmbus_writel(dev, PMBUS_WR_FIFO, data); /* write left */
	pmbus_cmd_exec(dev, dev_addr, INNO_PMBUS_WRITE, repeat, 2 + len);

	/* Make sure pcie order normal write read > write cmd first */
	udelay(10);

	if ((ret = pmbus_wait_idle(dev, wait_idle)) != 0)
		return ret;

	if ((ret = pmbus_check_status(dev, dev_addr)) != 0)
		return ret;

	pmbus_clr_fifo(dev, INNO_PMBUS_WRITE);

	return ret;
}

static int inno_pmbus_write_byte_data(struct inno_pmbus_dev *dev, u8 dev_addr,
	u8 cmd, u8 value)
{
	return inno_pmbus_write(dev, dev_addr, cmd, &value, 1, INNO_PMBUS_STOP);
}

static int inno_pmbus_write_word_data(struct inno_pmbus_dev *dev, u8 dev_addr,
	u8 cmd, u16 value)
{
	return inno_pmbus_write(dev, dev_addr, cmd, (u8 *)(&value), 2, INNO_PMBUS_STOP);
}

static int inno_pmbus_read_byte(struct inno_pmbus_dev *dev, u8 dev_addr, u8 *val)
{
	int ret = 0;
	u32 wdata = 0;

	if ((ret = pmbus_wait_idle(dev, pmbus_mb_idle)) != 0)
		return ret;

	pmbus_cmd_exec(dev, dev_addr, INNO_PMBUS_READ, INNO_PMBUS_STOP, 2);

	if ((ret = pmbus_wait_idle(dev, pmbus_mb_idle) != 0))
		return ret;

	if ((ret = pmbus_check_status(dev, dev_addr)) != 0)
		return ret;

	if (pmbus_is_rd_fifo_empty(dev)) {
		dev_err(dev->dev, "rd fail, no data\n");
		return -ENODEV;
	}

	wdata = pmbus_readl(dev, PMBUS_RD_FIFO);
	*val = (u8)wdata;
	pmbus_clr_fifo(dev, INNO_PMBUS_READ);

	return ret;
}

/*special voltctrl chip i2c addr of 7bit from hwinfo*/
#define XDPE12284C_ADDR_1 (0x76) /*hwinfo_v6_G0_G0M: 0xEC >> 1*/
#define XDPE12284C_ADDR_2 (0x72) /*hwinfo_v6_G1&G1P*/
#define XDPE12284C_ADDR_3 (0x6A) /*hwinfo_v6_G1&G1P*/
static int inno_pmbus_read(struct inno_pmbus_dev *dev, u8 dev_addr, u8 cmd,
	u8 *buf, u16 len)
{
	u16 i = 0;
	int ret = 0;
	u32 data = 0;

	if (buf == NULL || len == 0 || len > INNO_PMBUS_FIFO_MAX) {
		pmbus_notice(dev->dev, "invalid param");
		return -EINVAL;
	}

	/*  1、start+dev_addr(w)+reg_addr+stop;
		2、start+dev_addr(r)+val+stop;
	*/
	if (dev_addr != DDC_CI_ADDR) {
		if (!dev->chip_need_rs)
			ret = inno_pmbus_write(dev, dev_addr, cmd, NULL, 0, 0);
		else
			ret = inno_pmbus_write(dev, dev_addr, cmd, NULL, 0, INNO_PMBUS_START);

		if (ret)
			return ret;
	}

	pmbus_cmd_exec(dev, dev_addr, INNO_PMBUS_READ, INNO_PMBUS_STOP, 1 + len);

	/* Make sure pcie order normal write read > write cmd first */
	udelay(10);

	if ((ret = pmbus_wait_idle(dev, pmbus_mb_idle)) != 0)
		return ret;

	if ((ret = pmbus_check_status(dev, dev_addr) != 0))
		return ret;

	if (pmbus_is_rd_fifo_empty(dev))
		return -EFAULT;

	for (i = 0; i < len; i++) {
		if (DATA_FULL(i))
			data = pmbus_readl(dev, PMBUS_RD_FIFO);
		DATA_GET(data, buf, i);
	}

	pmbus_clr_fifo(dev, INNO_PMBUS_READ);

	return ret;
}

static int inno_pmbus_read_byte_data(struct inno_pmbus_dev *dev, u8 dev_addr,
	u8 cmd, u8 *data)
{
	return inno_pmbus_read(dev, dev_addr, cmd, data, 1);
}

static int inno_pmbus_read_word_data(struct inno_pmbus_dev *dev, u8 dev_addr,
	u8 cmd, u16 *data)
{
	return inno_pmbus_read(dev, dev_addr, cmd, (u8 *)data, 2);
}

static int inno_pmbus_xfer(struct i2c_adapter *adap, u16 addr,
	unsigned short flags, char read_write, u8 command, int size,
	union i2c_smbus_data *data)
{
	int ret = -EOPNOTSUPP;
	struct inno_pmbus_dev *i2c_dev = i2c_get_adapdata(adap);

	if (!i2c_dev->enable) {
		pmbus_info(i2c_dev->dev, "Access denied by pmbus\n");
		return ret;
	}

	switch (size) {
	case I2C_SMBUS_BYTE:
		if (read_write == I2C_SMBUS_READ)
			ret = inno_pmbus_read_byte(i2c_dev, addr, &data->byte);
		else
			ret = inno_pmbus_write(i2c_dev, addr, command, NULL, 0, INNO_PMBUS_STOP);
		break;
	case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_READ)
			ret = inno_pmbus_read_byte_data(i2c_dev, addr, command, &data->byte);
		else
			ret = inno_pmbus_write_byte_data(i2c_dev, addr, command, data->byte);
		break;
	case I2C_SMBUS_WORD_DATA:
		if (read_write == I2C_SMBUS_READ)
			ret = inno_pmbus_read_word_data(i2c_dev, addr, command, &data->word);
		else
			ret = inno_pmbus_write_word_data(i2c_dev, addr, command, data->word);
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (read_write == I2C_SMBUS_READ) {
			if (data->block[0] == 0)
				data->block[0] = I2C_SMBUS_BLOCK_MAX;
			ret = inno_pmbus_read(i2c_dev, addr, command, &data->block[1], data->block[0]);
		} else
			ret = inno_pmbus_write(i2c_dev, addr, command, &data->block[1], data->block[0], INNO_PMBUS_STOP);
		break;
	default:
		break;
	}
	return ret;
}

static u32 inno_pmbus_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_BYTE      |
	       I2C_FUNC_SMBUS_BYTE_DATA |
	       I2C_FUNC_SMBUS_WORD_DATA |
	       I2C_SMBUS_I2C_BLOCK_DATA;
};

static const struct i2c_algorithm inno_pmbus_algorithm = {
	.smbus_xfer = inno_pmbus_xfer,
	.functionality = inno_pmbus_func,
};

static int inno_pmbus_probe(struct platform_device *pdev)
{
	int err;
	struct resource *res;
	struct i2c_adapter *adap;
	struct inno_pmbus_dev *i2c_dev;
	struct device *dev = &pdev->dev;
	plat_data_t *pdata = dev_get_platdata(dev);

	i2c_dev = devm_kzalloc(dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmbus-regs");
	if (res == NULL) {
		dev_err(dev, "get res fail\n");
		return PTR_ERR(res);
	}

	i2c_dev->dev = dev;
	adap = &i2c_dev->adap;
#ifdef INNOGPU_IOREMAP_NOCACHE_PRESENT
	i2c_dev->base = ioremap_nocache(res->start, resource_size(res));
#else
	i2c_dev->base = ioremap(res->start, resource_size(res));
#endif
	i2c_dev->pmbus_funcs.set_pmbus_freq = set_pmbus_freq;
	i2c_set_adapdata(adap, i2c_dev);
	snprintf(adap->name, sizeof(adap->name), "Inno pmbus%d", pdata->dev_idx);
	adap->owner = THIS_MODULE;
	adap->algo = &inno_pmbus_algorithm;
	adap->dev.parent = dev;

	i2c_dev->chip_need_rs = 1;
	mutex_init(&i2c_dev->chip_rs_lock);
	err = i2c_add_adapter(adap);
	if (err)
		return err;

	fh2m_hal_set_pmbus_adapter_and_funcs(pdev->dev.parent, adap, &i2c_dev->pmbus_funcs, pdata->dev_idx);
	fh2m_hal_pmbus_nr_set(pdev->dev.parent, pdata->dev_idx, adap->nr);

	platform_set_drvdata(pdev, i2c_dev);
	return 0;
}

static int inno_pmbus_remove(struct platform_device *pdev)
{
	struct inno_pmbus_dev *i2c_dev = platform_get_drvdata(pdev);
	if (i2c_dev){
		iounmap(i2c_dev->base);
		i2c_del_adapter(&i2c_dev->adap);
		mutex_destroy(&i2c_dev->chip_rs_lock);
	}

	fh2m_hal_pmbus_nr_set(pdev->dev.parent, pdev->id, -1);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int inno_pmbus_suspend(struct device *dev)
{
	return 0;
}

static int inno_pmbus_resume(struct device *dev)
{
	struct inno_pmbus_dev *i2c_dev = dev_get_drvdata(dev);

	/* when the system first wakes up, the pmbus is not initialised and cannot be accessed
	 * you need to wait for the module using the pmbus to initialise the pmbus before accessing it
	 * */
	if (i2c_dev)
		i2c_dev->enable = false;

	return 0;
}
#endif

static struct platform_device_id inno_pmbus_device_id_table[] = {
	{ .name = INNO_PMBUS_DEVICE_NAME, .driver_data = 0 },
	{},
};

static const struct dev_pm_ops inno_pmbus_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(inno_pmbus_suspend, inno_pmbus_resume)
};

struct platform_driver inno_pmbus_driver = {
	.probe = inno_pmbus_probe,
	.remove = inno_pmbus_remove,
	.driver = {
		.name = INNO_PMBUS_DEVICE_NAME,
		.pm = &inno_pmbus_pm_ops,
	},
	.id_table = inno_pmbus_device_id_table,
};

#ifdef CONFIG_DRM_INNO_PMBUS
int innopmbus_driver_register(void)
{
	return platform_driver_register(&inno_pmbus_driver);
}

void innopmbus_driver_unregister(void)
{
	platform_driver_unregister(&inno_pmbus_driver);
}
#else
module_platform_driver(inno_pmbus_driver);
MODULE_AUTHOR("Innosilicon Technologies Ltd. <support@innosilicon.com.cn>");
MODULE_DESCRIPTION("Innosilicon pmbus adapter");
MODULE_LICENSE("Dual BSD/GPL");
#endif

