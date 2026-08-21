#include "hal.h"
#include "power_hw_info.h"
#include "inno_debug.h"

extern struct i2c_driver mp2979_i2c_driver;
extern struct i2c_driver xdpe12284_i2c_driver;
extern struct i2c_driver mpq8645p_i2c_driver;
extern struct i2c_driver is6608a_i2c_driver;
extern struct i2c_driver gs8601_i2c_driver;
extern struct i2c_driver virture_chip_i2c_driver;

struct power_drvobj_info {
	char *chip_name;
	struct i2c_driver *drv;
};

/* chip_name can be a powerchip type, it can stand for a series of powerchip
 * eg. GS8601 can stand for a sub chip type GS8601TDR, however the hwinfo name is GS8601TDR */
struct power_drvobj_info drvobjs[] = {
	{"XDPE12284C",  &xdpe12284_i2c_driver},
	{"MP2979A",     &mp2979_i2c_driver},
	{"MPQ8645PGVT", &mpq8645p_i2c_driver},
	{"IS6608A",     &is6608a_i2c_driver},
	{"VL6608A",     &is6608a_i2c_driver}, /*the register operations of vl6608a and is6608a are the same*/
	{"GS8601",      &gs8601_i2c_driver},
	{"VIRTURE_CHIP",&virture_chip_i2c_driver},
};

struct i2c_driver *innopower_get_drvobj(char *chip_name)
{
	int i = 0;

	for (i = 0; i < INNO_ARRAY_SIZE(drvobjs); i++) {
		if (0 == fh2m_inno_strncmp(chip_name, drvobjs[i].chip_name, fh2m_inno_strlen(drvobjs[i].chip_name))) {
			return drvobjs[i].drv;
		}
	}
	inno_info("%s not matched drv, use VIRTURE_CHIP\n", __func__);

	return &virture_chip_i2c_driver;
}

