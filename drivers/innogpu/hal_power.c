#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>

#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/thermal.h>
#include "hal.h"
#include "hal_interface.h"
#include "inno_misc.h"
#include "inno_debug.h"

#define HALPOWER "hal_power"
#define pr_power(fmt) "[%s][%s:%d]" fmt,HALPOWER,__func__,__LINE__

#define hal_pwr_info(fmt, ...) \
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) \
		fh2m_inno_printk(KERN_INFO pr_power(fmt), ##__VA_ARGS__)

#define hal_pwr_dbg(fmt, ...)\
		if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_DBG) \
			fh2m_inno_printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)

#define hal_pwr_warn( fmt, ...) \
		if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_WARN) \
			fh2m_inno_printk(KERN_WARNING pr_power(fmt), ##__VA_ARGS__)

#define hal_pwr_err(fmt, ...)\
		fh2m_inno_printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)

#define hal_pwr_notice( fmt, ...) \
			fh2m_inno_printk(KERN_NOTICE pr_power(fmt), ##__VA_ARGS__)

static struct thermal_zone_device_ops inno_thermal_zone_ops;
static struct thermal_zone_device_ops inno_thermal_bzone_ops;

#if CONFIG_THERMAL
struct innothermal *fh2m_inno_thermal_register(inno_dev *posdev)
{
	struct innothermal *pinnothmal = NULL;
	struct power_custom_hwinfo_t *powerinfo = NULL;
	char user_pm_mode = 0;

	/*get custom param info*/
	powerinfo = fh2m_hal_get_powerinfo(fh2m_inno_dev_get_parent(posdev));
	if (!powerinfo) {
		hal_pwr_err("powerinfo is null\n");
		return NULL;
	}

	user_pm_mode = powerinfo->pwr_ctrl_mode & INNO_POWER_DEFAULT_WORKMODE_MASK;
	if ((user_pm_mode & DEFAULT_WORKMODE_DYN) != DEFAULT_WORKMODE_DYN) {
		hal_pwr_dbg("not support dyn adjust freq\n");
		return NULL;
	}

	pinnothmal = kzalloc(sizeof(struct innothermal), GFP_KERNEL);
	if (!pinnothmal) {
		hal_pwr_err("devm_kzalloc failed\n");
		return NULL;
	}

	pinnothmal->posdev = posdev;
	pinnothmal->ppci_bdev = fh2m_inno_dev_get_parent(posdev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	pinnothmal->thermal_dev =  thermal_zone_device_register("innothermal", 0, 0, pinnothmal, &inno_thermal_zone_ops, NULL, 0, 0);
#else
	pinnothmal->thermal_dev =  thermal_zone_device_register_with_trips("innothermal", NULL, 0, 0, pinnothmal, &inno_thermal_zone_ops, NULL, 0, 0);
#endif
	if (fh2m_inno_is_err_or_null(pinnothmal->thermal_dev)) {
			hal_pwr_err("thermal_dev is invalid %px done\n", pinnothmal->thermal_dev);
			goto tfreq_tbl_fail;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	pinnothmal->thermal_bdev =  thermal_zone_device_register("innothermalb", 0, 0, pinnothmal, &inno_thermal_bzone_ops, NULL, 0, 0);
#else
	pinnothmal->thermal_bdev =  thermal_zone_device_register_with_trips("innothermalb", NULL, 0, 0, pinnothmal, &inno_thermal_bzone_ops, NULL, 0, 0);
#endif
	if (fh2m_inno_is_err_or_null(pinnothmal->thermal_bdev)) {
			kfree(pinnothmal->thermal_dev);
			hal_pwr_err("thermal_bdev is invalid %px done\n", pinnothmal->thermal_bdev);
			goto tfreq_tbl_fail;
	}

	return pinnothmal;

tfreq_tbl_fail:
	kfree(pinnothmal);

	return NULL;
}
INNO_EXT_SYM(fh2m_inno_thermal_register);

int fh2m_inno_thermal_unregister(struct innothermal *pinnothmal)
{
	if (!pinnothmal) {
		hal_pwr_err("pinnothmal is null\n");
		return -1;
	}

	thermal_zone_device_unregister(pinnothmal->thermal_dev);
	thermal_zone_device_unregister(pinnothmal->thermal_bdev);

	kfree(pinnothmal);
	hal_pwr_info("done\n");

	return 0;
}
INNO_EXT_SYM(fh2m_inno_thermal_unregister);
#endif

static void innopower_tempctl_init(inno_dev *posdev, struct devfreq_inno_govdata *data)
{
	inno_dev *ppci_bdev = fh2m_inno_dev_get_parent(posdev);
	void *chip_params = fh2m_hal_get_tempctl_chip_params(ppci_bdev);
	struct tpid_chip_parms algopid_parms;
	if (chip_params && (fh2m_hal_get_dynfreq_algo(ppci_bdev) == DYNFREQ_ALGO_PID)) {
		algopid_parms = *((struct tpid_chip_parms*)chip_params);

		data->pidc.P = algopid_parms.P;
		data->pidc.I = algopid_parms.I;
		data->pidc.D = algopid_parms.D;

		data->pidc.ref_temp = algopid_parms.ref_temp;
		data->pidc.recover_temp = algopid_parms.ref_temp - 10;
		data->pidc.warnning_temp = 95;
		data->pidc.over_temp = 100;
		data->pidc.outfreq[0] = algopid_parms.outfreq[0] * GPU_FREQ_M2HZ_UNIT;
		data->pidc.outfreq[1] = algopid_parms.outfreq[1] * GPU_FREQ_M2HZ_UNIT;
		data->pidc.outfreq[PIDCTL_OUT_FREQ_NUM-1] = algopid_parms.outfreq[PIDCTL_OUT_FREQ_NUM-1] * GPU_FREQ_M2HZ_UNIT;
		data->pidc.max_freq = data->pidc.outfreq[0];
		data->pidc.min_freq = data->pidc.outfreq[PIDCTL_OUT_FREQ_NUM-1];

		data->pidc.real_target_freq = algopid_parms.real_target_freq * GPU_FREQ_M2HZ_UNIT;
		data->pidc.pid_out_freq = data->pidc.real_target_freq;
		data->pidc.cur_temp = 40;
		data->pidc.pre_tmp_error = 0;
		data->pidc.ppre_tmp_error = 0;

		data->pidc.tolerance = 25 * GPU_FREQ_M2HZ_UNIT;
		data->pidc.timeout= 4;
		data->pidc.time = 0;
	}
}

#define INNO_POWER_ADJ_FREQ_SUPPORT	1
#define INNO_POWER_ADJ_FREQ_NOT_SUPPORT	0
static int devfreq_id = 0;
int fh2m_inno_governor_param_init(inno_dev *posdev, void *pvrsrv_dev_node, struct devfreq_inno_govdata *data)
{
	int i = 0;
	unsigned char found = 0;
	struct power_custom_hwinfo_t *powerinfo = NULL;
	data->ppci_bdev = NULL;
	data->pvrsrv_dev_node = NULL;
	if (fh2m_hal_hw_thermal_type(fh2m_inno_dev_get_parent(posdev)) == HAL_THML_TYPE_DEBUG) {
		hal_pwr_notice("thml type is HAL_THML_TYPE_DEBUG and return\n");
		return -EINVAL;
	}

	/*get custom param info*/
	powerinfo = fh2m_hal_get_powerinfo(fh2m_inno_dev_get_parent(posdev));
	if (!powerinfo) {
		hal_pwr_err("powerinfo is null\n");
		return -EINVAL;
	}

	if ((powerinfo->pwr_ctrl_mode & INNO_POWER_WORKMODE_SUPPORT_MASK)
		!= INNO_POWER_WORKMODE_SUPPORT_MASK) {
		hal_pwr_warn("custom info is not exist or invalid\n");
		return -EINVAL;
	}

	/*check custom mode*/
	data->userinfo.user_pm_mode = powerinfo->pwr_ctrl_mode & INNO_POWER_DEFAULT_WORKMODE_MASK;
	if (data->userinfo.user_pm_mode != DEFAULT_WORKMODE_DYN) {
		hal_pwr_notice("user_pm_mode=%u is fix freq mode\n", data->userinfo.user_pm_mode);
		return -EINVAL;
	}

	/*default disable devfreq by module params and custom.bin*/
	data->ppci_bdev = fh2m_inno_dev_get_parent(posdev);
	data->enable = fh2m_hal_is_enable_dyn_freq(data->ppci_bdev);
	if (!fh2m_hal_is_enable_dyn_freq(data->ppci_bdev)) {
		hal_pwr_notice("user_pm_mode=%u disable dvfs\n", data->userinfo.user_pm_mode);
		return -EINVAL;
	}

	if (fh2m_hal_support_idle_feature(fh2m_inno_dev_get_parent(posdev))) {
		data->support_idle_switch = true;
	} else {
		data->support_idle_switch = false;
	}

	/*dyn mode will devfreq*/
	data->freq_offset = fh2m_hal_power_get_freq_offset(fh2m_inno_dev_get_parent(posdev)) * GPU_FREQ_M2HZ_UNIT;
	data->default_freq = powerinfo->dyn_mode_mode_freq * GPU_FREQ_M2HZ_UNIT - data->freq_offset;

	data->user_mode_powsave = powerinfo->user_mode_powsave;
	data->user_powsave_freq = powerinfo->user_powsave_freq * GPU_FREQ_M2HZ_UNIT - data->freq_offset;

	/*build user level table*/
	data->userinfo.valid = 1;

	data->userinfo.tbl_size = (powerinfo->dyn_freq_max - powerinfo->dyn_freq_min) / powerinfo->dyn_freq_step + 1;
	if ((powerinfo->dyn_freq_max - powerinfo->dyn_freq_min) % powerinfo->dyn_freq_step) {
		data->userinfo.tbl_size += 1;
	}

	data->userinfo.level2freq = (unsigned long *)kmalloc(sizeof(unsigned long) * data->userinfo.tbl_size, GFP_KERNEL);
	if (fh2m_inno_is_err_or_null(data->userinfo.level2freq)) {
		data->userinfo.level2freq = NULL;
		hal_pwr_err("level2freq is invalid %px done\n", data->userinfo.level2freq);
		return -EINVAL;
	}

	for (i = 0; i < data->userinfo.tbl_size - 1; i++) {
		data->userinfo.level2freq[i] = (powerinfo->dyn_freq_min + (powerinfo->dyn_freq_step * i)) * GPU_FREQ_M2HZ_UNIT - data->freq_offset;
		hal_pwr_info("level2freq[%d] = %lu\n", i, data->userinfo.level2freq[i]);
	}
	/*handle edge*/
	data->userinfo.level2freq[i] = powerinfo->dyn_freq_max * GPU_FREQ_M2HZ_UNIT - data->freq_offset;
	hal_pwr_info("level2freq[%d] = %lu\n", i, data->userinfo.level2freq[i]);

	for (i = 0; i < data->userinfo.tbl_size; i++) {
		if (data->default_freq == data->userinfo.level2freq[i]) {
			data->userinfo.userlevel = i;
			found = 1;
		}
	}
	/*check default freq is valid*/
	if (!found) {
		data->userinfo.userlevel = data->userinfo.tbl_size - 1;
		hal_pwr_dbg("default_freq=[%lu] is not found, take %lu\n", data->default_freq, data->userinfo.level2freq[data->userinfo.userlevel]);

		data->default_freq = data->userinfo.level2freq[data->userinfo.userlevel];
	}
	hal_pwr_info("default userlevel = %lu",data->userinfo.userlevel);

	data->thermal = fh2m_inno_thermal_register(posdev);
	data->pvrsrv_dev_node = pvrsrv_dev_node;

	data->idle_enter_cnt = 0;
	data->idle_exit_cnt = 0;
	data->input_kicked = 0;
	data->load_state = LOAD_STATE_ACTIVE;

	innopower_tempctl_init(posdev, data);
	data->id = devfreq_id;
	devfreq_id++;
	fh2m_hal_get_init_gpuvol(fh2m_inno_dev_get_parent(posdev));

	return 0;
}
INNO_EXT_SYM(fh2m_inno_governor_param_init);

int fh2m_inno_governor_param_deinit(inno_dev *posdev, struct devfreq_inno_govdata *data)
{
	fh2m_inno_thermal_unregister(data->thermal);

	if (data->userinfo.level2freq) {
		kfree(data->userinfo.level2freq);
	}

	hal_pwr_info("done\n");
	return 0;
}
INNO_EXT_SYM(fh2m_inno_governor_param_deinit);

static int inno_thermal_get_temp(struct thermal_zone_device *thermal, int *temp)
{
	struct innothermal *innothmal = thermal->devdata;
	static int pre_temp = 0;
	int real_temp = 0;

	if (!innothmal)
		return -EINVAL;

	real_temp = fh2m_hal_get_chip_temperature(fh2m_inno_dev_get_parent(innothmal->posdev));
	if (thermal->emul_temperature) {
		*temp = thermal->emul_temperature;
	} else {
		*temp = real_temp;
	}

	if (pre_temp != *temp) {
		hal_pwr_info("real_temp = %d *temp = %d emul_temp = %d\n",
				real_temp, *temp, thermal->emul_temperature);
	}
	pre_temp = *temp;

	return 0;
}

static int inno_thermal_get_btemp(struct thermal_zone_device *thermal, int *temp)
{
	struct innothermal *innothmal = thermal->devdata;
	int real_temp = 0;

	if (!innothmal)
		return -EINVAL;

	real_temp = fh2m_hal_get_board_temperature(fh2m_inno_dev_get_parent(innothmal->posdev));
	*temp = real_temp;
	return 0;
}

static struct thermal_zone_device_ops inno_thermal_zone_ops = {
	.get_temp = inno_thermal_get_temp,
};

static struct thermal_zone_device_ops inno_thermal_bzone_ops = {
	.get_temp = inno_thermal_get_btemp,
};

static bool _innopwr_vol_cmd_ver_probe(struct dev_rsrc *pdevrsrc, uint32_t *pvol_cmd_version)
{
	uint32_t sys_stat = PWR_NOTIFY_FOR_NORMAL;
	uint32_t mv = 0;
	if (!fh2m_hal_mcufw_comm_msg_xfer(pdevrsrc, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_NOTIFY, &sys_stat, sizeof(sys_stat))) {
		*pvol_cmd_version = MCUFW_COMM_CMD_VOL_V2;
		return true;
	}

	if (!fh2m_hal_mcufw_comm_msg_xfer(pdevrsrc, MCUFW_COMM_MODULE_VOLTAGE, MCUFW_COMM_CMD_VOL_GET, &mv, sizeof(mv))) {
		*pvol_cmd_version = MCUFW_COMM_CMD_VOL_V1;
		return true;
	}
	*pvol_cmd_version = MCUFW_COMM_CMD_VOL_V0_LEGACY;

	return false;
}

uint32_t hal_innopwr_cmd_ver_init(inno_dev *dev)
{
	int flag = 0;
	char buf[32] = {0};
	mcufw_ver_info_t *pmcufw_ver_info = NULL;
	uint32_t vol_cmd_version = MCUFW_COMM_CMD_VOL_V1;
	struct dev_rsrc *pdevrsrc = fh2m_inno_rsrc_devres_find(dev);

	fh2m_hal_get_mcufw_version(pdevrsrc, buf, sizeof(buf));
	pmcufw_ver_info = &pdevrsrc->mcufw_ver_info;

	/*step1: MCUFW_COMM_CMD_VOL_V0_LEGACY ===> not support cmd between kmd and mcufw */
	if (fh2m_hal_mcufw_comm_get_version(pdevrsrc->dev) < 0) {
		vol_cmd_version = MCUFW_COMM_CMD_VOL_V0_LEGACY;
		goto cmd_get_finished;
	}

	/*step2: MCUFW_COMM_PROTOCOL_V2 ===> Getting module's verison by the module's cmd which must be supported */
	if (pdevrsrc->mcufw_comm_v >= MCUFW_COMM_PROTOCOL_V2) {
		vol_cmd_version = MCUFW_COMM_CMD_VOL_V2;
		if (fh2m_hal_mcufw_comm_msg_xfer(pdevrsrc, MCUFW_COMM_MODULE_VOLTAGE,
			MCUFW_COMM_CMD_VOL_GET_VER, &vol_cmd_version, sizeof(vol_cmd_version))) {
			vol_cmd_version = MCUFW_COMM_CMD_VOL_V2;
			hal_pwr_notice("vol_cmd_version get failed and take MCUFW_COMM_CMD_VOL_V2\n");
		}
		flag = 1;
		goto cmd_get_finished;
	}

	/*step3: MCUFW_COMM_PROTOCOL_V1 ===> Getting module's verison by probe dynamically*/
	if (_innopwr_vol_cmd_ver_probe(pdevrsrc, &vol_cmd_version)) {
		flag = 3;
		goto cmd_get_finished;
	}

	/*Fallback to legacy which host drv access voltage chip by pmbus*/
	flag = 4;
	vol_cmd_version = MCUFW_COMM_CMD_VOL_V0_LEGACY;

cmd_get_finished:
	pdevrsrc->vol_cmd_version = vol_cmd_version;
	pdevrsrc->vol_cmd_inited = true;
	hal_pwr_info("===>mcufw_v_format = %u and it's valid version info[%d %u] flag=%d\n",
		pmcufw_ver_info->mcufw_v_format, pdevrsrc->mcufw_comm_v, vol_cmd_version, flag);

	return vol_cmd_version;
}

