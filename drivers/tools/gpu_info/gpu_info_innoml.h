#ifndef GPU_INFO_INNOML_H
#define GPU_INFO_INNOML_H

#include "inno_plat_dev.h"
#include "inno_debug.h"

#define KBUILD_HAL "innoml"
#define pr_fmt_ml(fmt) "[%s][%s:%d]" fmt,KBUILD_HAL,__func__,__LINE__

#define innoml_error(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_ERR pr_fmt_ml(fmt), ##__VA_ARGS__); \
	} while (0);

#define innoml_warning(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_WARNING pr_fmt_ml(fmt), ##__VA_ARGS__); \
	} while (0);

#define innoml_debug(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_DEBUG pr_fmt_ml(fmt), ##__VA_ARGS__); \
	} while (0);

#define innoml_info(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_INFO pr_fmt_ml(fmt), ##__VA_ARGS__); \
	} while (0);

#if defined(NO_HARDWARE)
static inline PVRSRV_ERROR InnoGpuCharDevInitDriver(void)
{
    return 0;
}
static inline PVRSRV_ERROR InnoGpuCharDevDeInitDriver(void)
{
    return 0;
}
static inline PVRSRV_ERROR InnoGpuCharDevInitDevice(inno_dev *pdev)
{
    return 0;
}
static inline PVRSRV_ERROR InnoGpuCharDevDeInitDevice(inno_dev *pdev)
{
    return 0;
}
#else
int InnoGpuCharDevInitDriver(void);
void InnoGpuCharDevDeInitDriver(void);
int InnoGpuCharDevInitDevice(inno_dev *pdev);
void InnoGpuCharDevDeInitDevice(inno_dev *pdev);
#endif

#endif
