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

#define IS_REG_OPERATION            0x01
#define IS_REG_ON_OFF_CONFIG        0x02
#define IS_REG_VOUT_COMMAND         0x21
#define IS_REG_VOUT_SCALE_LOOP      0x29
#define IS_REG_READ_VOUT            0x8B
#define IS_REG_READ_IOUT            0x8C
#define IS_REG_READ_TEMP            0x8D

#define is6608a_read_word(cmd)          innopwr_read16(pwrchip, (cmd))
#define is6608a_read_byte(cmd)          innopwr_read8(pwrchip, (cmd))
#define is6608a_write_byte(cmd, val)    innopwr_write8(pwrchip, (cmd), (val))
#define is6608a_write_word(cmd, val)    innopwr_write16(pwrchip, (cmd), (val))

#define E_IS6608A_FAIL                  -1
#define E_IS6608A_OK                    0

static uint32_t vid_to_mv(uint32_t vid){return ((vid * 1250) / 1000 + 6);}
static unsigned int mv_to_vid(unsigned mv){ return mv <= 0 ? 0 : ((mv)/2);}
#define FB_V_DIV_TO_VID_CONVERT(Rbottom, Rtop) (((Rbottom * 1000) / (Rbottom + Rtop)))
#define FB_V_DIV_TO_VID (FB_V_DIV_TO_VID_CONVERT(1600, 665) & 0xfff)

#define FB_V_DIV_TO_MV_CONVERT(Rbottom, Rtop) ((((Rbottom + Rtop) * 1000) / Rbottom))/1000
#define FB_V_DIV_TO_MV FB_V_DIV_TO_MV_CONVERT(1600, 665)

static int is6608a_get_voltage(struct powerchip *pwrchip, int channel)
{
	int regval = is6608a_read_word(IS_REG_READ_VOUT);
	int mv = 0;

	if (regval != E_IS6608A_FAIL) {
		regval &= 0x1fff;
		mv = vid_to_mv((uint)regval) * FB_V_DIV_TO_MV;
		innopwr_info("vid_to_mv(0x%x) = %u mv = %d!!!\n", (uint32_t)regval, vid_to_mv((uint32_t)regval), mv);
		return mv;
	}
	innopwr_err("failed regval = 0x%x!!!\n", regval);
	return E_IS6608A_FAIL;
}

static int is6608a_set_voltage(struct powerchip *pwrchip, int channel, unsigned int mv)
{
	unsigned int regval = 0, reg0 = 0, reg1 = 0;

	regval = FB_V_DIV_TO_VID;
	if (E_IS6608A_OK != is6608a_write_word(IS_REG_VOUT_SCALE_LOOP, regval)) {
		innopwr_err("write IS_REG_VOUT_SCALE_LOOP failed!!!\n");
		return E_IS6608A_FAIL;
	}

	reg0 = is6608a_read_word(IS_REG_VOUT_SCALE_LOOP);
	regval = mv_to_vid(mv);
	if (E_IS6608A_OK != is6608a_write_word(IS_REG_VOUT_COMMAND, regval)) {
		innopwr_err("write IS_REG_VOUT_COMMAND failed reg0=0x%x!!!\n", reg0);
		return E_IS6608A_FAIL;
	}
	reg1 = is6608a_read_word(IS_REG_VOUT_COMMAND);

	innopwr_info("reg0=0x%x reg2=0x%x mv=%u!!!\n", reg0, reg1, mv);

	return E_IS6608A_OK;
}

static int is6608a_hw_init(struct powerchip *pwrchip)
{
	/*mcufw have init done*/

	return E_IS6608A_OK;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int is6608a_i2c_probe(struct i2c_client *client)
#else
static int is6608a_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	int ret;
	struct powerchip *pwrchip;

	pwrchip = i2c_get_clientdata(client);
	mutex_init(&pwrchip->lock);

	pwrchip->set_gpu_voltage = is6608a_set_voltage;
	pwrchip->get_gpu_voltage = is6608a_get_voltage;

	ret = is6608a_hw_init(pwrchip);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void is6608a_i2c_remove(struct i2c_client *client)
#else
static int is6608a_i2c_remove(struct i2c_client *client)
#endif
{
	struct powerchip *pwrchip = i2c_get_clientdata(client);

	mutex_destroy(&pwrchip->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

struct i2c_driver is6608a_i2c_driver = {
	.driver = {
		.name = INNO_POWER_CHIP_NAME,
	},
	.probe = is6608a_i2c_probe,
	.remove = is6608a_i2c_remove,
	.id_table = NULL,	/*dyn generate*/
};
