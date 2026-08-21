#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include "innopower.h"

#define E_VIRTRUE_CHIP_FAIL                  -1
#define E_VIRTRUE_CHIP_OK                    0

static int virture_chip_get_voltage(struct powerchip *pwrchip, int channel)
{
	return 800;
}

static int virture_chip_set_voltage(struct powerchip *pwrchip, int channel, unsigned int vol)
{
	return 0;
}

static int virture_chip_hw_init(struct powerchip *pwrchip)
{
	/*mcufw have init done*/

	return E_VIRTRUE_CHIP_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int virture_chip_i2c_probe(struct i2c_client *client)
#else
static int virture_chip_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret;
	struct powerchip *pwrchip;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = virture_chip_set_voltage;
	pwrchip->get_gpu_voltage = virture_chip_get_voltage;

	ret = virture_chip_hw_init(pwrchip);
	innopwr_info("success ret=%d\n", ret);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void virture_chip_i2c_remove(struct i2c_client *client)
#else
static int virture_chip_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver virture_chip_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = virture_chip_i2c_probe,
	.remove = virture_chip_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};

