#ifndef RGXGPUUTIL_H
#define RGXGPUUTIL_H

#include <linux/miscdevice.h>
#define CMD_IOC_MAGIC 'k'

#define CMD_IOC_NULL _IO(CMD_IOC_MAGIC, 0)

#define CMD_IOC_SET _IOW(CMD_IOC_MAGIC, 1, int)
#define CMD_IOC_GET _IOR(CMD_IOC_MAGIC, 2, int)
#define CMD_IOC_GET_REM _IOR(CMD_IOC_MAGIC, 3, int)

int inno_gpu_util_init(void);
void inno_gpu_util_exit(void);

#endif
