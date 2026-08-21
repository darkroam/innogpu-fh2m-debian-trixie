#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "innopmbus_drv.h"
#include "hal_interface.h"
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/thermal.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sizes.h>
#include "inno_timer.h"
#include "inno_io.h"
#include "hal.h"
#include "innopower.h"
#include "inno_devfreq_gov.h"
#include "inno_input_event.h"

#define INNO_POWERCHIP_MAX_NUM	32

static struct i2c_device_id inno_powerchip_id_tbl[INNO_POWERCHIP_MAX_NUM];
static int id_tbl_idx = 0;
static struct i2c_driver *i2c_drv = NULL;

static uint32_t innopwr_get_vol_cmd_ver(struct dev_rsrc *pdevrsrc)
{
	return pdevrsrc->vol_cmd_version;
}

#define VOL_CMD_HOLD_10MS (10000)
static int innopwr_drv_mcufw_sysstat_notify(struct innopower *power, uint32_t sys_stat, uint32_t timout)
{
	int err = 0;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	uint32_t vol_cmd_verison = innopwr_get_vol_cmd_ver(pdev_rsrc);

	if (pdev_rsrc && pdev_rsrc->mcufw_comm_v) {
		if (vol_cmd_verison >= MCUFW_COMM_CMD_VOL_V2) {
			err = fh2m_hal_mcufw_comm_msg_xfer(pdev_rsrc, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_NOTIFY, &sys_stat, sizeof(sys_stat));
			if (err) {
				innopwr_notice("run finished err[%d] verison[%d %u] sys_stat[%d]\n",
					err, pdev_rsrc->mcufw_comm_v, vol_cmd_verison, sys_stat);
			}
			/*The interval time between commands to avoid mcufw lost irqs*/
			if (timout) {
				fh2m_inno_usleep_range(timout, timout);
			}
		}
	}

	return err;
}

static int innopwr_drv_mcufw_set_pcie_speed(struct innopower *power, uint32_t pcie_speed_cap, uint32_t timout)
{
	int err = 0;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	uint32_t vol_cmd_verison = innopwr_get_vol_cmd_ver(pdev_rsrc);

	if (pdev_rsrc && pdev_rsrc->mcufw_comm_v >= 4) {
		err = fh2m_hal_mcufw_comm_msg_xfer(pdev_rsrc, MCUFW_COMM_MODULE_FAN, MCUFW_COMM_CMD_SET_PCIE_SPEED, &pcie_speed_cap, sizeof(pcie_speed_cap));
		if (err) {
			innopwr_notice("run finished err[%d] verison[%d %u] pcie_speed_cap[%d]\n",
				err, pdev_rsrc->mcufw_comm_v, vol_cmd_verison, pcie_speed_cap);
		}
		/*The interval time between commands to avoid mcufw lost irqs*/
		if (timout) {
			fh2m_inno_usleep_range(timout, timout);
		}
	}

	return err;
}

static int innopwr_drv_mcufw_set_voltage(void* chip_ctx, uint32_t mv, int sys_stat)
{
	uint32_t cmd_data[5] = {0};
	uint32_t vol_cmd_verison = innopwr_get_vol_cmd_ver(chip_ctx);

	if (MCUFW_COMM_CMD_VOL_V1 == vol_cmd_verison) {
		innopwr_dbg("MCUFW_COMM_CMD_VOL_V1 mv[%u] with sys_stat[%d]\n", mv, sys_stat);
		cmd_data[0] = mv;
		cmd_data[1] = sys_stat;
		return fh2m_hal_mcufw_comm_msg_xfer(chip_ctx, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_SET, cmd_data, sizeof(cmd_data));
	} else {
		innopwr_dbg("MCUFW_COMM_CMD_VOL_V%u mv[%u] without sys_stat[%d]\n", vol_cmd_verison, mv, sys_stat);
		return fh2m_hal_mcufw_comm_msg_xfer(chip_ctx, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_SET, &mv, sizeof(mv));
	}
}

static int innopwr_drv_mcufw_get_voltage_from_bmc(void* chip_ctx, uint32_t *mv)
{
	return fh2m_hal_get_voltage_from_bmc(chip_ctx, mv);
}

static int innopwr_drv_mcufw_get_voltage_from_reg(void* chip_ctx, uint32_t *mv)
{
	return fh2m_hal_mcufw_comm_msg_xfer(chip_ctx, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_GET, mv, sizeof(*mv));
}

static int innopwr_drv_mcufw_get_voltage(void* chip_ctx, uint32_t *mv)
{
	if (PWRD_DBG_REAL_VOL == fh2m_get_pwr_debug_lvl()) {
		return innopwr_drv_mcufw_get_voltage_from_reg(chip_ctx, mv);
	} else {
		return innopwr_drv_mcufw_get_voltage_from_bmc(chip_ctx, mv);
	}
}

static int innopower_set_voltage(void *chip_ctx, unsigned int vol, int sys_stat)
{
	int err = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	struct innopower *power = platform_get_drvdata(pdev_rsrc->power_dev[0]);

	if (!fh2m_is_support_update_voltage(power->pdev) || !fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
		innopwr_dbg("is_support_update_vol[%d] is_digital_vol[%d]\n",
			fh2m_is_support_update_voltage(power->pdev), fh2m_hal_vol_is_digital(pdev_rsrc->dev));
		return INNO_POWER_OK;
	}

	if (!innopwr_get_vol_cmd_ver(pdev_rsrc)) {
		if (!power || !power->chip || !power->chip->set_gpu_voltage) {
			innopwr_notice("null pointer\n");
			return INNO_POWER_ERR;
		}

		return power->chip->set_gpu_voltage(power->chip, 0, vol);
	} else {
		err = innopwr_drv_mcufw_set_voltage(chip_ctx, vol, sys_stat);
		if (err) {
			innopwr_notice("set voltage is failed err[%d]\n", err);
			return INNO_POWER_ERR;
		}
	}

	return INNO_POWER_OK;
}

static int innopower_get_voltage(void *chip_ctx)
{
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	struct innopower *power = platform_get_drvdata(pdev_rsrc->power_dev[0]);
	int vol = 0;

	if (!fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
		return ANALOG_VOLT_DEFAULT_OUTPUT;
	}

	if (!innopwr_get_vol_cmd_ver(pdev_rsrc)) {
		if (!power || !power->chip || !power->chip->get_gpu_voltage) {
			innopwr_notice("null pointer\n");
			return INNO_POWER_ERR;
		}

		vol = power->chip->get_gpu_voltage(power->chip, 0);
		return vol;
	} else {
		if (!innopwr_drv_mcufw_get_voltage(chip_ctx, (uint32_t *)&vol)) {
			innopwr_info("get mv = %d\n", vol);
			return vol;
		} else {
			innopwr_notice("get failed\n");
			return INNO_POWER_ERR;
		}
	}
}

static ssize_t innopower_store_gpu_voltage(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int gpu_voltage = 0;
	struct innopower *power = NULL;
	struct dev_rsrc *pdev_rsrc = NULL;

	power = platform_get_drvdata(to_platform_device(dev));
	if (!power){
		innopwr_notice("null pointer\n");
		return -EINVAL;
	}

	pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	if (!pdev_rsrc){
		innopwr_notice("null pointer\n");
		return -EINVAL;
	}

	if (!fh2m_is_support_update_voltage(power->pdev)) {
		return count;
	}

	if (sscanf(buf, " %i ", &gpu_voltage) != 1) {
		innopwr_err("Failed to store  gpu_voltage attribute\n");
		return -EINVAL;
	}

	if (!innopwr_get_vol_cmd_ver(pdev_rsrc)) {
		if (!power || !power->chip || !power->chip->set_gpu_voltage) {
			innopwr_notice("null pointer\n");
			return -EINVAL;
		}

		retval = power->chip->set_gpu_voltage(power->chip, 0, gpu_voltage);
	} else {
			fh2m_hal_set_voltage(power->pdev, (unsigned int)gpu_voltage);
	}
	innopwr_notice("gpu_voltage = %d\n", gpu_voltage);

	return retval ? retval : count;
}

static ssize_t innopower_show_gpu_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int  retval = 0;
	int  gpu_voltage = 0;
	int  retry = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);

	do {
		retval = innopower_get_voltage(pdev_rsrc);
		retry++;
		fh2m_inno_msleep(5);
	} while ((retry < 5) && (retval <= 0 || retval > 1200));

	if (retval <= 0 || retval > 1200) {
		innopwr_err("Failed to read  gpu_voltage attribute\n");
		sprintf(buf, "0\n");
		return retval;
	}
	gpu_voltage = retval;
	innopwr_notice("[%s]: gpu_voltage = %d retry=[%d]\n",
		gpu_voltage > 950 ? "warnning" : "success", retval, retry);

	return sprintf(buf, "%d\n", gpu_voltage);
}

DEVICE_ATTR(gpu_voltage, S_IRUGO | S_IWUSR, innopower_show_gpu_voltage, innopower_store_gpu_voltage);

static ssize_t innopwr_store_pwrhelp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t innopwr_mode_info_print(struct dev_rsrc *pdev_rsrc, struct innopower *power, char *buf_offset)
{
	unsigned char pwr_mode = 0;
	ssize_t count = 0;
	struct power_custom_hwinfo_t *pwrinfo = &pdev_rsrc->pwrinfo;

	if (pwrinfo->pwr_ctrl_mode & INNO_POWER_WORKMODE_SUPPORT_MASK) {
		pwr_mode = pwrinfo->pwr_ctrl_mode & INNO_POWER_DEFAULT_WORKMODE_MASK;
		if ((pwr_mode == DEFAULT_WORKMODE_POWSAVE)) {
			count += fh2m_inno_sprintf(buf_offset + count, 1024,
				"pwr_mode[powersave]\n" \
				"pwr_ctrl_mode[0x%x]\n" \
				"powersave_vdd_core_vol %d\n" \
				"powersave_databus_freq %d\n" \
				"powersave_ddr_speed %d\n" \
				"powersave_pcie_speed %d\n",
				pwrinfo->pwr_ctrl_mode,
				pwrinfo->powersave_vdd_core_vol,
				pwrinfo->powersave_databus_freq,
				pwrinfo->powersave_ddr_speed,
				pwrinfo->powersave_pcie_speed);
		} else if (pwr_mode == DEFAULT_WORKMODE_PERF) {
			count += fh2m_inno_sprintf(buf_offset + count, 1024,
				"pwr_mode[performance]\n" \
				"pwr_ctrl_mode[0x%x]\n" \
				"performance_mode_freq %d\n" \
				"performence_mode_volt %d\n",
				pwrinfo->pwr_ctrl_mode,
				pwrinfo->performance_mode_freq,
				pwrinfo->performence_mode_volt);
		} else if (pwr_mode == DEFAULT_WORKMODE_DYN) {
			count += fh2m_inno_sprintf(buf_offset + count, 1024,
				"pwr_mode[dvfs || dyn]\n" \
				"pwr_ctrl_mode[0x%x]\n" \
				"dyn_mode_freq %d\n" \
				"dyn_freq_max %d\n" \
				"dyn_freq_step %d\n" \
				"dyn_freq_min %d\n" \
				"dyn_lpc_version %d\n" \
				"dyn_lpc_dbus_freq %u\n" \
				"dyn_lpc_gpu_utils %d\n",
				pwrinfo->pwr_ctrl_mode,
				pwrinfo->dyn_mode_mode_freq,
				pwrinfo->dyn_freq_max,
				pwrinfo->dyn_freq_step,
				pwrinfo->dyn_freq_min,
				pwrinfo->dyn_lpc_version,
				pwrinfo->dyn_lpc_dbus_freq,
				pwrinfo->dyn_lpc_gpu_utils);

				count += fh2m_inno_sprintf(buf_offset + count, 1024, "pcie_max_speed_cap[0x%x] dbus_idle_freq[%d] pcie_drop_timeout[%d]\n",
					power->pcie_speed_max_cap, fh2m_hal_get_mod_pcie_drop_timeout());

				if (fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
					if ((pdev_rsrc->custom_ver.major == 5 && pdev_rsrc->custom_ver.minor >= 6) || pdev_rsrc->custom_ver.major >= 6) {
						count += fh2m_inno_sprintf(buf_offset + count, 1024, "power_is_digital[true]:cus_update_vol[%d] mod_update_vol[%d] finally update_vol[%d]\n",
							pwrinfo->dyn_update_vol_enable, fh2m_is_mod_update_voltage_enable(), fh2m_is_support_update_voltage(pdev_rsrc->dev));
					} else {
						count += fh2m_inno_sprintf(buf_offset + count, 1024, "power_is_digital[true]:mod_update_vol[%d] finally update_vol[%d]\n",
							fh2m_is_mod_update_voltage_enable(), fh2m_is_support_update_voltage(pdev_rsrc->dev));
					}
				} else {
					count += fh2m_inno_sprintf(buf_offset + count, 1024, "power_is_digital[false]: finally update_vol[false]\n");
				}
		} else {
			count += fh2m_inno_sprintf(buf_offset + count, 1024,
				"pwr_mode[normal]\n" \
				"pwr_ctrl_mode[0x%x]\n" \
				"normal_mode_freq %d\n",
				pwrinfo->pwr_ctrl_mode,
				pwrinfo->normal_mode_freq);
		}
	}
	return count;
}

static ssize_t innopwr_show_pwrhelp(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	uint32_t vol_cmd_version = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	mcufw_ver_info_t *pmcufw_ver = &pdev_rsrc->mcufw_ver_info;
	mcufw_ver_desc_t * pmcufw_ver_desc = &pmcufw_ver->mcufw_ver_desc;
	gpupll_screen_info_t *gpupll_screen_info = &pdev_rsrc->efuse.gpupll_screen_info;

	count += fh2m_inno_sprintf(buf + count, 128, "mcufw %ubit format, %s verison[%u.%u.%u.%u]\n",
		pmcufw_ver->mcufw_v_format, pmcufw_ver->internal_verison ? "internal" : "external" , pmcufw_ver_desc->mcufw_major,
		pmcufw_ver_desc->mcufw_minor, pmcufw_ver_desc->mcufw_revision, pmcufw_ver_desc->mcufw_build);

	vol_cmd_version = pdev_rsrc->vol_cmd_version;
	count += fh2m_inno_sprintf(buf + count, 128, "kmd2mcufw mcufw_comm_v[%d] vol_cmd_version[%u]: bmcv[%d] custom[%d %d] opp[1.1]\n",
		pdev_rsrc->mcufw_comm_v, pdev_rsrc->vol_cmd_version, pdev_rsrc->bmc.verison, pdev_rsrc->custom_ver.major, pdev_rsrc->custom_ver.minor);

	if (MCUFW_COMM_CMD_VOL_V1 == vol_cmd_version) {
		count += fh2m_inno_sprintf(buf + count, 128, "updating voltage with sys_stat\n");
	} else if (vol_cmd_version >= MCUFW_COMM_CMD_VOL_V2) {
		count += fh2m_inno_sprintf(buf + count, 128, "updating voltage with not sys_stat and additional cmd is used to do it\n");
	} else {
		count += fh2m_inno_sprintf(buf + count, 128, "kmd Fallback to legacy: host drv access voltage chip by pmbus directly\n");
	}

	count += fh2m_inno_sprintf(buf + count, 128, "prjinfo: name[%s] prj_is_legacy[%d] pwr_is_debug[%d]\n\n",
		pdev_rsrc->prj_name ? pdev_rsrc->prj_name : "PRJ_NUM_OTHER", pdev_rsrc->prj_is_legacy, pdev_rsrc->pwr_is_debug);

	if (CHIP_G1P_SOC == pdev_rsrc->chip_type) {
		count += fh2m_inno_sprintf(buf + count, 128, "gpu_init_freq=%u custom_freq=%u efuse_is_valid=%d efuse_freq=%u\n",
		pdev_rsrc->gpu_init_freq, pdev_rsrc->custom_gpu_freq, pdev_rsrc->efuse_is_valid, pdev_rsrc->efuse_gpu_freq);
	} else {
		fh2m_hal_get_gpufreq_info(pdev_rsrc->dev);
		count += fh2m_inno_sprintf(buf + count, 128, "vol_chip[%s]\n", pdev_rsrc->vol_is_digital ? "digital" : "analog");
		count += fh2m_inno_sprintf(buf + count, 128, "gpu_init_freq=%u custom_freq=%u ft,slt[%d %u, %d %u] efuse_is_valid=%d efuse_freq=%u efuse_spec_lvl=%u max/min[%d %d]\n",
			pdev_rsrc->gpu_init_freq, pdev_rsrc->custom_gpu_freq,
			gpupll_screen_info->ft_is_valid, gpupll_screen_info->ft_spec_lvl, gpupll_screen_info->slt_is_valid, gpupll_screen_info->slt_spec_lvl,
			pdev_rsrc->efuse_is_valid, pdev_rsrc->efuse_gpu_freq, pdev_rsrc->efuse_spec_lvl,
			pdev_rsrc->gfreqinfo.maxfreq, pdev_rsrc->gfreqinfo.minfreq);
	}

	count += innopwr_mode_info_print(pdev_rsrc, power, buf + count);

	return count;
}

DEVICE_ATTR(pwrhelp, S_IRUGO | S_IWUSR, innopwr_show_pwrhelp, innopwr_store_pwrhelp);

static ssize_t innopower_store_pcie_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int pcie_speed = 0;
	struct innopower *power = NULL;
	struct dev_rsrc *pdev_rsrc = NULL;

	power = platform_get_drvdata(to_platform_device(dev));
	if (!power){
		innopwr_notice("null pointer\n");
		return -EINVAL;
	}

	pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	if (!pdev_rsrc){
		innopwr_notice("null pointer\n");
		return -EINVAL;
	}

	if (sscanf(buf, " %i ", &pcie_speed) != 1) {
		innopwr_err("Failed to store  pcie_speed attribute\n");
		return -EINVAL;
	}

	if (pcie_speed == PCIE_SPEED_GEN1) {
		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN1, 0);
	} else if (pcie_speed == PCIE_SPEED_GEN2) {
		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN2, 0);
	} else {
#if defined(CONFIG_LOONGARCH)
		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN2, 0);
#else
		innopwr_drv_mcufw_set_pcie_speed(power, power->pcie_speed_max_cap, 0);
#endif
	}

	power->pcie_speed = pcie_speed;

	innopwr_notice("pcie_speed = %d\n", pcie_speed);

	return retval ? retval : count;
}

static ssize_t innopower_show_pcie_speed(struct device *dev, struct device_attribute *attr, char *buf)
{
	int  pcie_speed = 0;
	int  retry = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_notice("[%s]: pcie_speed = %d retry=[%d]\n",
		pcie_speed > 950 ? "warnning" : "success", power->pcie_speed, retry);

	return sprintf(buf, "%d\n", pcie_speed);
}
DEVICE_ATTR(pcie_speed, S_IRUGO | S_IWUSR, innopower_show_pcie_speed, innopower_store_pcie_speed);

static int innopwr_pcie_speed_switch_to(void *chip_ctx,  int speed)
{
	int err = 0;
	static long long switch_cnt = 0;
	struct dev_rsrc *pdev_rsrc = (struct dev_rsrc *)chip_ctx;
	struct innopower *power = platform_get_drvdata(pdev_rsrc->power_dev[0]);
	unsigned long pcie_drop_timeout = 1000 * fh2m_hal_get_mod_pcie_drop_timeout();

	mutex_lock(&power->drop_speed_mtx);

	if (speed == PCIE_SPEED_GEN1 && power->pcie_speed == PCIE_SPEED_MAX) { 		/*drop speed*/
		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN2, 0);

		fh2m_inno_usleep_range(pcie_drop_timeout, pcie_drop_timeout);

		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN1, 0);
	} else if (speed == PCIE_SPEED_MAX && power->pcie_speed == PCIE_SPEED_GEN1) {	/*ring speed*/
#if defined(CONFIG_LOONGARCH)
		innopwr_drv_mcufw_set_pcie_speed(power, PCIE_SPEED_CAP_GEN2, 0);
#else
		innopwr_drv_mcufw_set_pcie_speed(power, power->pcie_speed_max_cap, 0);
#endif
	} else {
		mutex_unlock(&power->drop_speed_mtx);
		return err;
	}
	switch_cnt++;
	power->pcie_speed = speed;
	mutex_unlock(&power->drop_speed_mtx);

	innopwr_info("pcie_speed = %s switch_cnt[%lld]\n",
		speed == PCIE_SPEED_GEN1 ? "PCIE_SPEED_GEN1" : "PCIE_SPEED_MAX", switch_cnt);

	return err;
}

static int innopwr_pmbus_rs_lock(struct powerchip *pwrchip, bool chip_need_rs)
{
	struct inno_pmbus_dev *pmbus = NULL;
	pmbus = (struct inno_pmbus_dev *)pwrchip->priv_data;
	if (!pmbus) {
		innopwr_err("pmbus get failed!!!\n");
		return INNO_POWER_ERR;
	}
	mutex_lock(&pmbus->chip_rs_lock);
	pmbus->chip_need_rs = chip_need_rs;
	return INNO_POWER_OK;
}

static int innopwr_pmbus_rs_unlock(struct powerchip *pwrchip)
{
	struct inno_pmbus_dev *pmbus = NULL;
	pmbus = (struct inno_pmbus_dev *)pwrchip->priv_data;
	if (!pmbus) {
		innopwr_err("pmbus get failed!!!\n");
		return INNO_POWER_ERR;
	}
	pmbus->chip_need_rs = 1;
	mutex_unlock(&pmbus->chip_rs_lock);
	return INNO_POWER_OK;
}

static int _innopwr_read8(struct powerchip *pwrchip, unsigned char reg, bool chip_need_rs)
{
	int ret = 0;
	if (!pwrchip) {
		innopwr_notice("gchip is null!!!\n");
		return INNO_POWER_ERR;
	}

	mutex_lock(&pwrchip->lock);
	if (innopwr_pmbus_rs_lock(pwrchip, chip_need_rs)) {
		mutex_unlock(&pwrchip->lock);
		innopwr_notice("%s read reg[0x%x] pmbus_rs_lock failed!!!\n", pwrchip->name, reg);
		return INNO_POWER_ERR;
	}

	ret = i2c_smbus_read_byte_data(pwrchip->client, reg);
	if (ret < 0) {
		innopwr_pmbus_rs_unlock(pwrchip);
		mutex_unlock(&pwrchip->lock);
		innopwr_err("%s read reg[0x%x] failed!!!\n", pwrchip->name, reg);

		return INNO_POWER_ERR;
	}
	innopwr_pmbus_rs_unlock(pwrchip);
	mutex_unlock(&pwrchip->lock);

	return ret;
}

static int _innopwr_write8(struct powerchip *pwrchip, unsigned char reg, unsigned char data, bool chip_need_rs)
{
	int ret = 0;

	if (!pwrchip) {
		innopwr_notice("gchip is null!!!\n");
		return INNO_POWER_ERR;
	}

	mutex_lock(&pwrchip->lock);
	if (innopwr_pmbus_rs_lock(pwrchip, chip_need_rs)) {
		mutex_unlock(&pwrchip->lock);
		innopwr_notice("%s write 0x%x to reg[0x%x] pmbus_rs_lock failed!!!\n", pwrchip->name, data, reg);
		return INNO_POWER_ERR;
	}
	ret = i2c_smbus_write_byte_data(pwrchip->client, reg, data);
	if (ret < 0) {
		innopwr_pmbus_rs_unlock(pwrchip);
		mutex_unlock(&pwrchip->lock);
		innopwr_err("%s write 0x%x to reg[0x%x] failed!!!\n", pwrchip->name, data, reg);

		return INNO_POWER_ERR;
	}
	innopwr_pmbus_rs_unlock(pwrchip);
	mutex_unlock(&pwrchip->lock);

	return INNO_POWER_OK;
}

static int _innopwr_read16(struct powerchip *pwrchip, unsigned char reg, bool chip_need_rs)
{
	int ret = 0;

	if (!pwrchip) {
		innopwr_notice("gchip is null!!!\n");
		return INNO_POWER_ERR;
	}

	mutex_lock(&pwrchip->lock);
	if (innopwr_pmbus_rs_lock(pwrchip, chip_need_rs)) {
		mutex_unlock(&pwrchip->lock);
		innopwr_notice("%s read reg[0x%x] pmbus_rs_lock failed!!!\n", pwrchip->name, reg);
		return INNO_POWER_ERR;
	}
	ret = i2c_smbus_read_word_data(pwrchip->client, reg);
	if (ret < 0) {
		innopwr_pmbus_rs_unlock(pwrchip);
		mutex_unlock(&pwrchip->lock);
		innopwr_err("%s read reg[0x%x] failed!!!\n", pwrchip->name, reg);

		return INNO_POWER_ERR;
	}
	innopwr_pmbus_rs_unlock(pwrchip);
	mutex_unlock(&pwrchip->lock);

	return ret;
}

static int _innopwr_write16(struct powerchip *pwrchip, unsigned char reg, unsigned int data, bool chip_need_rs)
{
	int ret = 0;

	if (!pwrchip) {
		innopwr_notice("gchip is null!!!\n");
		return INNO_POWER_ERR;
	}

	mutex_lock(&pwrchip->lock);
	if (innopwr_pmbus_rs_lock(pwrchip, chip_need_rs)) {
		mutex_unlock(&pwrchip->lock);
		innopwr_notice("%s write 0x%x to reg[0x%x] pmbus_rs_lock failed!!!\n", pwrchip->name, data, reg);
		return INNO_POWER_ERR;
	}
	ret = i2c_smbus_write_word_data(pwrchip->client, reg, data);
	if (ret < 0) {
		innopwr_pmbus_rs_unlock(pwrchip);
		mutex_unlock(&pwrchip->lock);
		innopwr_err("%s write 0x%x to reg[0x%x] failed!!!\n", pwrchip->name, data, reg);

		return INNO_POWER_ERR;
	}
	innopwr_pmbus_rs_unlock(pwrchip);
	mutex_unlock(&pwrchip->lock);

	return ret;
}

int innopwr_read8(struct powerchip *pwrchip, unsigned char reg)
{
	return _innopwr_read8(pwrchip, reg, 1);
}

int innopwr_read8_no_repeat_start(struct powerchip *pwrchip, unsigned char reg)
{
	return _innopwr_read8(pwrchip, reg, 0);
}

int innopwr_write8(struct powerchip *pwrchip, unsigned char reg, unsigned char data)
{
	return _innopwr_write8(pwrchip, reg, data, 1);
}

int innopwr_write8_no_repeat_start(struct powerchip *pwrchip, unsigned char reg, unsigned char data)
{
	return _innopwr_write8(pwrchip, reg, data, 0);
}

int innopwr_read16(struct powerchip *pwrchip, unsigned char reg)
{
	return _innopwr_read16(pwrchip, reg, 1);
}

int innopwr_read16_no_repeat_start(struct powerchip *pwrchip, unsigned char reg)
{
	return _innopwr_read16(pwrchip, reg, 0);
}

int innopwr_write16(struct powerchip *pwrchip, unsigned char reg, unsigned int data)
{
	return _innopwr_write16(pwrchip, reg, data, 1);
}

int innopwr_write16_no_repeat_start(struct powerchip *pwrchip, unsigned char reg, unsigned int data)
{
	return _innopwr_write16(pwrchip, reg, data, 0);
}

static ssize_t gpu_bootvol_read_file(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	ssize_t pos = 0;
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	char *buf = (void *)__get_free_page(GFP_KERNEL);
	struct innopower *power = NULL;

	if (!buf)
		return -ENOMEM;

	if (!dev)
		return -EINVAL;

	power = platform_get_drvdata(to_platform_device(dev));
	pos += fh2m_inno_sprintf(buf, 128, "gpu boot_vol_expect[%d] boot_vol_real[%d]\n", power->boot_vol_expect, power->boot_vol_real);
	pos = simple_read_from_buffer(user_buf, count, ppos, buf, pos);

	free_page((unsigned long)buf);
	return pos;
}

static ssize_t gpu_bootvol_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	return count;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static const struct file_operations gpu_bootvol_fops = {
	.open = simple_open,
	.read = gpu_bootvol_read_file,
	.write = gpu_bootvol_write_file,
};
#else
static const struct proc_ops gpu_bootvol_fops = {
	.proc_open = simple_open,
	.proc_read = gpu_bootvol_read_file,
	.proc_write = gpu_bootvol_write_file,
};
#endif

static ssize_t inno_gtest_gpupll_read_file(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	ssize_t pos = 0;
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	char *buf = (void *)__get_free_page(GFP_KERNEL);
	struct innopower *power = NULL;

	if (!buf)
		return -ENOMEM;

	if (!dev)
		return -EINVAL;

	power = platform_get_drvdata(to_platform_device(dev));
	pos += fh2m_inno_sprintf(buf, 128,
						"%s %u %s %u\n", "gpupll", power->gpupll, "gpuvol", power->gpuvol);
	pos = simple_read_from_buffer(user_buf, count, ppos, buf, pos);

	free_page((unsigned long)buf);
	return pos;
}

static int innopwr_gtest_set_gpu_pll(inno_dev* dev, uint32_t target_freq, uint32 volt)
{
	uint32_t freq = target_freq;
	uint32_t curfreq = fh2m_hal_get_pll(dev, PLL_GPU);

	if (freq == curfreq) {
		innopwr_notice("freq[%u, %u] is not change\n", freq, curfreq);
		return -1;
	}

	fh2m_hal_gpudrv_clkchange(dev, INNO_GPU_PRE_CLK_CHANGE);
	if (freq > curfreq)
	{
		fh2m_hal_set_voltage(dev, volt);
	}

	fh2m_hal_set_pll(dev, PLL_GPU, target_freq);

	if (freq < curfreq)
	{
		fh2m_hal_set_voltage(dev, volt);
	}
	fh2m_hal_gpudrv_clkchange(dev, INNO_GPU_POST_CLK_CHANGE);

	return 0;
}

static ssize_t inno_gtest_gpupll_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	uint32_t gpupll;
	uint32_t gpuvol;
	char *buf;
	char mod_name[10] = { 0 };
	char mod_volt[10] = { 0 };
#if defined(CONFIG_DEBUG_FS)
	void *dev = file->private_data;
#elif defined(CONFIG_PROC_FS)
	void *dev = fh2m_inno_get_dfs_file(file_inode(file));
#endif
	struct innopower *power = NULL;

	if (!dev)
		return -EINVAL;

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (fh2m_inno_copy_from_user(buf, user_buf, min(count, PAGE_SIZE))) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	sscanf(buf, "%s %u %s %u", mod_name, &gpupll, mod_volt, &gpuvol);
	power = platform_get_drvdata(to_platform_device(dev));
	if (!innopwr_gtest_set_gpu_pll(power->pdev, gpupll, gpuvol)) {
		power->gpupll = gpupll;
		power->gpuvol = gpuvol;
		innopwr_notice("mod_name[%s]=%u mod_volt[%s]=%u\n", mod_name, gpupll, mod_volt, gpuvol);
	}

	free_page((unsigned long)buf);

	return count;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static const struct file_operations inno_gtest_gpupll_fops = {
	.open = simple_open,
	.read = inno_gtest_gpupll_read_file,
	.write = inno_gtest_gpupll_write_file,
};
#else
static const struct proc_ops inno_gtest_gpupll_fops = {
	.proc_open = simple_open,
	.proc_read = inno_gtest_gpupll_read_file,
	.proc_write = inno_gtest_gpupll_write_file,
};
#endif

#define INNO_POWER_DEBUGFS_NAME "innopower"
static int innopwr_node_init(struct innopower *power)
{
	int err = 0;
	char temp_str[128];

	if (!power) {
		innopwr_err("power is NULL\n");
		return -ENODEV;
	}

	snprintf(temp_str, sizeof(temp_str), "%s%d", INNO_POWER_DEBUGFS_NAME, power->idx);
	power->dfs_dir_pwrdir = fh2m_inno_debugfs_or_procfs_create_dir(temp_str, NULL);
	if (IS_ERR_OR_NULL(power->dfs_dir_pwrdir)) {
		return -1;
	}

	snprintf(temp_str, sizeof(temp_str), "%s%d", "gpu_bootvol", power->idx);
	power->dfs_node_gpu_bootvol = fh2m_inno_debugfs_or_procfs_create_file(power->pltdev, temp_str, 0644, power->dfs_dir_pwrdir, &gpu_bootvol_fops);

	snprintf(temp_str, sizeof(temp_str), "%s%d", "gtest_gpupll", power->idx);
	power->dfs_node_gtest_gpupll = fh2m_inno_debugfs_or_procfs_create_file(power->pltdev, temp_str, 0644, power->dfs_dir_pwrdir, &inno_gtest_gpupll_fops);

	return err;
}

static int innopwr_node_deinit(struct innopower *power) {

	char temp_str[128];
	int err = 0;

	if (!power) {
		innopwr_err("power is NULL\n");
		return -ENODEV;
	}

	snprintf(temp_str, sizeof(temp_str), "%s%d", "gtest_gpupll", power->idx);
	if (power->dfs_node_gtest_gpupll)
		fh2m_inno_debugfs_or_procfs_remove_file(power->dfs_node_gtest_gpupll, temp_str, power->dfs_dir_pwrdir);

	snprintf(temp_str, sizeof(temp_str), "%s%d", "gpu_bootvol", power->idx);
	if (power->dfs_node_gpu_bootvol)
		fh2m_inno_debugfs_or_procfs_remove_file(power->dfs_node_gpu_bootvol, temp_str, power->dfs_dir_pwrdir);

	if (power->dfs_dir_pwrdir)
		fh2m_inno_debugfs_or_procfs_remove_dir(power->dfs_dir_pwrdir);

	return err;
}

static int innopower_sys_register(struct device *dev)
{
	int ret = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	ret = device_create_file(dev, &dev_attr_gpu_voltage);

	ret = device_create_file(dev, &dev_attr_pwrhelp);

	ret = device_create_file(dev, &dev_attr_pcie_speed);

	innopwr_node_init(power);

	return ret;
}

static void innopower_sys_unregister(struct device *dev)
{
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_node_deinit(power);

	device_remove_file(dev, &dev_attr_pcie_speed);

	device_remove_file(dev, &dev_attr_pwrhelp);

	device_remove_file(dev, &dev_attr_gpu_voltage);
}

#define I2C_SLAVE_ADDR_VALID 0x7F
#define I2C_SLAVE_ADDR_INVALID 0x0
#define I2C_SLAVE_ADDE_DEFAULT_VALID 0x20
/*default double chip use xdpe12284c*/
#define DOUBLE_CHIP_IDX0_DEFAULT_ADDR 0xEC
static int innopower_register_chip(struct innopower *power)
{
	struct powerchip *chip;
	struct i2c_board_info info;

	chip = devm_kzalloc(power->pltdev, sizeof(struct powerchip), GFP_KERNEL);
	if (!chip) {
		innopwr_err("devm_kzalloc failed\n");
		goto chip_fail;
	}

	/*i2c info static register*/
	/*double is not support hwinfo and custom info parase, and have a default addr*/

	memset(&info, 0, sizeof(struct i2c_board_info));
	if (power->params->slave_addr == I2C_SLAVE_ADDR_INVALID) {
		power->params->slave_addr = DOUBLE_CHIP_IDX0_DEFAULT_ADDR ;
		if (power->params->slave_addr > I2C_SLAVE_ADDR_VALID) {
			info.addr = power->params->slave_addr >> 1;
		} else {
			info.addr = power->params->slave_addr;
		}

		strcpy(power->params->chip_name, "VIRTURE_CHIP");
	}

	info.addr = power->params->slave_addr;
	if (info.addr > I2C_SLAVE_ADDR_VALID) { /*default xdpe12284c*/
		info.addr = DOUBLE_CHIP_IDX0_DEFAULT_ADDR >> 1;
	}
	sprintf(info.type, "%s-%d", power->params->chip_name, id_tbl_idx);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
	chip->client = i2c_new_device(power->adapter, &info);
#else
	chip->client = i2c_new_client_device(power->adapter, &info);
#endif
	if (!chip->client) {
		innopwr_err("i2c_new_device failed\n");
		goto chip_fail;
	}
	i2c_set_clientdata(chip->client, chip);
	power->chip = chip;
	chip->priv_data = i2c_get_adapdata(power->adapter);

	/*inno_powerchip_id_tbl is created dynamic*/
	strcpy(inno_powerchip_id_tbl[id_tbl_idx].name, chip->client->name);
	id_tbl_idx++;

	i2c_drv = innopower_get_drvobj(power->params->chip_name);
	if (!i2c_drv) {
		innopwr_err("not support drv\n");
		goto chip_fail;
	}
	innopwr_info("name = %s type= %s addr = 0x%x i2c_drv = %px\n", chip->client->name, info.type, info.addr, i2c_drv);

	return 0;

chip_fail:
	devm_kfree(power->pltdev, chip);
	power->chip = NULL;

	return -ENOMEM;
}

static int innopower_unregister_chip(struct innopower *power)
{
	if (power->chip) {
		i2c_unregister_device(power->chip->client);
		devm_kfree(power->pltdev, power->chip);
	}

	return 0;
}

static uint32_t innopwr_find_vol(inno_dev *dev, uint32_t gpu_freq)
{
	int i = 0;
	uint32_t vol = 0;
	struct inno_gpu_opp *opp_tbl = fh2m_hal_power_get_opptbl(dev);
	int32_t opp_size = fh2m_hal_power_get_opptbl_size(dev);

	for (i = 0; i < opp_size; i++) {
		if (gpu_freq == opp_tbl[i].freq / GPU_FREQ_M2HZ_UNIT) {
			vol = opp_tbl[i].volt / GPU_VOL_UV2MV_UNIT;
			break;
		}
	}

	if (!vol) {
		vol = opp_tbl[opp_size-1].volt / GPU_VOL_UV2MV_UNIT;
		innopwr_notice("found init vol failed and take max vol[%u] as init vol opp_size[%d]\n", vol, opp_size);
	}

	return vol;
}

static int innopower_vol_init(inno_dev *dev, int sys_stat)
{
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(dev);
	struct power_custom_hwinfo_t *pwrinfo = &pdev_rsrc->pwrinfo;
	uint8_t  pwr_default_mode = pwrinfo->pwr_ctrl_mode & INNO_POWER_DEFAULT_WORKMODE_MASK;
	uint32_t init_vol = 0;
	uint16_t init_freq = 0;

	switch (pwr_default_mode) {
		case DEFAULT_WORKMODE_POWSAVE:
			init_freq = pwrinfo->powersave_mode_freq;
			break;
		case DEFAULT_WORKMODE_DYN:
			init_freq = pwrinfo->dyn_mode_mode_freq;
			break;
		case DEFAULT_WORKMODE_PERF:
			init_freq = pwrinfo->performance_mode_freq;
			break;
		case DEFAULT_WORKMODE_NORMAL:
		default:
			init_freq = pwrinfo->normal_mode_freq;
			break;
	}

	init_vol = innopwr_find_vol(dev, init_freq);

	if (!fh2m_is_support_update_voltage(dev)) {
		return init_vol;
	}

	innopwr_dbg("pwr_default_mode = %u init_freq[%d] init_vol[%u]\n", pwr_default_mode, init_freq, init_vol);

	if (INNO_POWER_OK != innopower_set_voltage(pdev_rsrc, init_vol, sys_stat)) {
		innopwr_err("set hw init voltage failed and pwr_default_mode=[%u] init_freq=[%d] init_vol[%u]\n",
			pwr_default_mode, init_freq, init_vol);
		return INNO_POWER_ERR;
	}

	return init_vol;
}

static int innopower_pmbus_probe(struct platform_device *pdev)
{
	struct innopower *power;
	struct device *dev = &pdev->dev;
	struct dev_rsrc *pdev_rsrc = NULL;
	int boot_vol_real = 0;

	power = devm_kzalloc(dev, sizeof(*power), GFP_KERNEL);
	if (!power) {
		innopwr_err("devm_kzalloc failed\n");
		goto power_fail;
	}

	power->pltdev = dev;
	platform_set_drvdata(pdev, power);
	power->params = fh2m_hal_get_volctrl_hwinfo(pdev->dev.parent);
	power->adapter = fh2m_hal_get_pmbus_adapter(pdev->dev.parent, power->params->pmbus_id);
	if (!power->adapter) {
		innopwr_err("i2c_get_adapter failed\n");
		goto power_fail;
	}

	innopower_register_chip(power);

	power->pdev = pdev->dev.parent;
	pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	power->idx = pdev_rsrc->pcie_func_idx;

	mutex_init(&power->drop_speed_mtx);
	fh2m_hal_power_ops_init(power->pdev, innopower_set_voltage,
		innopower_get_voltage, innopwr_pcie_speed_switch_to);
	power->boot_vol_expect = innopower_vol_init(power->pdev, PWR_NOTIFY_FOR_NORMAL);
	if (fh2m_hal_vol_is_digital(pdev_rsrc->dev) && innopwr_get_vol_cmd_ver(pdev_rsrc)) {
		if (innopwr_drv_mcufw_get_voltage_from_reg(pdev_rsrc, &boot_vol_real)) {
			boot_vol_real = power->boot_vol_expect;
		}
	} else {
		boot_vol_real = 0;
	}
	power->boot_vol_real = boot_vol_real;

	power->pcie_speed_max_cap = fh2m_hal_pcie_speed_max_cap(pdev_rsrc->dev);
	power->pcie_speed = PCIE_SPEED_MAX;

	innopower_sys_register(&pdev->dev);

	innopwr_dbg("power[%d] run finished and init vol is %d is_digital = %d fh2m_is_support_update_voltage = %d pcie_speed_max_cap[0x%x]\n",
		power->idx, power->boot_vol_real, fh2m_hal_vol_is_digital(power->pdev), fh2m_is_support_update_voltage(power->pdev), power->pcie_speed_max_cap);

	return 0;

power_fail:
	devm_kfree(dev, power);
	innopwr_err("power_fail\n");

	return -ENOMEM;
}

static int innopower_pmbus_remove(struct platform_device *pdev)
{
	struct innopower *power = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	innopower_sys_unregister(&pdev->dev);

	mutex_destroy(&power->drop_speed_mtx);

	innopower_vol_init(power->pdev, PWR_NOTIFY_FOR_NORMAL);

	innopower_unregister_chip(power);

	devm_kfree(dev, power);

	innopwr_dbg("run finished\n");

	return 0;
}

static void innopower_pmbus_shutdown(struct platform_device *pdev)
{
	struct innopower *power = platform_get_drvdata(pdev);

	/* fix bug11626/11628: "magic num or version num error" is
	 * reported by mcufw as mcufw cleaned protocal vram */
	innopower_vol_init(power->pdev, PWR_NOTIFY_FOR_SHUTDOWN);

	fh2m_inno_usleep_range(10000, 10000);

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_SHUTDOWN, 0);

	innopwr_dbg("run finished\n");
}
struct platform_device_id innopower_pmbus_device_id_table[] = {
	{ .name = INNO_POWER_DEVICE_NAME, .driver_data = 0 },
	{},
};

static int innopwr_enter_sleep(struct device *dev, char *msg)
{
	int ret = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	uint32_t sleep_gpu_freq = fh2m_hal_get_pll(pdev_rsrc->dev, PLL_GPU);
	uint32_t real_vol = 0;

	if (!fh2m_is_support_update_voltage(power->pdev)) {
		return 0;
	}

	if (fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
		if (innopwr_drv_mcufw_get_voltage_from_reg(pdev_rsrc, &real_vol)) {
			innopwr_notice("%s read voltage failed and sleep_vol[%d]\n", msg, pdev_rsrc->sleep_vol);
			return -1;
		}

	} else {
		real_vol = ANALOG_VOLT_DEFAULT_OUTPUT;
	}
	pdev_rsrc->sleep_vol = innopwr_find_vol(pdev_rsrc->dev, sleep_gpu_freq);

	pdev_rsrc->sleep_freq = sleep_gpu_freq;
	innopwr_dbg("%s run finished and opp[%u, %d %u]\n", msg, sleep_gpu_freq, pdev_rsrc->sleep_vol, real_vol);

	return ret;
}

static int innopwr_exit_sleep(struct device *dev, char *msg, int sys_stat)
{
	int ret = 0;
	uint32_t real_vol = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(power->pdev);
	uint32_t gpu_freq = fh2m_hal_get_pll(pdev_rsrc->dev, PLL_GPU);

	if (!fh2m_is_support_update_voltage(power->pdev)) {
		return 0;
	}

	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_DBG) {
		if (fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
			if (!innopwr_drv_mcufw_get_voltage_from_reg(pdev_rsrc, &real_vol)) {
				innopwr_notice("%s bf run start and opp[%u, %u | %u, %d]\n",
						msg, gpu_freq, real_vol, pdev_rsrc->sleep_freq, pdev_rsrc->sleep_vol);
			} else {
				innopwr_notice("%s bf read voltage failed\n", msg);
			}
		} else {
			real_vol = ANALOG_VOLT_DEFAULT_OUTPUT;
		}
	}

	if (pdev_rsrc->sleep_vol > 0) {
		if (gpu_freq < pdev_rsrc->sleep_freq) { //increase freq
			innopower_set_voltage(pdev_rsrc, pdev_rsrc->sleep_vol, sys_stat);
		}

		fh2m_hal_set_pll(pdev_rsrc->dev, PLL_GPU, pdev_rsrc->sleep_freq);

		if (gpu_freq >= pdev_rsrc->sleep_freq) { //decrease freq
			innopower_set_voltage(pdev_rsrc, pdev_rsrc->sleep_vol, sys_stat);
		}
	}

	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_DBG) {
		gpu_freq = fh2m_hal_get_pll(pdev_rsrc->dev, PLL_GPU);
		if (fh2m_hal_vol_is_digital(pdev_rsrc->dev)) {
			if (!innopwr_drv_mcufw_get_voltage_from_reg(pdev_rsrc, &real_vol)) {
				innopwr_notice("%s af run finished and opp[%u, %u | %u, %d]\n",
						msg, gpu_freq, real_vol, pdev_rsrc->sleep_freq, pdev_rsrc->sleep_vol);
			} else {
				innopwr_notice("%s af read voltage failed\n", msg);
			}
		} else {
			innopwr_notice("%s bf analog power real_vol is %d\n", msg, real_vol);
		}
	}

	return ret;
}

static int innopwr_suspend(struct device *dev)
{
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_S3_ENTER, VOL_CMD_HOLD_10MS);

	innopwr_enter_sleep(dev, "s3 suspend");

	return 0;
}

static int innopwr_resume(struct device *dev)
{
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_S3_EXIT, VOL_CMD_HOLD_10MS);

	innopwr_exit_sleep(dev, "s3 resume", PWR_NOTIFY_FOR_S3_EXIT);

	return 0;
}

static int innopwr_freeze(struct device *dev)
{
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_S4_ENTER, VOL_CMD_HOLD_10MS);

	innopwr_enter_sleep(dev, "s4 freeze");

	return 0;
}

static int innopwr_thaw(struct device *dev)
{
	int ret = 0;

	innopwr_dbg("run finished\n");

	return ret;
}
static int innopwr_poweroff(struct device *dev)
{
	int ret = 0;
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_POWEROFF, VOL_CMD_HOLD_10MS);

	innopower_vol_init(power->pdev, PWR_NOTIFY_FOR_POWEROFF);

	innopwr_dbg("run finished\n");

	return ret;
}

static int innopwr_restore(struct device *dev)
{
	struct innopower *power = platform_get_drvdata(to_platform_device(dev));

	innopwr_drv_mcufw_sysstat_notify(power, PWR_NOTIFY_FOR_S4_EXIT, VOL_CMD_HOLD_10MS);

	innopwr_exit_sleep(dev, "s4 restore", PWR_NOTIFY_FOR_S4_EXIT);

	return 0;
}

const static struct dev_pm_ops innopwr_pm_ops = {
	.suspend = innopwr_suspend,
	.resume = innopwr_resume,
	.freeze = innopwr_freeze,
	.thaw = innopwr_thaw,
	.poweroff = innopwr_poweroff,
	.restore = innopwr_restore,
};


struct platform_driver innopower_pmbus_driver = {
	.probe = innopower_pmbus_probe,
	.remove = innopower_pmbus_remove,
	.shutdown = innopower_pmbus_shutdown,
	.driver = {
		.name = INNO_POWER_DEVICE_NAME,
		.pm = &innopwr_pm_ops,
	},
	.id_table = innopower_pmbus_device_id_table,
};

#ifdef CONFIG_DRM_INNO_POWER
int innopower_driver_register(void)
#else
static int __init innopower_init(void)
#endif
{
	platform_driver_register(&innopower_pmbus_driver);
	if (i2c_drv) {
		i2c_drv->id_table = inno_powerchip_id_tbl;
		i2c_add_driver(i2c_drv);
	}

#ifdef CONFIG_PM_DEVFREQ
	if (inno_devfreq_gov_register())
		innopwr_err("failed register governor\n");
	inno_input_init();
#endif

	return 0;
}

#ifdef CONFIG_DRM_INNO_POWER
void innopower_driver_unregister(void)
#else
static void __exit innopower_exit(void)
#endif
{
#ifdef CONFIG_PM_DEVFREQ
	inno_input_exit();
	if (inno_devfreq_gov_unregister())
		innopwr_err("failed remove governor\n");
#endif
	if (i2c_drv) {
		i2c_del_driver(i2c_drv);
	}

	platform_driver_unregister(&innopower_pmbus_driver);
}

#ifndef CONFIG_DRM_INNO_POWER
module_init(innopower_init);
module_exit(innopower_exit);
MODULE_LICENSE("Dual MIT/GPL");
#endif
