/*************************************************************************/ /*!
@File           hal_bmcif.h
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

#ifndef _HAL_BMCIF_H_
#define _HAL_BMCIF_H_
enum bmc_item_id {
	//0x00 REG_ENTITY0000
	BMC_VENDOR_ID_MSB,  //8BIT        生产厂商
	BMC_VENDOR_ID_LSB,  //8BIT        生产厂商

	//0x04 REG_ENTITY0001
	BMC_DEVICE_ID_MSB,  //8BIT        设备信息
	BMC_DEVICE_ID_LSB,  //8BIT        设备信息

	//0x08 REG_ENTITY0002
	BMC_DATA_CODE_YEAR, //8BIT        生产批次-年
	BMC_DATA_CODE_WEEK, //8BIT        生产批次-周

	//0x70 REG_ENTITY0028
	BMC_MEM_TYPE,       //8BIT        内存类型
	BMC_MEM_WIDTH,      //4BIT        内存位宽
	BMC_MEM_NUM,        //4BIT        内存颗粒数

	//0x74 REG_ENTITY0029
	BMC_MEM_SPEED,      //16BIT       ddr速度

	//0x78 REG_ENTITY0030
	BMC_DP2VGA_I2C_ADDR, //8BIT       dp转vga的iic设备地址
	BMC_DP2VGA_I2C_ID,   //4BIT       pmbus号
	BMC_DP2VGA_CHIP,     //4BIT       chip名称代号

	//0x7c REG_ENTITY0031
	BMC_DP2VGA_EN,       //1BIT       是否DP转VGA
	BMC_HDMI2DVI_EN,     //1BIT       是否HDMI转DVI

	//0x80 REG_ENTITY0032
	BMC_MCUFW_VER_MAJOR, //8BIT       MCU固件主版本号
	BMC_MCUFW_VER_MINOR, //8BIT       MCU固件次版本号

	//0x84  REG_ENTITY0033
	BMC_MCUFW_VER_REVISION,     //8BIT MCU固件修订版本号
	BMC_VBIOS_VER_MAJOR,        //8BIT VBIOS固件主版本号

	//0x88  REG_ENTITY0034
	BMC_VBIOS_VER_MINOR,        //8BIT VBIOS固件次版本号
	BMC_VBIOS_VER_REVISION,     //8BIT VBIOS固件修订版本号

	//0x8c  REG_ENTITY0035
	BMC_PCIE_LINK_STATUS,       //1BIT PCIE link 状态
	BMC_PCIE_WIDTH,             //7BIT PCIE 位宽
	BMC_PCIE_SPEED,             //4BIT PCIE 速度

	//0X90  REG_ENTITY0036
	BMC_GPU_POWER,              //8BIT GPU的实时功耗
	BMC_GPU_FREQ,               //8BIT GPU频率

	//0X94  REG_ENTITY0037
	BMC_OVERHEAT_STATUS,        //1BIT 过温断电
	BMC_HIGH_TEMP_STATUS,       //1BIT 高温预警
	BMC_MEM_INIT_STATUES,       //1BIT DDR初始化有无异常
	BMC_DDR_RW_STATUS,          //1BIT DDR读写错误
	BMC_BOARD_TEMP_STATUS,      //1BIT 板温获取错误
	BMC_POWER_MONITOR_STATUS,   //1BIT 功耗监测芯片访问错误
	BMC_VOLT_CTRL_STATUS,       //1BIT 电压调节芯片访问错误
	BMC_BOARD_FAN_STATUS,       //1BIT 板极风扇访问错误
	BMC_PMIC_STATUS,            //1BIT 电源管理芯片访问错误

	//0X98  REG_ENTITY0038
	BMC_BOAR_TEMP,              //8BIT 电路板温度

	//0X9C  REG_ENTITY0039
	BMC_CHIP_TEMP,              //8BIT 芯片温度
	BMC_FAN_SPEED,              //8BIT 风扇转速

	//0XA0 REG_ENTITY0040
    //RESERVED

	//0XA4 REG_ENTITY0041
	BMC_CHIP_NUM,               //4BIT 1张显卡多个GPU芯片

	//0XA8 REG_ENTITY0042
	BMC_GPU_VOLTAGE,            //8BIT gpu电压

	//0XAC REG_ENTITY0043
	BMC_MCUFW_VER_BUILD,       //16BIT       MCU固件build号

	//0XC0 REG_ENTITY0048
	BMC_DATABUS_FREQ,           //8BIT databus运行频率
	BMC_APU_FREQ,               //8BIT APU运行速率

	//0XC4 REG_ENTITY0049
	BMC_CPU_FREQ,               //8BIT cpu运行频率
	BMC_VPU_FREQ,               //8BIT vpu运行频率

	//0XC8 - 0XD4 RESERVED

	//0XD8 REG_ENTITY0054
	BMC_VER_MSB,                //4BIT BMC版本号高位
	BMC_VER_LSB,                //4BIT BMC版本号低位

	//0XDC REG_ENTITY0055
	BMC_VBIOS_FLAG,             //4BIT vbios flag

	//0XF0 REG_ENTITY0056
	BMC_HWINFO_UPGRADE_ADDR_MSB, //16BIT hwinfo(extra)升级/读取数据基地址高位

	//0XF4 REG_ENTITY0057
	BMC_HWINFO_UPGRADE_ADDR_LSB, //16BIT hwinfo(extra)升级/读取数据基地址低位

	//0XF8 REG_ENTITY0058
	BMC_OTA_UPGRADE_FIN_REST,    //1BIT 升级结果
	BMC_OTA_FW_UPGRADE_REST,     //2BIT FW升级结果
	BMC_OTA_VBIOS_UPGRADE_REST,  //2BIT VBIOS升级结果
	BMC_OTA_HWINFO_UPGRADE_REST, //2BIT HWINFO(EXTRA)升级结果
	BMC_OTA_UPGRADE_STEP,        //4BIT 升级步骤

	//0XFC REG_ENTITY0059
	BMC_INT_OTA_UPGRADE,         //1BIT OTA升级标识
	BMC_UPGRADE_HWINFO,          //1BIT 升级hwinfo
	BMC_READ_HWINFO,             //1BIT 读取hwinfo
	BMC_UPGRADE_HWINFO_EXTRA,    //1BIT 升级hwinfo_extra
	BMC_READ_HWINFO_EXTRA,       //1BIT 读取hwinfo_extra
	BMC_CPU2MCU_CMD,             //1BIT cpu下发命令给mcu
	BMC_OVER_HEAT,               //1BIT 上报过温
	BMC_POWER_DOWN,              //1BIT 通知MCU下电操作
	BMC_FLASH_FILE_UPGRADE,      //1BIT flash通用升级
	BMC_FLASH_FILE_READ,         //1BIT flash通用读取
	BMC_ITEM_MAX,
};

int fh2m_hal_bmc_get_val(void *dev, enum bmc_item_id item_id);
#endif
