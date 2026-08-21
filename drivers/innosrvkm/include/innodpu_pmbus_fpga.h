/*************************************************************************/ /*!
@File                   innodpu_pmbus_fpga.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License   	     	Dual MIT/GPLv2

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
#ifndef __INNODPU_PMBUS_H__
#define __INNODPU_PMBUS_H__

#include "hal_interface.h"
#include "inno_debug.h"

#define INNOPMBUS "innopmbus"

#define pr_fmt_pmbus(fmt) "[%s][%s:%d]" fmt,INNOPMBUS,__func__,__LINE__

#define innopmbus_dev_dbg(dev, fmt,...)\
	fh2m_inno_dev_printk(KERN_DEBUG, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define innopmbus_dev_info(dev, fmt, ...)\
	fh2m_inno_dev_printk(KERN_INFO, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define innopmbus_dev_warn(dev, fmt, ...)\
	fh2m_inno_dev_printk(KERN_WARNING, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define innopmbus_dev_err(dev, fmt, ...)\
	fh2m_inno_dev_printk(KERN_ERR, dev, pr_fmt_pmbus(fmt), ##__VA_ARGS__)

#define innopmbus_info(fmt, ...)\
	fh2m_inno_printk(KERN_INFO, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define innopmbus_warn(fmt, ...)\
	fh2m_inno_printk(KERN_WARNING, pr_fmt_pmbus(fmt), ##__VA_ARGS__)
#define innopmbus_err(fmt, ...)\
	fh2m_inno_printk(KERN_ERR, pr_fmt_pmbus(fmt), ##__VA_ARGS__)

#define I2C_PMBUS_BASE									(0xc70000)

#define PMBUS_COUNT										(3)

#define SEND_COUNT										(8)
#define RECEIVE_COUNT									(8)

#define RPT_START										(1)
#define PEC_SEL											(1)
#define EN_PEC											(1)

#define FDEP_PMBUS_WR_FIFO								(255)
#define FDEP_PMBUS_RD_FIFO								(255)

#define ADDR_S_PMBUS_CFG								(0x00000000)
#define ADDR_S_PMBUS_SLV_CTRL							(0x00000004)
#define ADDR_S_PMBUS_CMD								(0x00000008)
#define ADDR_S_PMBUS_ERR_STA							(0x0000000C)
#define ADDR_S_PMBUS_STA								(0x00000010)
#define ADDR_S_PMBUS_WR_FIFO							(0x00000014)
#define ADDR_S_PMBUS_RD_FIFO							(0x00000018)
#define ADDR_S_PMBUS_PEC								(0x0000001c)
#define ADDR_S_PMBUS_FIFO_CTRL							(0x00000020)
#define ADDR_S_PMBUS_INTR_MASK							(0x00000024)
#define ADDR_S_PMBUS_INTR_CLR							(0x00000028)
#define ADDR_S_PMBUS_INTR								(0x0000002C)

#define PMBUS_CLOCK_FREQ 								(100*1000)

/* Register macros to access fields within a 32-bit register value */

#define RegClr(reg_val) \
	do {reg_val = 0;} while(0)

#define RegLd(reg_val, field_val, REGNAME, FIELDNAME) \
	do {reg_val |= ((field_val) & MSK_##REGNAME##__##FIELDNAME ) << LSB_##REGNAME##__##FIELDNAME;}\
		while (0)

#define RegUnldU(reg_val, field_val, REGNAME, FIELDNAME) \
	do {field_val = ((reg_val >> LSB_##REGNAME##__##FIELDNAME ) & MSK_##REGNAME##__##FIELDNAME );}\
		while (0)

#define RegUnldS(reg_val, field_val, REGNAME, FIELDNAME) \
	do {field_val = (((int32_t) reg_val << (31 - MSB_##REGNAME##__##FIELDNAME )) >>\
		(31 - MSB_##REGNAME##__##FIELDNAME + LSB_##REGNAME##__##FIELDNAME ));} while (0)

#define RegMsk(reg,  REGNAME, FIELDNAME) \
	do {reg &= ~(( MSK_##REGNAME##__##FIELDNAME ) << LSB_##REGNAME##__##FIELDNAME);} while (0)

#define RegChgField(reg, field_val, REGNAME, FIELDNAME) \
	do {RegMsk(reg,  REGNAME, FIELDNAME); RegLd(reg, field_val, REGNAME, FIELDNAME);} while (0)

#define RegClampField(reg, field_val, REGNAME, FIELDNAME) \
	do { if ((field_val) > ( MSK_##REGNAME##__##FIELDNAME ))\
		{RegChgField(reg, ( MSK_##REGNAME##__##FIELDNAME ), REGNAME, FIELDNAME);}\
		else {RegChgField(reg, field_val, REGNAME, FIELDNAME);} } while (0)

/* Misc */
#define LSB_S_PMBUS_CFG__TIMEOUT_THRESH_DIV8						(16)
#define MSB_S_PMBUS_CFG__TIMEOUT_THRESH_DIV8						(31)
#define MSK_S_PMBUS_CFG__TIMEOUT_THRESH_DIV8				(0x0000ffff)
#define SGN_S_PMBUS_CFG__TIMEOUT_THRESH_DIV8						 (0)
#define LSB_S_PMBUS_CFG__BAUDR										 (2)
#define MSB_S_PMBUS_CFG__BAUDR										(15)
#define MSK_S_PMBUS_CFG__BAUDR								(0x00003fff)
#define SGN_S_PMBUS_CFG__BAUDR										 (0)
#define LSB_S_PMBUS_CFG__EN_PEC										 (1)
#define MSB_S_PMBUS_CFG__EN_PEC										 (1)
#define MSK_S_PMBUS_CFG__EN_PEC								(0x00000001)
#define SGN_S_PMBUS_CFG__EN_PEC										 (0)
#define LSB_S_PMBUS_CFG__ENABLE										 (0)
#define MSB_S_PMBUS_CFG__ENABLE										 (0)
#define MSK_S_PMBUS_CFG__ENABLE								(0x00000001)
#define SGN_S_PMBUS_CFG__ENABLE										 (0)

#define LSB_S_PMBUS_CTRL_SLV__CTRL									 (0)
#define MSB_S_PMBUS_CTRL_SLV__CTRL									 (0)
#define MSK_S_PMBUS_CTRL_SLV__CTRL							(0x00000001)
#define SGN_S_PMBUS_CTRL_SLV__CTRL									 (0)

#define LSB_S_PMBUS_CMD__CMD_ACTIVE									(31)
#define MSB_S_PMBUS_CMD__CMD_ACTIVE									(31)
#define MSK_S_PMBUS_CMD__CMD_ACTIVE							(0x00000001)
#define SGN_S_PMBUS_CMD__CMD_ACTIVE									 (0)
#define LSB_S_PMBUS_CMD__DEV_ADDR									(17)
#define MSB_S_PMBUS_CMD__DEV_ADDR									(26)
#define MSK_S_PMBUS_CMD__DEV_ADDR							(0x000003ff)
#define SGN_S_PMBUS_CMD__DEV_ADDR									 (0)
#define LSB_S_PMBUS_CMD__READ_WRITEB								(16)
#define MSB_S_PMBUS_CMD__READ_WRITEB								(16)
#define MSK_S_PMBUS_CMD__READ_WRITEB						(0x00000001)
#define SGN_S_PMBUS_CMD__READ_WRITEB								 (0)
#define LSB_S_PMBUS_CMD__PEC_REPORT									(10)
#define MSB_S_PMBUS_CMD__PEC_REPORT									(10)
#define MSK_S_PMBUS_CMD__PEC_REPORT							(0x00000001)
#define SGN_S_PMBUS_CMD__PEC_REPORT									 (0)
#define LSB_S_PMBUS_CMD__RPT_START									 (9)
#define MSB_S_PMBUS_CMD__RPT_START									 (9)
#define MSK_S_PMBUS_CMD__RPT_START							(0x00000001)
#define SGN_S_PMBUS_CMD__RPT_START									 (0)
#define LSB_S_PMBUS_CMD__EN_10B										 (8)
#define MSB_S_PMBUS_CMD__EN_10B										 (8)
#define MSK_S_PMBUS_CMD__EN_10B								(0x00000001)
#define SGN_S_PMBUS_CMD__EN_10B										 (0)
#define LSB_S_PMBUS_CMD__NUM_BYTES									 (0)
#define MSB_S_PMBUS_CMD__NUM_BYTES									 (7)
#define MSK_S_PMBUS_CMD__NUM_BYTES							(0x000001ff)
#define SGN_S_PMBUS_CMD__NUM_BYTES									 (0)

#define LSB_S_PMBUS_ERR_STA__PEC_ERR								(14)
#define MSB_S_PMBUS_ERR_STA__PEC_ERR								(14)
#define MSK_S_PMBUS_ERR_STA__PEC_ERR						(0x00000001)
#define SGN_S_PMBUS_ERR_STA__PEC_ERR								 (0)
#define LSB_S_PMBUS_ERR_STA__BYTES_AT_FAILURE						 (6)
#define MSB_S_PMBUS_ERR_STA__BYTES_AT_FAILURE						(13)
#define MSK_S_PMBUS_ERR_STA__BYTES_AT_FAILURE				(0x000000ff)
#define SGN_S_PMBUS_ERR_STA__BYTES_AT_FAILURE						 (0)
#define LSB_S_PMBUS_ERR_STA__TIMEOUT_START							 (5)
#define MSB_S_PMBUS_ERR_STA__TIMEOUT_START							 (5)
#define MSK_S_PMBUS_ERR_STA__TIMEOUT_START					(0x00000001)
#define SGN_S_PMBUS_ERR_STA__TIMEOUT_START							 (0)
#define LSB_S_PMBUS_ERR_STA__TIMEOUT_XFR							 (4)
#define MSB_S_PMBUS_ERR_STA__TIMEOUT_XFR							 (4)
#define MSK_S_PMBUS_ERR_STA__TIMEOUT_XFR					(0x00000001)
#define SGN_S_PMBUS_ERR_STA__TIMEOUT_XFR							 (0)
#define LSB_S_PMBUS_ERR_STA__TIMEOUT_CLKSTRETCH						 (3)
#define MSB_S_PMBUS_ERR_STA__TIMEOUT_CLKSTRETCH						 (3)
#define MSK_S_PMBUS_ERR_STA__TIMEOUT_CLKSTRETCH				(0x00000001)
#define SGN_S_PMBUS_ERR_STA__TIMEOUT_CLKSTRETCH						 (0)
#define LSB_S_PMBUS_ERR_STA__NO_ACK									 (2)
#define MSB_S_PMBUS_ERR_STA__NO_ACK									 (2)
#define MSK_S_PMBUS_ERR_STA__NO_ACK							(0x00000001)
#define SGN_S_PMBUS_ERR_STA__NO_ACK									 (0)
#define LSB_S_PMBUS_ERR_STA__WR_FIFO_UNDERFLOW						 (1)
#define MSB_S_PMBUS_ERR_STA__WR_FIFO_UNDERFLOW						 (1)
#define MSK_S_PMBUS_ERR_STA__WR_FIFO_UNDERFLOW				(0x00000001)
#define SGN_S_PMBUS_ERR_STA__WR_FIFO_UNDERFLOW						 (0)
#define LSB_S_PMBUS_ERR_STA__RD_FIFO_OVERFLOW						 (0)
#define MSB_S_PMBUS_ERR_STA__RD_FIFO_OVERFLOW						 (0)
#define MSK_S_PMBUS_ERR_STA__RD_FIFO_OVERFLOW				(0x00000001)
#define SGN_S_PMBUS_ERR_STA__RD_FIFO_OVERFLOW						 (0)

#define LSB_S_PMBUS_STA__RD_FIFO_ENTRIES							(16)
#define MSB_S_PMBUS_STA__RD_FIFO_ENTRIES							(23)
#define MSK_S_PMBUS_STA__RD_FIFO_ENTRIES					(0x000000ff)
#define SGN_S_PMBUS_STA__RD_FIFO_ENTRIES							 (0)
#define LSB_S_PMBUS_STA__CMD_STATUS									(15)
#define MSB_S_PMBUS_STA__CMD_STATUS									(15)
#define MSK_S_PMBUS_STA__CMD_STATUS							(0x00000001)
#define SGN_S_PMBUS_STA__CMD_STATUS									 (0)
#define LSB_S_PMBUS_STA__WR_FIFO_ENTRIES							 (6)
#define MSB_S_PMBUS_STA__WR_FIFO_ENTRIES							(13)
#define MSK_S_PMBUS_STA__WR_FIFO_ENTRIES					(0x000000ff)
#define SGN_S_PMBUS_STA__WR_FIFO_ENTRIES							 (0)
#define LSB_S_PMBUS_STA__RD_FIFO_FULL								 (5)
#define MSB_S_PMBUS_STA__RD_FIFO_FULL								 (5)
#define MSK_S_PMBUS_STA__RD_FIFO_FULL						(0x00000001)
#define SGN_S_PMBUS_STA__RD_FIFO_FULL								 (0)
#define LSB_S_PMBUS_STA__WR_FIFO_FULL								 (4)
#define MSB_S_PMBUS_STA__WR_FIFO_FULL								 (4)
#define MSK_S_PMBUS_STA__WR_FIFO_FULL						(0x00000001)
#define SGN_S_PMBUS_STA__WR_FIFO_FULL								 (0)
#define LSB_S_PMBUS_STA__RD_FIFO_EMPTY								 (3)
#define MSB_S_PMBUS_STA__RD_FIFO_EMPTY								 (3)
#define MSK_S_PMBUS_STA__RD_FIFO_EMPTY						(0x00000001)
#define SGN_S_PMBUS_STA__RD_FIFO_EMPTY								 (0)
#define LSB_S_PMBUS_STA__WR_FIFO_EMPTY								 (2)
#define MSB_S_PMBUS_STA__WR_FIFO_EMPTY								 (2)
#define MSK_S_PMBUS_STA__WR_FIFO_EMPTY						(0x00000001)
#define SGN_S_PMBUS_STA__WR_FIFO_EMPTY								 (0)
#define LSB_S_PMBUS_STA__ERROR										 (1)
#define MSB_S_PMBUS_STA__ERROR										 (1)
#define MSK_S_PMBUS_STA__ERROR								(0x00000001)
#define SGN_S_PMBUS_STA__ERROR										 (0)
#define LSB_S_PMBUS_STA__BUSY										 (0)
#define MSB_S_PMBUS_STA__BUSY										 (0)
#define MSK_S_PMBUS_STA__BUSY								(0x00000001)
#define SGN_S_PMBUS_STA__BUSY										 (0)

#define LSB_S_PMBUS_WR_FIFO__DATA									 (0)
#define MSB_S_PMBUS_WR_FIFO__DATA									(31)
#define MSK_S_PMBUS_WR_FIFO__DATA							(0xffffffff)
#define SGN_S_PMBUS_WR_FIFO__DATA									 (0)

#define LSB_S_PMBUS_RD_FIFO__DATA									 (0)
#define MSB_S_PMBUS_RD_FIFO__DATA									(31)
#define MSK_S_PMBUS_RD_FIFO__DATA							(0xffffffff)
#define SGN_S_PMBUS_RD_FIFO__DATA									 (0)

#define LSB_S_PMBUS_FIFO_CTRL__WR									 (0)
#define MSB_S_PMBUS_FIFO_CTRL__WR									 (0)
#define MSK_S_PMBUS_FIFO_CTRL__WR							(0x00000001)
#define SGN_S_PMBUS_FIFO_CTRL__WR									 (0)

#define LSB_S_PMBUS_FIFO_CTRL__RD									 (1)
#define MSB_S_PMBUS_FIFO_CTRL__RD									 (1)
#define MSK_S_PMBUS_FIFO_CTRL__RD							(0x00000001)
#define SGN_S_PMBUS_FIFO_CTRL__RD									 (0)

#define LSB_S_PMBUS_PEC__PEC_VAL									 (0)
#define MSB_S_PMBUS_PEC__PEC_VAL									 (7)
#define MSK_S_PMBUS_PEC__PEC_VAL							(0x000000ff)
#define SGN_S_PMBUS_PEC__PEC_VAL									 (0)

#define LSB_S_PMBUS_INTR_MASK__PEC_ERR_M							 (2)
#define MSB_S_PMBUS_INTR_MASK__PEC_ERR_M							 (2)
#define MSK_S_PMBUS_INTR_MASK__PEC_ERR_M					(0x00000001)
#define SGN_S_PMBUS_INTR_MASK__PEC_ERR_M							 (0)
#define LSB_S_PMBUS_INTR_MASK__TIMEOUT_M							 (1)
#define MSB_S_PMBUS_INTR_MASK__TIMEOUT_M							 (1)
#define MSK_S_PMBUS_INTR_MASK__TIMEOUT_M					(0x00000001)
#define SGN_S_PMBUS_INTR_MASK__TIMEOUT_M							 (0)
#define LSB_S_PMBUS_INTR_MASK__SLV_ALERT_M							 (0)
#define MSB_S_PMBUS_INTR_MASK__SLV_ALERT_M							 (0)
#define MSK_S_PMBUS_INTR_MASK__SLV_ALERT_M					(0x00000001)
#define SGN_S_PMBUS_INTR_MASK__SLV_ALERT_M							 (0)

#define LSB_S_PMBUS_INTR_CLR__TIMEOUT_CLR							 (1)
#define MSB_S_PMBUS_INTR_CLR__TIMEOUT_CLR							 (1)
#define MSK_S_PMBUS_INTR_CLR__TIMEOUT_CLR					(0x00000001)
#define SGN_S_PMBUS_INTR_CLR__TIMEOUT_CLR							 (0)
#define LSB_S_PMBUS_INTR_CLR__SLV_ALERT_CLR							 (0)
#define MSB_S_PMBUS_INTR_CLR__SLV_ALERT_CLR							 (0)
#define MSK_S_PMBUS_INTR_CLR__SLV_ALERT_CLR					(0x00000001)
#define SGN_S_PMBUS_INTR_CLR__SLV_ALERT_CLR							 (0)

#define LSB_S_PMBUS_INTR__TIMEOUT									 (1)
#define MSB_S_PMBUS_INTR__TIMEOUT									 (1)
#define MSK_S_PMBUS_INTR__TIMEOUT							(0x00000001)
#define SGN_S_PMBUS_INTR__TIMEOUT									 (0)
#define LSB_S_PMBUS_INTR__SLV_ALERT									 (0)
#define MSB_S_PMBUS_INTR__SLV_ALERT									 (0)
#define MSK_S_PMBUS_INTR__SLV_ALERT							(0x00000001)
#define SGN_S_PMBUS_INTR__SLV_ALERT									 (0)

typedef enum {
	quick_command = 0x00,
	send_byte = 0x01,
	receive_byte = 0x02,
	write_bywr = 0x03,
	read_bywr = 0x04,
	process_call = 0x05,
	block_read = 0x06,
	block_write = 0x07,
	wrb_rd_process_call = 0x08,
	group_command_mode = 0x09
} command_type_e;

#define XIic_In32   Xil_In32
#define XIic_Out32  Xil_Out32

/****************************************************************************/
/**
*
* Read from the specified IIC device register.
*
* @param	BaseAddress is the base address of the device.
* @param	RegOffset is the offset from the 1st register of the device to
*			select the specific register.
*
* @return   The value read from the register.
*
* @note		C-Style signature:
*			u32 XIic_ReadReg(u32 RegOffset);
*
*			This macro does not do any checking to ensure that the
*			register exists if the register may be excluded due to
*			parameterization, such as the GPO Register.
*
******************************************************************************/

u32 FPGA_XIic_ReadReg(u32 RegOffset, void __iomem * reg_base);
void FPGA_XIic_WriteReg(u32 offset, u32 value, void __iomem * reg_base);

/***************************************************************************/
/**
*
* Write to the specified IIC device register.
*
* @param	BaseAddress is the base address of the device.
* @param	RegOffset is the offset from the 1st register of the
*			device to select the specific register.
* @param	RegisterValue is the value to be written to the register.
*
* @return	None.
*
* @note		C-Style signature:
*		void XIic_WriteReg(u32 RegOffset, u32 RegisterValue);
*		This macro does not do any checking to ensure that the
*		register exists if the register may be excluded due to
*		parameterization, such as the GPO Register.
*
******************************************************************************/
u32 fpga_innopmbus_send_data(void __iomem * reg_base, u32 byte_mode, u32 dev_addr,
						u32 value, u32 num_bytes);
u32 fpga_innopmbus_receive_data(void __iomem * reg_base, u32 byte_mode, u32 dev_addr, u32 num_bytes);
u32 fpga_innopmbus_hdmi_receive_data(void __iomem * reg_base, u32 reg_addr, u32 num_bytes);

void __iomem *innopmbus_base(u32 id);

extern struct platform_driver g_inno_pmbus_driver;

#endif //__INNODPU_PMBUS_H__
