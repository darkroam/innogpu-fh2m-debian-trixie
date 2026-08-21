#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include "innopower.h"

#ifndef uint
typedef  unsigned int uint;
#endif

#ifndef uchar
typedef  unsigned char uchar;
#endif

#define GS8601_READ_word(cmd)          innopwr_read16(pwrchip, (cmd))
#define GS8601_READ_byte(cmd)          innopwr_read8(pwrchip, (cmd))
#define GS8601_WRITE_byte(cmd, val)    innopwr_write8(pwrchip, (cmd), (val))
#define GS8601_WRITE_word(cmd, val)    innopwr_write16(pwrchip, (cmd), (val))

#define E_GS8601_FAIL                  -1
#define E_GS8601_OK                    0

#define REG_INI             0x8B    // R,  initial voltage read back(0.75~1.8V)
#define REG_SVI             0xD2    // RW, Vref set by SVI(0.6~1.875V, 5mV increment)
// |-----------|--------|---------------|--------------------|
// |7         6|    5   | 4           2 | 1                0 |
// |-----------|--------|---------------|--------------------|
// | volt_mode | on/off | current limit | read back VID[1:0] |
// |-----------|--------|---------------|--------------------|
#define REG_ILIM            0xD3    // RW, Mode, on/off control

#define VOLT_ON             0       // REG_ILIM bit 5
#define VOLT_OFF            1
#define SVI_VOLT_BIAS       600     // SVI mode bias volt

typedef enum volt_mode {
	E_Initial_Mode = 0,
	E_PVI_Mode     = 1,
	E_SVI_Mode     = 2
} volt_mode_t;

static int gs8601_get_voltage(struct powerchip *pwrchip, int channel)
{
	uint vid = 0;
	int voltage = 0;
	volt_mode_t mode = (volt_mode_t)(GS8601_READ_byte(REG_ILIM) >> 6);

	if (E_Initial_Mode == mode) {
		vid = GS8601_READ_byte(REG_INI);
	} else if (E_SVI_Mode == mode) {
		vid = GS8601_READ_byte(REG_SVI);
	}
	voltage = (int)(vid * 5 + SVI_VOLT_BIAS);

	innopwr_info("get voltage is %d\n", voltage);
	return voltage;
}

static int gs8601_set_voltage(struct powerchip *pwrchip, int channel, unsigned int mv)
{
	uchar ilim = 0;
	uchar vid = (uchar)((mv - SVI_VOLT_BIAS) / 5);

	GS8601_WRITE_byte(REG_SVI, vid);
	msleep(1);

	ilim = GS8601_READ_byte(REG_ILIM);
	ilim = (ilim & 0x3f) | (E_SVI_Mode << 6);
	GS8601_WRITE_byte(REG_ILIM, ilim);
	innopwr_info("set mv is %u\n", mv);

	return 0;
}

static int gs8601_hw_init(struct powerchip *pwrchip)
{
	/*mcufw have init done*/

	return E_GS8601_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int gs8601_i2c_probe(struct i2c_client *client)
#else
static int gs8601_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret;
	struct powerchip *pwrchip;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = gs8601_set_voltage;
	pwrchip->get_gpu_voltage = gs8601_get_voltage;

	ret = gs8601_hw_init(pwrchip);

	innopwr_info("detect successfully\n");

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void gs8601_i2c_remove(struct i2c_client *client)
#else
static int gs8601_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver gs8601_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = gs8601_i2c_probe,
	.remove = gs8601_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};

