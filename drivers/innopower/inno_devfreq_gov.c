#ifdef CONFIG_PM_DEVFREQ
#include <linux/errno.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
#include <linux/thermal.h>
#define COMPAT_GET_TEMP(tz, temp) thermal_zone_get_temp(tz, temp)
#define COMPAT_TZ_VALID(tz) (tz != NULL)
#else
#define COMPAT_GET_TEMP(tz, temp) (tz->ops->get_temp(tz, temp))
#define COMPAT_TZ_VALID(tz) (tz && tz->ops && tz->ops->get_temp)
#endif
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/devfreq.h>
#include <linux/thermal.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

#include "inno_devfreq_gov.h"
#include "inno_input_event.h"
#include "hal.h"
#include "inno_task.h"
#include "inno_misc.h"
#include "inno_debug.h"

#define INNOGOV "dfreqgov"
#define innogov(fmt) "[%s][%s:%d]" fmt,INNOGOV,__func__,__LINE__
#define innogov_info(fmt, ...) \
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) \
		fh2m_inno_printk(KERN_INFO innogov(fmt), ##__VA_ARGS__)

#define innogov_dbg(fmt, ...) \
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_DBG) \
		fh2m_inno_printk(KERN_NOTICE innogov(fmt), ##__VA_ARGS__)

#define innogov_notice( fmt, ...) \
	fh2m_inno_printk(KERN_NOTICE innogov(fmt), ##__VA_ARGS__)

#define innogov_warn( fmt, ...) \
	fh2m_inno_printk(KERN_WARNING innogov(fmt), ##__VA_ARGS__)

#define innogov_err(fmt, ...) \
	fh2m_inno_printk(KERN_NOTICE innogov(fmt), ##__VA_ARGS__)

#define INNOIDLE_DETECT_PERIOD_TIME 1
#define INNOIDLE_ENTER_IDLE_CONTINUOUS_DETECT_TIME 30
#define INNOIDLE_EXIT_IDLE_CONTINUOUS_DETECT_TIME 5

#define INNO_FAILED -1
#define INNO_SUCC   0

static struct devfreq_info gdevfreq_info = {
	false, NULL, NULL
};

static int inno_devfreq_bind_input_handler(void *osdev, void *devfreq)
{
	gdevfreq_info.devfreq = devfreq;
	gdevfreq_info.osdev = osdev;
	gdevfreq_info.inited = true;

	return 0;
}

static int inno_devfreq_unbind_input_handler(void)
{
	gdevfreq_info.inited = false;
	gdevfreq_info.devfreq = NULL;
	gdevfreq_info.osdev = NULL;

	return 0;
}

struct devfreq_info* inno_get_devfreq_info(void)
{
	return &gdevfreq_info;
}

/* called with reporting input events and reporting is interrupt context, so need to consider schedule */
void inno_input_kick(void *dev)
{
	struct devfreq *devfreq = NULL;
	struct devfreq_inno_govdata *innogov_data = NULL;
	struct devfreq_info *pdevfreq_info = (struct devfreq_info *)inno_get_devfreq_info();

	if (!pdevfreq_info->inited) {
		innogov_info("devfreq info is not inited\n");
		return;
	}

	devfreq = pdevfreq_info->devfreq;
	innogov_data = devfreq->data;

	innogov_data->idle_enter_cnt = 0;
	innogov_data->idle_exit_cnt = 0;
	if (fh2m_get_pwr_debug_lvl() == PWRD_DBG_INPUT) {
		innogov_notice("dev is %p devfreq is %p innogov_data is %p load state is %s\n", dev, devfreq, innogov_data,
		innogov_data->load_state == LOAD_STATE_IDLE ? "LOAD_STATE_IDLE" : "LOAD_STATE_ACTIVE");
	}
	if (innogov_data->load_state == LOAD_STATE_IDLE) {		//load state is changed
		innogov_dbg("load state is changed: idle ===> active\n");
		innogov_data->load_state = LOAD_STATE_ACTIVE;
		innogov_data->input_kicked = true;
		if (!fh2m_inno_queue_dwork(innogov_data->devfreq_wkq, innogov_data->input_kick_dwork, 0)) {
			innogov_notice("failed to start input kick dwork\n");
		}
	}

	return ;
}

static int inno_idle_check(struct devfreq *devfreq, unsigned long *freq)
{
	int util = 0;
	bool load_changed = false, input_kicked = false;
	int idle_exit_cnt = 0, idle_enter_cnt = 0;
	struct devfreq_inno_govdata *govdata = devfreq->data;
	static unsigned long pre_freq = 0;

	util = fh2m_hal_get_gpu_utils(govdata->pvrsrv_dev_node);
	if (util < 0 && util > 100) {
		govdata->idle_enter_cnt = 0;
		govdata->idle_exit_cnt = 0;
		innogov_err("util get failed util is %d\n", util);
		return INNO_FAILED;
	}

	if (govdata->load_state == LOAD_STATE_ACTIVE) { 	//LOAD_STATE_ACTIVE --> LOAD_STATE_IDLE: <=10
		if (util >=0 && util <= fh2m_hal_get_dyn_lpc_gpu_utils(govdata->ppci_bdev)) {
			govdata->idle_exit_cnt = 0;
			govdata->idle_enter_cnt++;
			if (govdata->idle_enter_cnt >= INNOIDLE_ENTER_IDLE_CONTINUOUS_DETECT_TIME) {
				govdata->load_state = LOAD_STATE_IDLE;
				*freq = govdata->user_powsave_freq + govdata->freq_offset;
				pre_freq = *freq;
				govdata->user_mode_powsave = true;

				load_changed = true;
			}
		} else {
			govdata->idle_enter_cnt = 0;
		}
	} else {
		if (util > (fh2m_hal_get_dyn_lpc_gpu_utils(govdata->ppci_bdev) + 10)) {
			govdata->idle_enter_cnt = 0;
			govdata->idle_exit_cnt++;
			if (govdata->idle_exit_cnt >= INNOIDLE_EXIT_IDLE_CONTINUOUS_DETECT_TIME) {
				govdata->load_state = LOAD_STATE_ACTIVE;
				*freq = govdata->default_freq + govdata->freq_offset;
				pre_freq = *freq;
				govdata->user_mode_powsave = false;

				load_changed = true;
			}
		} else {
			govdata->idle_exit_cnt = 0;
		}
	}

	if (!load_changed) {
		*freq = pre_freq;
	}

	/*clear cnt and kick data*/
	idle_exit_cnt = govdata->idle_exit_cnt;
	idle_enter_cnt = govdata->idle_enter_cnt;
	if (load_changed) {
		govdata->idle_exit_cnt = 0;
		govdata->idle_enter_cnt = 0;
	}

	if (govdata->input_kicked) {
		input_kicked = govdata->input_kicked;
		govdata->user_mode_powsave = false;
		govdata->load_state = LOAD_STATE_ACTIVE;
		*freq = govdata->default_freq + govdata->freq_offset;
		pre_freq = *freq;
		govdata->input_kicked = false;
	}

	innogov_dbg("govdata 0x%p util %d load[%s] idle_enter_cnt[%d %d] idle_exit_cnt[%d %d] " \
		"pre_freq = %d *freq = %d input_kicked[%d %d] load_changed[%d]\n", govdata,
		util, govdata->load_state == LOAD_STATE_IDLE ? "LOAD_STATE_IDLE" : "LOAD_STATE_ACTIVE",
		idle_enter_cnt, govdata->idle_enter_cnt, idle_exit_cnt, govdata->idle_exit_cnt, pre_freq,
		*freq, input_kicked, govdata->input_kicked, load_changed);
	return INNO_SUCC;
}

static void inno_input_kick_work(void *df)
{
	int err = 0;
	struct devfreq* devfreq = (struct devfreq* )df;

	mutex_lock(&devfreq->lock);
	err = update_devfreq(devfreq);
	if (err)
		innogov_notice("dvfs failed with (%d) error\n", err);
	mutex_unlock(&devfreq->lock);
}

static unsigned long devfreq_update_algo_pid(struct devfreq *dev)
{
	struct devfreq_inno_govdata *innogov_data = dev->data;
	struct innopower_temp_pidctl *pidc = &(innogov_data->pidc);
	struct innothermal *pinnothermal = innogov_data->thermal;
	struct thermal_zone_device *tz = pinnothermal->thermal_dev;

	long target_freq = 0, delta_freq = 0;
	long tolerance_win_min = 0, tolerance_win_max = 0;
	int i = 0, cur_temp = 0, cur_tmp_error = 0;

	if (!COMPAT_TZ_VALID(tz)) {
		innogov_notice("thermal_dev is null\n");
		return 0;
	}

	COMPAT_GET_TEMP(tz, &cur_temp);
	pidc->time++;
	if (pidc->time < pidc->timeout) {
		target_freq = pidc->real_target_freq;
		goto tget_skip;
	}
	pidc->time = 0;

	if (cur_temp < pidc->recover_temp) {
		cur_tmp_error = 0;
		pidc->pre_tmp_error = 0;
		pidc->ppre_tmp_error = 0;
		target_freq = pidc->max_freq;
		pidc->real_target_freq = target_freq;
		pidc->pid_out_freq = target_freq;
		goto tget_freq;
	}
	if (cur_temp > pidc->warnning_temp) {
		cur_tmp_error = 0;
		pidc->pre_tmp_error = 0;
		pidc->ppre_tmp_error = 0;
		target_freq = pidc->min_freq;
		pidc->real_target_freq = target_freq;
		pidc->pid_out_freq = target_freq;
		goto tget_freq;
	}

	cur_tmp_error = pidc->ref_temp - cur_temp;
	/*algo: u(k) - u(k-1) = P*[e(k) - e(k-1)] + I*e(k) + D*[e(k) - 2e(k-1) + e(k-2)] */
	delta_freq = pidc->P * (cur_tmp_error - pidc->pre_tmp_error) +
				pidc->I * cur_tmp_error +
				pidc->D * (cur_tmp_error - 2 * pidc->pre_tmp_error + pidc->ppre_tmp_error);

	/*u(k) = u(k-1) + delta*/
	target_freq = pidc->pid_out_freq + delta_freq * GPU_FREQ_M2HZ_UNIT;
	pidc->pid_out_freq = target_freq;

	/*make sure target freq is valid and avoid error is too big by parame  PID's I */
	if (target_freq <= pidc->min_freq) {
		target_freq = pidc->min_freq;
		pidc->pid_out_freq = target_freq;
	} else if (target_freq >= pidc->max_freq) {
		target_freq = pidc->max_freq;
		pidc->pid_out_freq = target_freq;
	} else {
		for (i = 0; i < PIDCTL_OUT_FREQ_NUM-1; i++) {
			if (target_freq == pidc->outfreq[i]) {
				goto tget_freq;
			}

			if (target_freq >= pidc->outfreq[i+1] &&
				target_freq < pidc->outfreq[i]) {

				tolerance_win_min = (pidc->outfreq[i] + pidc->outfreq[i+1])/2 - pidc->tolerance;
				tolerance_win_max = (pidc->outfreq[i] + pidc->outfreq[i+1])/2 + pidc->tolerance;

				/*tolerance adjust*/
				if (target_freq >= tolerance_win_min && target_freq <= tolerance_win_max) {
					target_freq = pidc->real_target_freq;
					goto tget_freq;
				}

				if (target_freq < tolerance_win_min) {
					target_freq = pidc->outfreq[i+1];
				} else {
					target_freq = pidc->outfreq[i];
				}
				goto tget_freq;
			}
		}
	}

tget_freq:
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) {
		innogov_info("time = %d temp=[%d, %d] err=[%d, %d, %d] PID=[%d, %d, %d]" \
			" delta_freq[%ld] target_freq=[pid:%ld, real_target%ld pre_real_target=%ld] min~max freq=[%ld, %ld]\n",
			pidc->time, cur_temp, pidc->ref_temp,
			cur_tmp_error, pidc->pre_tmp_error, pidc->ppre_tmp_error,
			pidc->P, pidc->I, pidc->D,
			delta_freq*GPU_FREQ_M2HZ_UNIT, pidc->pid_out_freq, target_freq, pidc->real_target_freq,
			pidc->min_freq, pidc->max_freq);
	}

	pidc->ppre_tmp_error = pidc->pre_tmp_error;
	pidc->pre_tmp_error = cur_tmp_error;
	pidc->real_target_freq = target_freq;
tget_skip:
	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) {
		innogov_info("time=%d, cur_temp, pid_out_freq, real_target_freq=[%d, %ld, %ld]",
			pidc->time, cur_temp, pidc->pid_out_freq, pidc->real_target_freq);
	}

	return (unsigned long)target_freq;
}

#define CHIP_TEMP_DROP_THD		85
#define CHIP_TEMP_RECOVER_THD	(CHIP_TEMP_DROP_THD-5)
#define BOARD_TEMP_DROP_THD		80
#define BOARD_TEMP_RECOVER_THD	(BOARD_TEMP_DROP_THD-5)
#define KEEP_DROPFREQ_STAT	1
#define EXIT_DROPFREQ_STAT	0
static unsigned long devfreq_update_algo_mix(struct devfreq *dev)
{
	struct devfreq_inno_govdata *innogov_data = dev->data;
	struct innothermal *pinnothermal = innogov_data->thermal;
	struct thermal_zone_device *tz = pinnothermal->thermal_dev;
	struct thermal_zone_device *btz = pinnothermal->thermal_bdev;

	int chip_temp = 0;
	int board_temp = 0;
	long target_freq = 0;

	if (!COMPAT_TZ_VALID(tz)) {
		innogov_notice("chip thermal_dev is null\n");
		return 0;
	}

	if (!COMPAT_TZ_VALID(btz)) {
		innogov_notice("board thermal_dev is null\n");
		return 0;
	}

	COMPAT_GET_TEMP(tz, &chip_temp);
	COMPAT_GET_TEMP(btz, &board_temp);

	if (innogov_data->keep_dropfreq_stat == KEEP_DROPFREQ_STAT) {
		target_freq = fh2m_hal_get_gpu_drop_freq(pinnothermal->ppci_bdev) * GPU_FREQ_M2HZ_UNIT;			/*400M*/
	} else {
		target_freq = fh2m_hal_get_gpu_recover_freq(pinnothermal->ppci_bdev) * GPU_FREQ_M2HZ_UNIT;		/*1000M*/
	}

	if (chip_temp >= CHIP_TEMP_DROP_THD || board_temp >= BOARD_TEMP_DROP_THD) {
		target_freq = fh2m_hal_get_gpu_drop_freq(pinnothermal->ppci_bdev) * GPU_FREQ_M2HZ_UNIT;
		innogov_data->keep_dropfreq_stat = KEEP_DROPFREQ_STAT;
	} else {
		if (chip_temp < CHIP_TEMP_RECOVER_THD && board_temp < BOARD_TEMP_RECOVER_THD) {
			target_freq = fh2m_hal_get_gpu_recover_freq(pinnothermal->ppci_bdev) * GPU_FREQ_M2HZ_UNIT;
			innogov_data->keep_dropfreq_stat = EXIT_DROPFREQ_STAT;
		}
	}
	innogov_info("devfreq--%d-- chip_temp = %d board_temp = %d target_freq = %ld\n\n",innogov_data->id, chip_temp, board_temp, target_freq);

	return target_freq;
}

static unsigned long devfreq_update_by_thermal(struct devfreq *dev)
{
	struct devfreq_inno_govdata *gov_data = dev->data;
	struct innothermal *pinnothermal = gov_data->thermal;
	int dynfreq_algo = 0;

	if (fh2m_inno_is_err_or_null(pinnothermal)) {
		innogov_notice("pinnothermal is null\n");
		return 0;
	}

	if (fh2m_inno_is_err_or_null(pinnothermal->ppci_bdev)) {
		innogov_notice("pinnothermal->ppci_bdev is null\n");
		return 0;
	}

	dynfreq_algo = fh2m_hal_get_dynfreq_algo(pinnothermal->ppci_bdev);
	if (dynfreq_algo == DYNFREQ_ALGO_MIX) {
		return devfreq_update_algo_mix(dev);
	} else if (dynfreq_algo == DYNFREQ_ALGO_PID){
		return devfreq_update_algo_pid(dev);
	} else {
		return 0;
	}
}


#define inno_devfreq(DEV)	container_of((DEV), struct devfreq, dev)
static ssize_t store_level(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct power_userspace_data *data;
	struct devfreq_inno_govdata *innogov_data = devfreq->data;
	unsigned long wanted;
	unsigned long max_lvl = innogov_data->userinfo.tbl_size - 1;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = &(innogov_data->userinfo);

	sscanf(buf, "%lu", &wanted);
	if (wanted > max_lvl) {
		mutex_unlock(&devfreq->lock);
		err = count;
		goto err_range_out;
	}
	data->userlevel = wanted;
	data->valid = true;
	err = update_devfreq(devfreq);
	if (err == 0)
		err = count;
	mutex_unlock(&devfreq->lock);

	innogov_notice("lvl update success, err = %d count=%lu cur userlevel[%lu]\n", err, count, data->userlevel);
	return err;

err_range_out:
	innogov_notice("lvl update failed, for maxlevel is %ld, err = %d count=%lu cur userlevel[%lu]\n", max_lvl, err, count, data->userlevel);
	return err;
}

static ssize_t show_level(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct power_userspace_data *data;
	struct devfreq_inno_govdata *innogov_data = devfreq->data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = &(innogov_data->userinfo);

	if (data->valid)
		err = sprintf(buf, "%lu\n", data->userlevel);
	else
		err = sprintf(buf, "undefined\n");
	mutex_unlock(&devfreq->lock);
	return err;
}

static DEVICE_ATTR(level, 0644, show_level, store_level);

#define inno_devfreq(DEV)	container_of((DEV), struct devfreq, dev)
static ssize_t store_inno_devfreq_enable(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct devfreq_inno_govdata *innogov_data = devfreq->data;

	if (!innogov_data) {
		return count;
	}

	mutex_lock(&devfreq->lock);
	sscanf(buf, "%u", &innogov_data->enable);
	mutex_unlock(&devfreq->lock);

	innogov_notice("devfreq enable=%u\n", innogov_data->enable);
	return count;
}

static ssize_t show_inno_devfreq_enable(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct devfreq_inno_govdata *innogov_data = devfreq->data;
	int err = 0;

	if (!innogov_data) {
		sprintf(buf, "undefined data\n");
		return 1;
	}

	mutex_lock(&devfreq->lock);
	err = sprintf(buf, "%u\n", innogov_data->enable);
	mutex_unlock(&devfreq->lock);

	return err;
}
static DEVICE_ATTR(inno_devfreq_enable, 0644, show_inno_devfreq_enable, store_inno_devfreq_enable);

static ssize_t store_user_mode_powsave(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct devfreq_inno_govdata *innogov_data = devfreq->data;
	unsigned int wanted;
	int err = 0;

	mutex_lock(&devfreq->lock);

	sscanf(buf, "%u", &wanted);
	innogov_data->user_mode_powsave = wanted;
	err = update_devfreq(devfreq);
	if (err == 0)
		err = count;
	mutex_unlock(&devfreq->lock);

	innogov_notice("err = %d count=%lu user_mode_powsave=%u\n", err, count, innogov_data->user_mode_powsave);
	return err;
}

static ssize_t show_user_mode_powsave(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = inno_devfreq(dev);
	struct devfreq_inno_govdata *innogov_data = devfreq->data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	err = sprintf(buf, "%u\n", innogov_data->user_mode_powsave);
	mutex_unlock(&devfreq->lock);

	return err;
}
static DEVICE_ATTR(user_mode_powsave, 0644, show_user_mode_powsave, store_user_mode_powsave);

static struct attribute *dev_entries[] = {
	&dev_attr_level.attr,
	&dev_attr_inno_devfreq_enable.attr,
	&dev_attr_user_mode_powsave.attr,
	NULL,
};

static const struct attribute_group dev_attr_group = {
	.name	= "user",
	.attrs	= dev_entries,
};

static int userspace_init(struct devfreq *devfreq)
{
	int err = 0;

	err = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);

	return err;
}

static void userspace_exit(struct devfreq *devfreq)
{
	/*
	 * Remove the sysfs entry, unless this is being called after
	 * device_del(), which should have done this already via kobject_del().
	 */
	if (devfreq->dev.kobj.sd)
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
}

static int devfreq_update_by_userelevel(struct devfreq * df)
{
	struct devfreq_inno_govdata *innogov_data = df->data;
	struct power_userspace_data *data;
	unsigned long target_freq = 0;
	if (!innogov_data) {
		innogov_notice("innogov_data is null\n");
		return 0;
	}

	data = &innogov_data->userinfo;
	if (data->userlevel < data->tbl_size) {
		target_freq = data->level2freq[data->userlevel];
	} else {
		target_freq = innogov_data->default_freq;
	}

	return target_freq;
}

static int devfreq_get_target_freq(struct devfreq *df,
					unsigned long *freq)
{
	unsigned long tfreq = 0;
	unsigned long user_setfreq = 0;
	unsigned long target_freq = 0;
	struct devfreq_inno_govdata *innogov_data = df->data;

	if (!innogov_data) {
		innogov_notice("innogov_data is null\n");
		return -1;
	}

	if (fh2m_get_pwr_debug_lvl() >= PWRD_LVL_INFO) {
		innogov_notice("devfreq %d is triggered\n",innogov_data->id);
	}
	//get devfreq_private
	if (innogov_data->curr_freq == 0) {
		innogov_data->curr_freq = innogov_data->default_freq;
	}

	/*enble pm & dvfs for debug*/
	if (!innogov_data->enable) {
		*freq = innogov_data->default_freq + innogov_data->freq_offset;
		return 0;
	}

	//only dyn mode, update freq by temperature and gpu uselization
	if (innogov_data->userinfo.user_pm_mode == HAL_GPU_WORK_MODE_DYN) {
		inno_idle_check(df, freq);
		if (innogov_data->user_mode_powsave) {
			*freq = innogov_data->user_powsave_freq + innogov_data->freq_offset;
			innogov_info("user_powsave_freq = %lu\n", *freq);
			return 0;
		}

		user_setfreq = devfreq_update_by_userelevel(df);
		tfreq = devfreq_update_by_thermal(df);

		if (user_setfreq == 0 && tfreq == 0) {
			target_freq = innogov_data->curr_freq;
		} else {
			if (user_setfreq == 0) {
				target_freq = tfreq;
			} else if (tfreq == 0) {
				target_freq = user_setfreq;
			} else {
				target_freq = (user_setfreq < tfreq) ? user_setfreq : tfreq;
			}
		}
	}

	*freq = target_freq + innogov_data->freq_offset;
	innogov_data->curr_freq = *freq;
	//innogov_err("%s: %d\n", __func__, __LINE__);

	return 0;
}

static void inno_devfreq_monitor_work(void *df)
{
	int err;
	struct devfreq* devfreq = (struct devfreq* )df;
	struct devfreq_inno_govdata *govdata = devfreq->data;

	mutex_lock(&devfreq->lock);
	err = update_devfreq(devfreq);
	if (err)
		innogov_notice("dvfs failed with (%d) error\n", err);

	fh2m_inno_queue_dwork(govdata->devfreq_wkq, govdata->devfreq_dwork,
				(devfreq->profile->polling_ms));
	mutex_unlock(&devfreq->lock);
}

static int devfreq_wkq = 0;
static void inno_devfreq_monitor_start(struct devfreq *devfreq)
{
	struct devfreq_inno_govdata *govdata = devfreq->data;

	/*init work*/
	sprintf(govdata->name, "%s-%d", "devfreq_wkq", devfreq_wkq);

	govdata->devfreq_wkq = fh2m_inno_create_singlethread_wq("devfreq_wkq");
	if (IS_ERR_OR_NULL(govdata->devfreq_wkq)) {
		innogov_notice("devfreq_wkq alloc is failed\n");
		goto err_end;
	}

	/*start temperature monitor*/
	govdata->devfreq_dwork = fh2m_inno_dwork_alloc(inno_devfreq_monitor_work, devfreq);
	if (IS_ERR_OR_NULL(govdata->devfreq_dwork)) {
		innogov_notice("failed to start temperature monitor\n");
		goto err_mwkq_alloc;
	}

	govdata->input_kick_dwork = fh2m_inno_dwork_alloc(inno_input_kick_work, devfreq);
	if (IS_ERR_OR_NULL(govdata->input_kick_dwork)) {
		innogov_notice("failed to start input event monitor\n");
		goto err_mdwk_alloc;
	}

	if (!fh2m_inno_queue_dwork(govdata->devfreq_wkq, govdata->devfreq_dwork, 0)) {
		innogov_notice("failed to start temperature monitor\n");
		goto err_inputwk_alloc;
	}

	innogov_dbg("run finished start %s successfully\n", govdata->name);
	inno_devfreq_bind_input_handler(NULL, devfreq);
	devfreq_wkq++;

	return;

err_inputwk_alloc:
	fh2m_inno_dwork_destroy(govdata->input_kick_dwork);
err_mdwk_alloc:
	fh2m_inno_dwork_destroy(govdata->devfreq_dwork);
err_mwkq_alloc:
	fh2m_inno_destroy_workqueue(govdata->devfreq_wkq);
err_end:
	govdata->input_kick_dwork = NULL;
	govdata->devfreq_dwork = NULL;
	govdata->devfreq_wkq = NULL;
	innogov_notice("start %s failed\n", govdata->name);

	return;
}

static void inno_devfreq_monitor_stop(struct devfreq *devfreq)
{
	struct devfreq_inno_govdata *govdata = devfreq->data;

	inno_devfreq_unbind_input_handler();

	fh2m_inno_cancel_dwork_sync(govdata->input_kick_dwork);

	fh2m_inno_cancel_dwork_sync(govdata->devfreq_dwork);

	fh2m_inno_flush_workqueue(govdata->devfreq_wkq);

	fh2m_inno_dwork_destroy(govdata->input_kick_dwork);

	fh2m_inno_dwork_destroy(govdata->devfreq_dwork);

	fh2m_inno_destroy_workqueue(govdata->devfreq_wkq);

	innogov_dbg("run finished\n");
}

struct inno_dwork {
	struct delayed_work dwork;
	void (*func)(void *data);
	void *data;
};

static void inno_devfreq_monitor_resume(struct devfreq *devfreq)
{
	struct devfreq_inno_govdata *govdata = devfreq->data;
	unsigned long freq;
	inno_dwork *devfreq_dwork = govdata->devfreq_dwork;
	struct inno_dwork *dwk = (struct inno_dwork *)devfreq_dwork;
	struct delayed_work *dwork = &(dwk->dwork);

	mutex_lock(&devfreq->lock);
	if (!devfreq->stop_polling)
		goto out;

	if (!delayed_work_pending(dwork) &&
			devfreq->profile->polling_ms) {
		fh2m_inno_queue_dwork(govdata->devfreq_wkq, govdata->devfreq_dwork, devfreq->profile->polling_ms);

		if (govdata->support_idle_switch) {
			fh2m_inno_queue_dwork(govdata->devfreq_wkq, govdata->input_kick_dwork, 0);
			inno_input_resume();
		}
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0))
	devfreq->last_stat_updated = jiffies;
#else
	devfreq->stats.last_update = get_jiffies_64();
#endif
	devfreq->stop_polling = false;

	if (devfreq->profile->get_cur_freq &&
		!devfreq->profile->get_cur_freq(devfreq->dev.parent, &freq))
		devfreq->previous_freq = freq;

out:
	mutex_unlock(&devfreq->lock);
	innogov_notice("resume done\n");
}

static void inno_devfreq_monitor_suspend(struct devfreq *devfreq)
{
	struct devfreq_inno_govdata *govdata = devfreq->data;

	mutex_lock(&devfreq->lock);
	if (devfreq->stop_polling) {
		mutex_unlock(&devfreq->lock);
		return;
	}

	devfreq_update_status(devfreq, devfreq->previous_freq);
	devfreq->stop_polling = true;
	mutex_unlock(&devfreq->lock);
	if (govdata->support_idle_switch) {
		inno_input_suspend();
		fh2m_inno_cancel_dwork_sync(govdata->input_kick_dwork);
	}
	fh2m_inno_cancel_dwork_sync(govdata->devfreq_dwork);
	innogov_notice("suspend done\n");
}

static int devfreq_inno_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		userspace_init(devfreq);
		inno_devfreq_monitor_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		userspace_exit(devfreq);
		inno_devfreq_monitor_stop(devfreq);
		break;
/*
	case DEVFREQ_GOV_INTERVAL:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
		devfreq_interval_update(devfreq, (unsigned int *)data);
#else
		devfreq_update_interval(devfreq, (unsigned int *)data);
#endif
		break;
*/
	case DEVFREQ_GOV_SUSPEND:
		inno_devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		inno_devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return 0;
}

struct devfreq_governor inno_devfreq_gov = {
	.name = "inno_dfreq_gov",
	.get_target_freq = devfreq_get_target_freq,
	.event_handler = devfreq_inno_handler,
};

int inno_devfreq_gov_register(void)
{
	return devfreq_add_governor(&inno_devfreq_gov);
}

int inno_devfreq_gov_unregister(void)
{
	int err = 0;

	err =  devfreq_remove_governor(&inno_devfreq_gov);
	innogov_info("%s err = %d\n", err);

	return err;
}
#endif
