#ifndef __INNO_POWER_HW_INFO_H__
#define __INNO_POWER_HW_INFO_H__

#define INNO_POWER_CHIP_NAME	"innopower"

struct i2c_driver *innopower_get_drvobj(char *chip_name);

#endif
