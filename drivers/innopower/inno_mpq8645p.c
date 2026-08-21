#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include "innopower.h"

#define CMD_PAGE                       0x00
#define CMD_VOUT                       0x21
#define CMD_VOUT_MARGIN_HIGH           0x25
#define CMD_VOUT_MARGIN_LOW            0x26
#define CMD_VOUT_SCALE_LOOP            0x29
#define CMD_READ_VIN                   0x88
#define CMD_READ_VOUT                  0x8b
#define CMD_READ_IOUT                  0x8c
#define CMD_READ_TEMP                  0x8d
#define CMD_STATUS_VOUT                0x7a
#define CMD_STATUS_IOUT                0x7b
#define CMD_STATUS_INPUT               0x7c

#define CMD_PMBUS_REVISION             0x98
#define CMD_MFR_REVISION               0x9B

#define MPQ8645P_READ16(reg)           innopwr_read16(pwrchip, (reg))
#define MPQ8645P_READ8(reg)            innopwr_read8(pwrchip, (reg))
#define MPQ8645P_WRITE8(reg, val)      innopwr_write8(pwrchip, (reg), (val))
#define MPQ8645P_WRITE16(reg, val)     innopwr_write16(pwrchip, (reg), (val))

#define MPQ8645P_INPUT_VOLT            (MPQ8645P_READ16(CMD_READ_VIN)  & 0x3FF)
#define MPQ8645P_OUTPUT_VOLT           (MPQ8645P_READ16(CMD_READ_VOUT) & 0x1FFF)
#define MPQ8645P_OUTPUT_CURRENT        (MPQ8645P_READ16(CMD_READ_IOUT) & 0x3FFF)
#define MPQ8645P_TEMPERATURE           (MPQ8645P_READ16(CMD_READ_TEMP) & 0x3FF)

// The following transfer algorithm is got from hardware team
#define MPQ8645P_RVOUT_TO_VOLT(reg_val)  (((reg_val) == 0) ? 0 : ((reg_val)*5/4))    // reg_val read from READ_VOUD(8bh) register transfer into vlot
#define MPQ8645P_VOLT_TO_CVOUT(volt)     (((volt) == 0) ? 0 : (((volt) + 10) / 2))   // volt transfer into reg_val for VOUT_COMMAND(21h) register

#define E_MPQ8645P_FAIL	-1
#define E_MPQ8645P_OK	0

#ifndef uint
typedef  unsigned int uint;
#endif

static int mpq8645p_set_voltage(struct powerchip *pwrchip, int channel, unsigned int volt)
{
	uint reg_val = 0;
	(void)channel;

	reg_val = MPQ8645P_VOLT_TO_CVOUT(volt) & 0xFFF;

	MPQ8645P_WRITE16(CMD_VOUT, reg_val);
	reg_val = MPQ8645P_READ16(CMD_VOUT);

	return 0;
}

static int mpq8645p_get_voltage(struct powerchip *pwrchip, int channel)
{
	uint output_volt = 0;
	uint reg_val = 0;
	(void)channel;

	reg_val = MPQ8645P_OUTPUT_VOLT;
	output_volt = MPQ8645P_RVOUT_TO_VOLT(reg_val);
	innopwr_dbg("reg_val=%#x, output_volt=%u\n", reg_val, output_volt);

	return output_volt;
}

static int mpq8645p_hw_init(struct powerchip *pwrchip)
{
	MPQ8645P_WRITE16(CMD_VOUT_SCALE_LOOP, 0x2BC);
	MPQ8645P_WRITE16(CMD_VOUT_MARGIN_LOW, 0x15E);

	return E_MPQ8645P_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int mpq8645p_i2c_probe(struct i2c_client *client)
#else
static int mpq8645p_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret = 0;
	struct powerchip *pwrchip = NULL;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = mpq8645p_set_voltage;
	pwrchip->get_gpu_voltage = mpq8645p_get_voltage;
	ret = mpq8645p_hw_init(pwrchip);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void mpq8645p_i2c_remove(struct i2c_client *client)
#else
static int mpq8645p_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver mpq8645p_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = mpq8645p_i2c_probe,
	.remove = mpq8645p_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};

