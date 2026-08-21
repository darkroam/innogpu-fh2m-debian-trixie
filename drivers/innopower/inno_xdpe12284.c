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

#define XDPE_PAGE 0x00
#define XDPE_OPERATION_VOUT 0x01
#define XDPE_CMD_VOUT 0x21
#define XDPE_READ_VOUT 0x8B
#define XDPE_READ_IOUT 0x8C

#define XDPE12284_READ_word(cmd)          innopwr_read16(pwrchip, (cmd))
#define XDPE12284_READ_byte(cmd)          innopwr_read8(pwrchip, (cmd))
#define XDPE12284_WRITE_byte(cmd, val)    innopwr_write8(pwrchip, (cmd), (val))
#define XDPE12284_WRITE_word(cmd, val)    innopwr_write16(pwrchip, (cmd), (val))

#define E_XDPE12284_FAIL                  -1
#define E_XDPE12284_OK                    0

static uint32_t vid_to_mv(uint32_t vid){ return vid < 1 ? 0 : (((vid-1)*5+250));}
static int mv_to_vid(int mv){ return !mv ? 0 : ((mv-250)/5+2);}
static uint32_t __xdpe12284_set_vout_vid(struct powerchip *pwrchip, uint32_t channel, uint32_t vid)
{
	XDPE12284_WRITE_byte(XDPE_PAGE, channel);
	XDPE12284_WRITE_byte(XDPE_OPERATION_VOUT, 0x80);
	XDPE12284_WRITE_word(XDPE_CMD_VOUT, vid);

	return 0;
}

static uint32_t __xdpe12284_get_vout_vid(struct powerchip *pwrchip, uint32_t channel)
{
	XDPE12284_WRITE_byte(XDPE_PAGE, channel);
	return XDPE12284_READ_word(XDPE_READ_VOUT);
}

static void __xdpe12284_set_voltage(struct powerchip *pwrchip, uint32_t channel, uint32_t mv)
{
	uint32_t vid = mv_to_vid(mv);
	__xdpe12284_set_vout_vid(pwrchip, channel, vid);
}

static int xdpe12284_get_voltage(struct powerchip *pwrchip, int channel)
{
	int voltage = 0;
	uint32_t vid = __xdpe12284_get_vout_vid(pwrchip, channel);
	voltage = vid_to_mv(vid);
	innopwr_dbg("vid = %u voltage = %d\n", vid, voltage);

	return voltage;
}

static uint32_t __maybe_unused xdpe12284_get_iout_literal(struct powerchip *pwrchip, uint32_t channel)
{
	XDPE12284_WRITE_byte(XDPE_PAGE, channel);
	return XDPE12284_READ_word(XDPE_READ_IOUT);
}

static void __maybe_unused xdpe12284_close_voltage(struct powerchip *pwrchip, uint32_t channel)
{
	XDPE12284_WRITE_byte(XDPE_PAGE, channel);
	XDPE12284_WRITE_word(XDPE_OPERATION_VOUT, 0x0);
	return ;
}

static int xdpe12284_set_voltage(struct powerchip *pwrchip, int channel, unsigned int vol)
{
	__xdpe12284_set_voltage(pwrchip, channel, vol);

	return 0;
}

static int xdpe12284_hw_init(struct powerchip *pwrchip)
{
	/*mcufw have init done*/

	return E_XDPE12284_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int xdpe12284_i2c_probe(struct i2c_client *client)
#else
static int xdpe12284_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret;
	struct powerchip *pwrchip;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = xdpe12284_set_voltage;
	pwrchip->get_gpu_voltage = xdpe12284_get_voltage;

	ret = xdpe12284_hw_init(pwrchip);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void xdpe12284_i2c_remove(struct i2c_client *client)
#else
static int xdpe12284_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver xdpe12284_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = xdpe12284_i2c_probe,
	.remove = xdpe12284_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};

