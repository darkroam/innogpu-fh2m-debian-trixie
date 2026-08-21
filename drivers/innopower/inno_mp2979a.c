#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include "innopower.h"

#define CMD_PAGE                     0x00
#define CMD_VOUT                     0x21
#define CMD_READ_VIN                 0x88
#define CMD_READ_VOUT                0x8b
#define CMD_READ_IOUT                0x8c
#define CMD_READ_TEMP                0x8d
#define CMD_READ_POUT                0x96
#define CMD_READ_PIN                 0x97
#define CMD_STATUS_VOUT              0x7a
#define CMD_STATUS_IOUT              0x7b
#define CMD_STATUS_INPUT             0x7c
#define MFR_VR_CONFIG                0xe4

#define MP2979_READ16(cmd)           innopwr_read16_no_repeat_start(pwrchip, (cmd))
#define MP2979_READ8(cmd)            innopwr_read8_no_repeat_start(pwrchip, (cmd))
#define MP2979_WRITE8(cmd, val)      innopwr_write8_no_repeat_start(pwrchip, (cmd), (val))
#define MP2979_WRITE16(cmd, val)     innopwr_write16_no_repeat_start(pwrchip, (cmd), (val))

#define MP2979_DIV_TO_VOLT(div)      (((div) == 0) ? 0 : (((div) - 1) * 5 + 250))
#define MP2979_VOLT_TO_DIV(volt)     (((volt) == 0) ? 0 : (((volt) - 250) / 5 + 1))
#define MP2979_SWITCH_RAIL_OP(rail)  MP2979_WRITE8(CMD_PAGE, rail)

#define MP2979_INPUT_VOLT            (MP2979_READ16(CMD_READ_VIN)  & 0xFF)
#define MP2979_INPUT_POWER           (MP2979_READ16(CMD_READ_PIN)  & 0xFF)
#define MP2979_OUTPUT_VOLT           (MP2979_READ16(CMD_READ_VOUT) & 0xFF)
#define MP2979_OUTPUT_CURRENT        (MP2979_READ16(CMD_READ_IOUT) & 0x7FF)
#define MP2979_OUTPUT_POWER          (MP2979_READ16(CMD_READ_POUT) & 0x1FF)
#define MP2979_TEMPERATURE           (MP2979_READ16(CMD_READ_TEMP) & 0xFF)

#define E_MP2679_FAIL	-1
#define E_MP2678_OK	0

#ifndef uint
typedef  unsigned int uint;
#endif

/* G0_HHSB4上VDD_GPU属于RAILA, VDD_CORE属于RAILB */
enum mp2979_rail {
    MP2979_RAIL_A,
    MP2979_RAIL_B,
    MP2979_RAIL_C,
};

struct mp2979_rail_power {
    uint input_volt;     /* mV */
    uint input_power;    /* mW */
    uint output_volt;    /* mV */
    uint output_current; /* mA */
    uint output_power;   /* mW */
    uint temperature;    /* C  */
};

static void mp2979_get_rail_power(struct powerchip *pwrchip, enum mp2979_rail rail, struct mp2979_rail_power *power)
{
	if (power == NULL) {
		innopwr_notice("power is null\n");
		return;
	}

	MP2979_SWITCH_RAIL_OP(MP2979_RAIL_A);
	power->input_volt = MP2979_INPUT_VOLT * 125;
	power->input_power = MP2979_INPUT_POWER * 500;
	power->temperature = MP2979_TEMPERATURE;

	MP2979_SWITCH_RAIL_OP(rail);
	power->output_volt = MP2979_DIV_TO_VOLT(MP2979_OUTPUT_VOLT);
	power->output_current = MP2979_OUTPUT_CURRENT * 250;
	power->output_power = MP2979_OUTPUT_POWER * 1000;

	innopwr_dbg("power[%u %u %u]\n", power->output_volt , power->output_current , power->output_power);
}

static void mp2979_set_rail_output_volt(struct powerchip *pwrchip, enum mp2979_rail rail, uint volt)
{
	uint div = MP2979_VOLT_TO_DIV(volt);
	MP2979_SWITCH_RAIL_OP(rail);

	MP2979_WRITE16(CMD_VOUT, div);
	div = MP2979_READ16(CMD_VOUT);
}

static int mp2979_set_voltage(struct powerchip *pwrchip, int channel, unsigned int vol)
{
	mp2979_set_rail_output_volt(pwrchip, MP2979_RAIL_A, vol);

	return 0;
}

static int mp2979_get_voltage(struct powerchip *pwrchip, int channel)
{
	struct mp2979_rail_power powerinfo;

	mp2979_get_rail_power(pwrchip, MP2979_RAIL_A, &powerinfo);

	return powerinfo.output_volt;
}

static int mp2979_hw_init(struct powerchip *pwrchip)
{
	/*mcufw have init done*/

	return E_MP2678_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int mp2979_i2c_probe(struct i2c_client *client)
#else
static int mp2979_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret;
	struct powerchip *pwrchip;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = mp2979_set_voltage;
	pwrchip->get_gpu_voltage = mp2979_get_voltage;
	ret = mp2979_hw_init(pwrchip);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void mp2979_i2c_remove(struct i2c_client *client)
#else
static int mp2979_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver mp2979_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = mp2979_i2c_probe,
	.remove = mp2979_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};

