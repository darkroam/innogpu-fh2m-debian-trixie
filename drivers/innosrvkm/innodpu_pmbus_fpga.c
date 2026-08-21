/*************************************************************************/ /*!
@File                   innodpu_pmbus_fpga.c
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License        	Dual MIT/GPLv2

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
#include <linux/platform_device.h>
#include "innodpu_drm_drv.h"
#include "innodpu_pmbus_fpga.h"
static void __iomem *pmbus_ctrl_base[10];

void __iomem *innopmbus_base(u32 id)
{
	if (id < 10)
		return pmbus_ctrl_base[id];
	else
		return NULL;
}

/**
 * FPGA_XIic_ReadReg() - 读寄存器
 * @offset:  - 偏移量
 * @reg_base:  基地址
 *
 * Return: 若reg_base等于NULL,函数返回0xffffffff;否则，返回读取到的寄存器的值
 */
u32 FPGA_XIic_ReadReg(u32 RegOffset, void __iomem * reg_base)
{
	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return 0xffffffff;
	}

	return ioread32(reg_base + RegOffset);
}

/**
 * FPGA_XIic_WriteReg() - 写寄存器
 * @offset:  - 偏移量
 * @value： -  待写入的值
 * @reg_base:  基地址
 *
 * Return: 函数无返回值
 */
void FPGA_XIic_WriteReg(u32 offset, u32 value, void __iomem * reg_base)
{
	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return;
	}

	iowrite32(value, (reg_base + offset));
}

/**
 * innopmbus_init() - 初始化pmbus总线
 * @reg_base:  基地址
 *
 * Return: 函数无返回值
 */
static void innopmbus_init(void __iomem * reg_base)
{
	u32 baud = 15 * 1000 * 1000 / (4 * PMBUS_CLOCK_FREQ);

	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return;
	}

	FPGA_XIic_WriteReg(ADDR_S_PMBUS_SLV_CTRL, 0x1, reg_base);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, (0x1388 << 16) | (baud << 2) | 0x3, reg_base);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x3, reg_base);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_INTR_MASK, 0x7, reg_base);
}

/**
 * fh2m_innopmbus_send_data() - 通过pmbus发送数据
 * @reg_base:  -  基地址
 * @byte_mode: -  字节模式，当byte_mode等于4的时候，需要对dev_addr、wdata进行大小端转换
 * @dev_addr： -  设备地址
 * @value：	   -  待写入的数据
 * @num_bytes: -  字节数
 *
 * Return: 函数正常结束时返回值0
 */
u32 fpga_innopmbus_send_data(void __iomem * reg_base, u32 byte_mode, u32 dev_addr,
						u32 value, u32 num_bytes)
{
	u32 wdata = 0, rdata = 0;

	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return -1;
	}

	if (byte_mode == 4) {
		dev_addr = ((dev_addr & 0xff) << 24) | ((dev_addr & 0xff00) << 8) |
			((dev_addr & 0xff0000) >> 8) | ((dev_addr & 0xff000000) >> 24);
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_WR_FIFO, dev_addr, reg_base);

		wdata = ((value & 0xff) << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8) |
			((value & 0xff000000) >> 24);

		FPGA_XIic_WriteReg(ADDR_S_PMBUS_WR_FIFO, wdata, reg_base);

		wdata = ((0x1 << 31) | (0x10 << 17) | (0x0 << 16) | (0x5 + num_bytes));
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CMD, wdata, reg_base);
	} else {
		wdata = (value << 8) | (dev_addr & 0xff);
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_WR_FIFO, wdata, reg_base);
		//innopmbus_info("PMBUS reg:%pK  data:0x%x\n", reg_base+ADDR_S_PMBUS_WR_FIFO, wdata);
		wdata = ((0x1 << 31) | ((dev_addr >> 8) << 17) | (0x0 << 16) | (0x2 + num_bytes));
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CMD, wdata, reg_base);
		//innopmbus_info("PMBUS reg:%pK  data:0x%x\n", reg_base+ADDR_S_PMBUS_CMD, wdata);
	}

	fh2m_inno_mdelay(5);

	while (1) {
		fh2m_inno_mdelay(5);
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_STA, reg_base);
		if (((rdata >> 15) & 0x1) == 1) {
			break;
		}
	}

	rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_ERR_STA, reg_base);
	if (rdata) {
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x1, reg_base);

		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_CFG, reg_base);

		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata & 0xfffffffc, reg_base);
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata | 0x3, reg_base);
		innopmbus_err("pmbus write error!!!\n");
	} else {
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x1, reg_base);
	}

	return 0;
}

/**
 * fh2m_innopmbus_receive_data() - 通过pmbus读取数据
 * @reg_base:  -  基地址
 * @byte_mode: -  字节模式
 * @dev_addr： -  设备地址
 * @num_bytes: -  字节数
 *
 * Return: 函数结束时返回读取到的数据
*/
u32 fpga_innopmbus_receive_data(void __iomem * reg_base, u32 byte_mode, u32 dev_addr, u32 num_bytes)
{
	u32 reg = 0;
	u32 wdata = 0;
	u32 rdata = 0;

	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return -1;
	}

	if (byte_mode == 4) {
		wdata = (0x1 << 31) | (0x10 << 17) | (0x1 << 16) | (0x1 + num_bytes);
	} else {
		wdata = (0x1 << 31) | ((dev_addr >> 8) << 17) | (0x1 << 16) | (0x1 + num_bytes);
	}
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_CMD, wdata, reg_base);

	fh2m_inno_mdelay(5);

	while (1) {
		fh2m_inno_mdelay(5);
		reg = FPGA_XIic_ReadReg(ADDR_S_PMBUS_STA, reg_base);
		if (((reg >> 15) & 0x01) == 1) {
			break;
		}
	}

	rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_ERR_STA, reg_base);
	if (rdata) {
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x1, reg_base);
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_CFG, reg_base);

		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata & 0xfffffffc, reg_base);
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata | 0x3, reg_base);
		innopmbus_err("pmbus read error!!!\n");
		return 0;
	} else {
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_RD_FIFO, reg_base);
	}

	return rdata;
}

static bool fpga_innopmbus_hdmi_check_status(void __iomem * reg_base)
{
	u32 rdata = 0;

	while (1) {
		fh2m_inno_mdelay(5);
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_STA, reg_base);
		if (((rdata >> 15) & 0x01) == 1) {
			break;
		}
	}

	rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_ERR_STA, reg_base);
	if (rdata) {
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x1, reg_base);
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_CFG, reg_base);

		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata & 0xfffffffc, reg_base);
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_CFG, rdata | 0x3, reg_base);
		innopmbus_err("pmbus write/read error!!!\n");
		return false;
	}

	return true;
}

u32 fpga_innopmbus_hdmi_receive_data(void __iomem * reg_base, u32 reg_addr, u32 num_bytes)
{
	u32 wdata = 0;
	u32 rdata = 0;

	if (NULL == reg_base) {
		innopmbus_err("Invalid argument: the pointer of reg_base is NULL!\n");
		return -1;
	}

	/* 1. write reg addr */
	reg_addr = ((reg_addr & 0x000000ff) << 24) | ((reg_addr & 0x0000ff00) << 8) |
               ((reg_addr & 0x00ff0000) >> 8)  | ((reg_addr & 0xff000000) >> 24);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_WR_FIFO, reg_addr, reg_base);
	wdata = (0x1 << 31) | (0x10 << 17) | (0x0 << 16) | (0x1 + num_bytes);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_CMD, wdata, reg_base);
	if (fpga_innopmbus_hdmi_check_status(reg_base)) {
		/* clear the write fifo after transfer done */
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x1, reg_base);
	}

	/* 2. read data from reg addr */
	wdata = (0x1 << 31) | (0x10 << 17) | (0x1 << 16) | (0x1 + num_bytes);
	FPGA_XIic_WriteReg(ADDR_S_PMBUS_CMD, wdata, reg_base);
	if (fpga_innopmbus_hdmi_check_status(reg_base)) {
		rdata = FPGA_XIic_ReadReg(ADDR_S_PMBUS_RD_FIFO, reg_base);
		/* clear the read fifo after transfer done */
		FPGA_XIic_WriteReg(ADDR_S_PMBUS_FIFO_CTRL, 0x2, reg_base);
	}

	return rdata;
}

/**
 * innopmbus_bind() - 获取设备的基地址，并映射到虚拟地址空间
 * @dev:  -   struct device类型的指针，指向device对象
 * @master: - struct device类型的指针，指向device对象
 * @data： -  任意类型的指针变量
 *
 * Return: 函数正常结束时返回0
*/
static s32 innopmbus_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = NULL;
	struct resource *memres_ctrl = NULL;
	s32 pmbus_id = 0;
	plat_data_t *pdata =  dev_get_platdata(dev);

	//if (!(s_pmbus_nums & (1<<pdata->dev_idx)))
	//	return 0;

	if (NULL == dev || NULL == master || NULL == data || NULL == pdata) {
		innopmbus_err("Invalid argument: the pointer of dev is [%p]!\n",(char *)dev);
		return -1;
	}
	pdev = fh2m_inno_to_platform_device(dev);
	pmbus_id = pdata->dev_idx;

	memres_ctrl = fh2m_inno_platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmbus-regs");
	if (!memres_ctrl) {
		innopmbus_err("platform_get_resource failed!\n");
		return -1;
	}

	pmbus_ctrl_base[pmbus_id] = devm_ioremap_resource(dev, memres_ctrl);
	innopmbus_err("!!!!!!!!!!pmbus-%d paddr-%#llx is %pK\n",pmbus_id,
		memres_ctrl->start, pmbus_ctrl_base[pmbus_id]);

	if (IS_ERR(pmbus_ctrl_base[pmbus_id])) {
		innopmbus_err("devm_ioremap_resource failed!\n");
		return PTR_ERR(pmbus_ctrl_base[pmbus_id]);
	}

	innopmbus_info("PMBUS%d  base:%pK  init!\n", pmbus_id, pmbus_ctrl_base[pmbus_id]);
	innopmbus_init(pmbus_ctrl_base[pmbus_id]);

	return 0;
}

/**
 * innopmbus_bind() - 解除映射关系
 * @dev:  -   struct device类型的指针，指向device对象
 * @master: - struct device类型的指针，指向device对象
 * @data： -  任意类型的指针变量
 *
 * Return: 函数无返回值
*/
static void innopmbus_unbind(struct device *dev, struct device *master, void *data)
{
	s32 pmbus_id = 0;
	plat_data_t *pdata =  dev_get_platdata(dev);
	pmbus_id = pdata->dev_idx;
#if 0
	if (!(s_pmbus_nums & (1<<pdata->dev_idx)))
		return;
#endif
	if (IS_ERR(pdata))
		devm_iounmap(dev, pmbus_ctrl_base[pmbus_id]);
}

static const struct component_ops innopmbus_ops = {
	.bind = innopmbus_bind,
	.unbind = innopmbus_unbind,
};

/**
 * innopmbus_probe() - 添加组件
 * @pdev:  -  struct platform_device类型的指针，指向平台设备对象
 *
 * Return: 函数结束时返回值0
*/
static s32 innopmbus_probe(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	return component_add(&pdev->dev, &innopmbus_ops);
}

/**
 * innopmbus_remove() - 删除组件
 * @pdev:  -  struct platform_device类型的指针，指向平台设备对象
 *
 * Return: 函数结束时返回值0
*/
static s32 innopmbus_remove(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	component_del(&pdev->dev, &innopmbus_ops);
	return 0;
}

static struct platform_device_id inno_pmbus_platform_device_id_table[] = {
	{.name = INNO_PMBUS_DEVICE_NAME,.driver_data = 0},
	{},
};

MODULE_DEVICE_TABLE(platform, inno_pmbus_platform_device_id_table);

static int innodpu_pmbus_suspend(struct device *dev)
{
	innopmbus_info("suspend\n");

	return 0;
}

static int innodpu_pmbus_resume(struct device *dev)
{
	innopmbus_info("resume\n");

	return 0;
}

static int innodpu_pmbus_freeze(struct device *dev)
{
	innopmbus_info("freeze\n");
	return innodpu_pmbus_suspend(dev);
}

static int innodpu_pmbus_thaw(struct device *dev)
{
	innopmbus_info("thaw\n");
	return 0;
}

static int innodpu_pmbus_poweroff(struct device *dev)
{
	innopmbus_info("poweroff\n");
	return 0;
}

static int innodpu_pmbus_restore(struct device *dev)
{
	innopmbus_info("restore\n");
	return innodpu_pmbus_resume(dev);
}

static const struct dev_pm_ops innodpu_pmbus_pm_ops = {
	.suspend = innodpu_pmbus_suspend,
	.resume = innodpu_pmbus_resume,
	.freeze = innodpu_pmbus_freeze,
	.thaw = innodpu_pmbus_thaw,
	.poweroff = innodpu_pmbus_poweroff,
	.restore = innodpu_pmbus_restore,
};

struct platform_driver g_inno_pmbus_driver = {
	.probe = innopmbus_probe,
	.remove = innopmbus_remove,
	.driver = {
		.name = INNO_PMBUS_DEVICE_NAME,
		.pm = &innodpu_pmbus_pm_ops,
	},
	.id_table = inno_pmbus_platform_device_id_table,
};
