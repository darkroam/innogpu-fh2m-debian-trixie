#ifdef CONFIG_PM_DEVFREQ
#ifndef INNO_DEVFREQ_GOV_H
#define INNO_DEVFREQ_GOV_H

#include <linux/devfreq.h>
#include "inno_drm_version.h"
#define to_devfreq(DEV)         container_of((DEV), struct devfreq, dev)

/* Devfreq events */
#define DEVFREQ_GOV_START       0x1
#define DEVFREQ_GOV_STOP        0x2
#define DEVFREQ_GOV_INTERVAL    0x3
#define DEVFREQ_GOV_SUSPEND     0x4
#define DEVFREQ_GOV_RESUME      0x5

/* Default constants for DevFreq-Simple-Ondemand (DFSO) */
#define INNO_DFSO_UPTHRESHOLD        (90)
#define INNO_DFSO_DOWNDIFFERENCTIAL  (5)
#define INNO_DEVFREQ_NAME_LEN        (16)

#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#if (DRM_VERSION >= KERNEL_VERSION(5, 11, 0))
struct devfreq_governor {
	struct list_head node;

	const char name[INNO_DEVFREQ_NAME_LEN];
	const u64 attrs;
	const u64 flags;
	int (*get_target_freq)(struct devfreq *this, unsigned long *freq);
	int (*event_handler)(struct devfreq *devfreq,
				unsigned int event, void *data);
};
#elif (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
struct devfreq_governor {
	struct list_head node;

	const char name[INNO_DEVFREQ_NAME_LEN];
	const unsigned int immutable;
	const unsigned int interrupt_driven;
	int (*get_target_freq)(struct devfreq *this, unsigned long *freq);
	int (*event_handler)(struct devfreq *devfreq,
				unsigned int event, void *data);
};
#else
struct devfreq_governor {
	struct list_head node;

	const char name[INNO_DEVFREQ_NAME_LEN];
	const unsigned int immutable;
	int (*get_target_freq)(struct devfreq *this, unsigned long *freq);
	int (*event_handler)(struct devfreq *devfreq, unsigned int event, void *data);
};
#endif
#endif

struct devfreq_info {
	bool inited;
	void *osdev;
	void *devfreq;
};

/* Caution: devfreq->lock must be locked before calling update_devfreq */
extern int update_devfreq(struct devfreq *devfreq);
extern void devfreq_monitor_start(struct devfreq *devfreq);
extern void devfreq_monitor_stop(struct devfreq *devfreq);
extern void devfreq_monitor_suspend(struct devfreq *devfreq);
extern void devfreq_monitor_resume(struct devfreq *devfreq);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
extern void devfreq_interval_update(struct devfreq *devfreq, unsigned int *delay);
#else
extern void devfreq_update_interval(struct devfreq *devfreq, unsigned int *delay);
#endif
extern int devfreq_add_governor(struct devfreq_governor *governor);
extern int devfreq_remove_governor(struct devfreq_governor *governor);
extern int devfreq_update_status(struct devfreq *devfreq, unsigned long freq);

int inno_devfreq_gov_register(void);
int inno_devfreq_gov_unregister(void);
void inno_input_kick(void *dev);
struct devfreq_info* inno_get_devfreq_info(void);
#endif /*INNO_DEVFREQ_GOV_H*/
#endif
