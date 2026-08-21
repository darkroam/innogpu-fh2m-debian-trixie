/*
	innopmbus_drv.h: SMBus (i2c) adapter for inno pmbus

	Copyright (C) Innosilicon Technology Ltd. All Rights Reserved
				Derived from CHIPS&MEDIA DDK
	COPYRIGHT (C) 2020 CHIPS&MEDIA INC. ALL RIGHTS RESERVED

	This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
	SPDX License Identifier: BSD-3-Clause
	SPDX License Identifier: LGPL-2.1-only
*/

#ifndef INNOPMBUS_DRV_H
#define INNOPMBUS_DRV_H

#include <linux/i2c.h>
#include "hal_interface.h"

struct inno_pmbus_dev {
	struct device *dev;
	void __iomem  *base;
	struct i2c_adapter adap;
	struct pmbus_funcs pmbus_funcs;

	/* compatibility design:
	 * i2c chip client whether need repeat start
	 * set by chip sequeue
	 */
	bool chip_need_rs;
	struct mutex chip_rs_lock;
	u64 idle_timeout_count;
	u64 checkerr_count;

	/* whether pmbus is enabled */
	bool enable;
};

#ifdef CONFIG_DRM_INNO_PMBUS
int innopmbus_driver_register(void);
void innopmbus_driver_unregister(void);
#endif

#endif

