/*************************************************************************/ /*!
@File			gen_g3_ne_hdmi.c
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

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
CONNECTION WITH THE SOFTWAcRE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#include "gen_g3_ne_hdmi.h"

extern int s_g3_hdmi_hwi2c;

#define gen_g3_ne_hdmi_write_reg(_ent, _val, _chip) ({ \
			int _x = (_chip)->regops->write((_ent), (_val), (_chip));\
			(_x);})

#define gen_g3_ne_hdmi_read_reg(_ent, _chip) ({ \
			int _x = (_chip)->regops->read((_ent), (_chip));\
			(_x);})

__attribute__ ((unused))
int gen_g3_ne_hdmi_regops_init(struct hdmi_chip_t *chip, const struct hdmi_chip_regops_t *regops)
{
	chip->regops = regops;
	return 0;
}

__attribute__ ((unused))
void gen_g3_ne_hdmi_regops_fini(struct hdmi_chip_t *chip)
{
	chip->regops = NULL;
}

static void gen_g3_ne_hdmi_gpio_set_sda(void *data, int state)
{
	struct hdmi_chip_t *chip = data;
	u32 value = 0;

	value = gen_g3_ne_hdmi_read_reg(REG_ENTITY0074, chip); //0x4a
	value &= ~(BIT(6));
	value |= state << 6;

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0074, value, chip);
}

static void gen_g3_ne_hdmi_gpio_set_scl(void *data, int state)
{
	struct hdmi_chip_t *chip = data;
	u32 value = 0;

	value = gen_g3_ne_hdmi_read_reg(REG_ENTITY0074, chip);
	value &= ~(BIT(5));
	value |= state << 5;

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0074, value, chip);
}

static int gen_g3_ne_hdmi_gpio_get_sda(void *data)
{
	struct hdmi_chip_t *chip = data;
	u32 value = 0;

	value = gen_g3_ne_hdmi_read_reg(REG_ENTITY0074, chip);
	return value & BIT(1);
}

static int gen_g3_ne_hdmi_gpio_get_scl(void *data)
{
	struct hdmi_chip_t *chip = data;
	u32 value = 0;

	value = gen_g3_ne_hdmi_read_reg(REG_ENTITY0074, chip);
	return value & BIT(0);
}

static void gen_g3_ne_hdmi_gpio_post_xfer(struct i2c_adapter *adapter)
{
	struct i2c_algo_bit_data *adap = adapter->algo_data;
	struct hdmi_chip_t *chip = adap->data;

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0074, 0x70, chip);
	fh2m_inno_udelay(20);
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0074, 0x63, chip);
	fh2m_inno_mutex_unlock(chip->chipi2c->mutex);
}

static int gen_g3_ne_hdmi_gpio_pre_xfer(struct i2c_adapter *adapter)
{
	struct i2c_algo_bit_data *adap = adapter->algo_data;
	struct hdmi_chip_t *chip = adap->data;

	fh2m_inno_mutex_lock(chip->chipi2c->mutex);
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0074, 0x70, chip);
	fh2m_inno_udelay(20);
	return 0;
}

void gen_g3_ne_hdmi_ddc_init(struct hdmi_chip_t *chip)
{
	int cbus_clk;
	int ddc_bus_clk;
	int ddc_bus_high_clk;

	return; //fpga 15MHz，use default value 4b-0x44 4c-0x00
	cbus_clk = fh2m_hal_get_pll(chip->parent, PLL_CBUS); //sys clk 100MHz
	if (!cbus_clk) {
		fh2m_innodpu_warn(chip->dev, "get cbus clk is 0, used 128M default\n");
		cbus_clk = 128;
	}

	/* ddc iic frequency = 100khz, so divide 100 */
	ddc_bus_clk = ((cbus_clk * 1000) / 100) >> 4;
	ddc_bus_high_clk = ddc_bus_clk >> 8 & 0xff;
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "cbus clock is %dMHz, ddc_bus_clk/100K is %#x!\n", \
			cbus_clk, ddc_bus_clk);

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0075, ddc_bus_clk, chip); //0x4b LSB
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0076, ddc_bus_high_clk, chip); //0x4c MSB
}

#define DIRCT_WRITE   (0)
#define DIRCT_READ    (1)
#define I2C_M_RD      0x0001
#define G3_HDMI_HWI2C_TIMOUT  (500)
static int gen_g3_ne_hdmi_hwi2c_transfer(struct hdmi_chip_t *chip, u16 addr, u8 *buf, u16 len)
{
	int ret = 0;
	int i = 0;
	int retry = 100;
	int dirct = (addr & 0x1);
	u32 val = 0;
	struct hdmi_chip_i2c_t *chipi2c = chip->chipi2c;

	do {
		retry -= 1;
		val = gen_g3_ne_hdmi_read_reg(REG_ENTITY0333, chip); //0x14d
		if (val & 0x10)
			break;
		fh2m_inno_mdelay(1);
	} while(retry);
	if (!retry) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hwi2c idle timeout\n");
		return -EAGAIN;
	}

	chipi2c->hwi2c_status = 0;
	fh2m_inno_reinit_completion(chipi2c->hwi2c_comp);

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0330, addr, chip); //0x14a dev addr

	if (DIRCT_READ == dirct) {
		gen_g3_ne_hdmi_write_reg(REG_ENTITY0332, len, chip);  //0x14c read len
	} else {
		gen_g3_ne_hdmi_write_reg(REG_ENTITY0331, len, chip);  //0x14b write len
	}

	if (DIRCT_WRITE == dirct) {
		for (i = 0; i < len; i++)
			gen_g3_ne_hdmi_write_reg(REG_ENTITY0334, buf[i], chip); //0x14e write fifo
	}

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0333, 0xcc, chip); //0x14d trigger

	ret = fh2m_inno_wait_for_completion_timeout(chipi2c->hwi2c_comp, \
			fh2m_inno_msecs_to_jiffies(G3_HDMI_HWI2C_TIMOUT));
	if (!ret) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hwi2c wait timeout\n");
		return -EAGAIN;
	}

	if (chipi2c->hwi2c_status & BIT(0)) {
		return -EIO;
	}

	if (DIRCT_READ == dirct) {
		for (i = 0; i < len; i++)
			buf[i] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0335, chip); //0x14f read fifo
	}

	return 0;
}

static int gen_g3_ne_hdmi_hwi2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct hdmi_chip_t *chip = container_of(adap, struct hdmi_chip_t, hwi2c_adapter);
	int i, ret;
	u16 addr;
	u8 *buf;
	u16 len;

	fh2m_inno_mutex_lock(chip->chipi2c->mutex);

	gen_g3_ne_hdmi_ddc_init(chip);

	for (i = 0; i < num; i++) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hwi2c xfer: num: %d/%d, addr:0x%02x, len: %d, flags: 0x%04x.\n",
				i + 1, num, msgs[i].addr, msgs[i].len, msgs[i].flags);

		addr = (msgs[i].flags & I2C_M_RD) ? ((msgs[i].addr<<1) | 0x1) : (msgs[i].addr<<1);
		buf = msgs[i].buf;
		len = msgs[i].len;

		ret = gen_g3_ne_hdmi_hwi2c_transfer(chip, addr, buf, len);

		if (ret < 0)
			break;
	}

	if (!ret) {
		ret = num;
	}

	fh2m_inno_mutex_unlock(chip->chipi2c->mutex);

	return ret;
}

static u32 gen_g3_ne_hdmi_hwi2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm gen_g3_ne_hdmi_algorithm = {
	.master_xfer = gen_g3_ne_hdmi_hwi2c_xfer,
	.functionality = gen_g3_ne_hdmi_hwi2c_func,
};

int gen_g3_ne_hdmi_i2c_init(struct hdmi_chip_t *chip)
{
	struct hdmi_chip_i2c_t *chipi2c = NULL;

	struct i2c_adapter *hwi2c_adap;
	struct i2c_adapter *bit_adap;
	struct i2c_algo_bit_data *bit_data;

	chipi2c = kzalloc(sizeof(struct hdmi_chip_i2c_t), GFP_KERNEL);
	if (!chipi2c)
		return -ENOMEM;
	chip->chipi2c = chipi2c;

	chipi2c->mutex = fh2m_inno_mutex_alloc();
	if (!chipi2c->mutex)
		goto free_chipi2c;

	chipi2c->edid_comp = fh2m_inno_completion_alloc();
	if (!chipi2c->edid_comp)
		goto free_mutex;

	chipi2c->scdc_comp = fh2m_inno_completion_alloc();
	if (!chipi2c->scdc_comp)
		goto free_edid_comp;

	chipi2c->hwi2c_comp = fh2m_inno_completion_alloc();
	if (!chipi2c->hwi2c_comp)
		goto free_scdc_comp;

	hwi2c_adap = &chip->hwi2c_adapter;
	bit_adap = &chip->bit_adapter;
	bit_data = &chip->bit_data;

	/* 1 hwi2c adapter register */
	hwi2c_adap->owner = THIS_MODULE;
	snprintf(hwi2c_adap->name, sizeof(hwi2c_adap->name), "%s-hwi2c", chip->name);
	hwi2c_adap->algo = &gen_g3_ne_hdmi_algorithm;
#if (DRM_VERSION < KERNEL_VERSION(6, 8, 0))
	hwi2c_adap->class = I2C_CLASS_DDC;
#endif
	hwi2c_adap->dev.parent = chip->dev;
	hwi2c_adap->dev.of_node = fh2m_inno_get_dev_ofnode(chip->dev);
	hwi2c_adap->nr = -1;

	if (i2c_add_adapter(hwi2c_adap)) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "hwi2c register fail\n");
		goto free_hwi2c_comp;
	}

	/* 2 biti2c adapter register */
	bit_data->setsda = gen_g3_ne_hdmi_gpio_set_sda;
	bit_data->setscl = gen_g3_ne_hdmi_gpio_set_scl;
	bit_data->getscl = gen_g3_ne_hdmi_gpio_get_scl;
	bit_data->getsda = gen_g3_ne_hdmi_gpio_get_sda;
	bit_data->pre_xfer = gen_g3_ne_hdmi_gpio_pre_xfer;
	bit_data->post_xfer = gen_g3_ne_hdmi_gpio_post_xfer;
	bit_data->udelay = 10;
	bit_data->timeout = usecs_to_jiffies(2200);
	bit_data->data = chip;

	bit_adap->owner = THIS_MODULE;
	snprintf(bit_adap->name, sizeof(bit_adap->name), "%s-biti2c", chip->name);
	bit_adap->algo_data = bit_data;
	bit_adap->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	bit_adap->dev.parent = chip->dev;
	bit_adap->dev.of_node = fh2m_inno_get_dev_ofnode(chip->dev);
	bit_adap->nr = -1;

	if (i2c_bit_add_numbered_bus(bit_adap)) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "biti2c register fail\n");
		goto free_hwi2c_comp;
	}

	return 0;

free_hwi2c_comp:
	fh2m_inno_completion_free(chipi2c->hwi2c_comp);
free_scdc_comp:
	fh2m_inno_completion_free(chipi2c->scdc_comp);
free_edid_comp:
	fh2m_inno_completion_free(chipi2c->edid_comp);
free_mutex:
	fh2m_inno_mutex_free(chipi2c->mutex);
free_chipi2c:
	fh2m_inno_kfree(chipi2c);

	return -EINVAL;
}

void gen_g3_ne_hdmi_i2c_fini(struct hdmi_chip_t *chip)
{
	struct hdmi_chip_i2c_t *chipi2c = chip->chipi2c;

	fh2m_inno_mutex_lock(chipi2c->mutex);
	i2c_del_adapter(&chip->bit_adapter);
	i2c_del_adapter(&chip->hwi2c_adapter);
	fh2m_inno_mutex_unlock(chipi2c->mutex);

	if (chipi2c->hwi2c_comp)
		fh2m_inno_completion_free(chipi2c->hwi2c_comp);

	if (chipi2c->scdc_comp)
		fh2m_inno_completion_free(chipi2c->scdc_comp);

	if (chipi2c->edid_comp)
		fh2m_inno_completion_free(chipi2c->edid_comp);

	if (chipi2c->mutex)
		fh2m_inno_mutex_free(chipi2c->mutex);

	if (chipi2c)
		fh2m_inno_kfree(chip->chipi2c);

	chip->chipi2c = NULL;
}

#define G3_HDMI_SCDC_TIMOUT  (500)
static int gen_g3_ne_hdmi_scdc_rwsink(struct hdmi_chip_t *chip, u32 addr, u32 reg,
		u8 *val, int cnt)
{
	int ret = 0;
	u8  regarray[8] = {0};
	int dirct = (addr & 0x1);
	struct hdmi_chip_i2c_t *chipi2c = chip->chipi2c;

	if ((addr != 0xa8) && (addr != 0xa9)) {
		return -EINVAL;
	}

	if (DIRCT_READ == dirct) {
		if (cnt <= 0 || cnt > 8) {
			return -EINVAL;
		}
	} else {
		cnt = 1;
	}

	if (!val) {
		return -EINVAL;
	}

	fh2m_inno_reinit_completion(chipi2c->scdc_comp);
	fh2m_inno_mutex_lock(chip->chipi2c->mutex);

	gen_g3_ne_hdmi_ddc_init(chip);
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0256, 0x1, chip); //0x100 enable hdmi2.0 mode

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0264, 0x3, chip); //0x108
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0265, 0x3, chip); //0x109

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0276, addr, chip); //0x114
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0277, reg, chip); //0x115
	if (DIRCT_WRITE == dirct) {
		gen_g3_ne_hdmi_write_reg(REG_ENTITY0278, (u32)val[0], chip); //0x116
	}
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0279, cnt, chip); //0x117
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0275, 0x01, chip); //0x113

	// wait scdc read done interrupt
	ret = fh2m_inno_wait_for_completion_timeout(chipi2c->scdc_comp, \
		fh2m_inno_msecs_to_jiffies(G3_HDMI_SCDC_TIMOUT));
	if (ret < 0) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "scdc wait failed(ret:%d)\n", ret);
		ret = -EIO;
		goto out;
	} else if (ret == 0) { //condition false after timeout
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "scdc wait timeout\n");
		ret = -EIO;
		goto out;
	}
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "scdc wait ok\n");

	if (DIRCT_READ == dirct) {
		regarray[7] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0287, chip); //0x11f
		regarray[6] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0286, chip); //0x11e
		regarray[5] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0285, chip); //0x11d
		regarray[4] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0284, chip); //0x11c
		regarray[3] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0283, chip); //0x11b
		regarray[2] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0282, chip); //0x11a
		regarray[1] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0281, chip); //0x119
		regarray[0] = (u8)gen_g3_ne_hdmi_read_reg(REG_ENTITY0280, chip); //0x118

		fh2m_inno_memcpy(val, regarray, cnt);
	}

	ret = 0;
out:
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0264, 0x00, chip); //0x108
	fh2m_inno_mutex_unlock(chip->chipi2c->mutex);

	return ret;
}

ssize_t gen_g3_ne_hdmi_scdc_read(struct hdmi_chip_t *chip, u8 offset,
                                void *buffer, size_t size)
{
	switch (s_g3_hdmi_hwi2c) {
	case 0x1:
		return gen_g3_ne_hdmi_scdc_rwsink(chip, 0xa9, offset, buffer, size);
	case 0x2:
		return fh2m_inno_drm_scdc_read(&chip->hwi2c_adapter, offset, buffer, size);
	}

	/* default gpio i2c */
	return fh2m_inno_drm_scdc_read(&chip->bit_adapter, offset, buffer, size);
}

ssize_t gen_g3_ne_hdmi_scdc_write(struct hdmi_chip_t *chip, u8 offset,
                                void *buffer, size_t size)
{
	switch (s_g3_hdmi_hwi2c) {
	case 0x1:
		return gen_g3_ne_hdmi_scdc_rwsink(chip, 0xa8, offset, buffer, size);
	case 0x2:
		return fh2m_inno_drm_scdc_write(&chip->hwi2c_adapter, offset, buffer, size);
	}

	/* default gpio i2c */
	return fh2m_inno_drm_scdc_write(&chip->bit_adapter, offset, buffer, size);
}

#define G3_HDMI_EDID_TIMOUT   (200)
static int gen_g3_ne_hdmi_edid_controller_read_block(struct hdmi_chip_t *chip, u32 block, u8 *buff)
{
	int ret = 0;
	u32 cnt = 0;
	static const u32 edid_block[][2] = {
		//segment, offset
		{0x00, 0x00,}, //block0
		{0x00, 0x80,}, //block1
		{0x01, 0x00,}, //block2
		{0x01, 0x80,}, //block3
	};
	struct hdmi_chip_i2c_t *chipi2c = chip->chipi2c;

	if (block >= ARRAY_SIZE(edid_block)) {
		return -EINVAL;
	}

	fh2m_inno_reinit_completion(chipi2c->edid_comp);
	fh2m_inno_mutex_lock(chip->chipi2c->mutex);

	gen_g3_ne_hdmi_ddc_init(chip);

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0192, 0x04, chip); //0xc0
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0193, 0x04, chip); //0xc1

	gen_g3_ne_hdmi_write_reg(REG_ENTITY0079, 0x00, chip); //0x4f
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0078, edid_block[block][1], chip); //0x4e
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0077, edid_block[block][0], chip); //0x4d

	// wait edid done interrupt
	ret = fh2m_inno_wait_for_completion_timeout(chipi2c->edid_comp, \
		fh2m_inno_msecs_to_jiffies(G3_HDMI_EDID_TIMOUT));
	if (ret < 0) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid wait failed(ret:%d)\n", ret);
		ret = -EIO;
		goto out;
	} else if (ret == 0) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid wait timeout\n");
		ret = -EIO;
		goto out;
	}
	fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid wait ok\n");

	for (cnt = 0; cnt < EDID_LENGTH; cnt++) {
		buff[cnt] = gen_g3_ne_hdmi_read_reg(REG_ENTITY0080, chip); //0x50
		fh2m_inno_udelay(1);
	}

	if (fh2m_inno_drm_edid_block_checksum(buff)) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "edid checksum failed\n");
		ret = -EIO;
		goto out;
	}

	ret = 0;
out:
	gen_g3_ne_hdmi_write_reg(REG_ENTITY0192, 0x00, chip); //0xc0
	fh2m_inno_mutex_unlock(chip->chipi2c->mutex);

	return ret;
}

static int gen_g3_ne_hdmi_edid_read_block(struct hdmi_chip_t *chip, u32 block, u8 *buff)
{
	int ret = 0;

	if (s_g3_hdmi_hwi2c & BIT(1))
		ret = fh2m_inno_drm_do_probe_ddc_edid(&chip->hwi2c_adapter, buff, block, EDID_LENGTH);
	else if (s_g3_hdmi_hwi2c & BIT(2))
		ret = fh2m_inno_drm_do_probe_ddc_edid(&chip->bit_adapter, buff, block, EDID_LENGTH);
	if (!ret)
		goto out;

	if (fh2m_inno_drm_edid_block_checksum(buff)) {
		ret = -2;
		goto out;
	}

	if ((block == 0) && fh2m_inno_drm_edid_header_is_valid(buff) < 6) {
		ret = -3;
		goto out;
	}

out:
	return ret;
}

int gen_g3_ne_hdmi_edid_read(struct hdmi_chip_t *chip)
{
	int ret = 0;
	int cnt = 0;
	int block = 0;
	int total_block = 2;
	u8 buf[EDID_LENGTH] = {0x55};

	if (!chip)
		return -EINVAL;

	do {
		fh2m_inno_memset(buf, 0x55, EDID_LENGTH);

		if (s_g3_hdmi_hwi2c & BIT(0))
			ret = gen_g3_ne_hdmi_edid_controller_read_block(chip, block, buf);
		else
			ret = gen_g3_ne_hdmi_edid_read_block(chip, block, buf);
		if (ret) {
			fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "read block%d failed, ret:%d\n", block, ret);
			return cnt;
		}

		fh2m_inno_memcpy(chip->edid_buf+cnt, buf, EDID_LENGTH);
		cnt += EDID_LENGTH;

		if (block == 0) {
			total_block = buf[EDID_LENGTH-2] + 1;
			fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "block0 extension:%d\n", buf[EDID_LENGTH-2]);

			if (total_block > 4) {
				total_block = 4;
				fh2m_innodpu_err(chip->dev, "the sink edid blocks number more than 4.\n");
			}
		}

		block++;
	}while(block < total_block);

	if (fh2m_inno_drm_edid_header_is_valid(chip->edid_buf) < 6) {
		fh2m_innodpu_info(chip->dev, DPU_UT_HDMI, "header is valid\n");
		return -EINVAL;
	}

	return cnt;
}
