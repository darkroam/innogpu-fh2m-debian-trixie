#ifndef __INNO_VGA_COMMON_H__
#define __INNO_VGA_COMMON_H__

#include "inno_misc.h"
#include "inno_plat_dev.h"
#include "inno_task.h"
#include "inno_lock.h"
#include "inno_timer.h"
#include "inno_mm.h"
#include "inno_waitqueue.h"
#include "inno_drm.h"
#include "inno_drm_mode.h"
#include "inno_fs.h"

#include "innodpu_common.h"
#include "innodpu_connector.h"

#define INNOVGA_FRAME_RATE_30	(30)
#define INNOVGA_FRAME_RATE_60	(60)
#define INNOVGA_DEFAULT_FRAME_RATE	(INNOVGA_FRAME_RATE_60)

#define INNOVGA_PCLK_148_50M					(148500)
#define INNOVGA_PCLK_138_50M					(138500)
#define INNOVGA_PCLK_146_25M					(146250)
#define INNOVGA_PCLK_121_75M					(121750)
#define INNOVGA_PCLK_108_00M					(108000)
#define INNOVGA_PCLK_106_50M					(106500)
#define INNOVGA_PCLK_101_00M					(101000)
#define INNOVGA_PCLK_88_75M						(88750)
#define INNOVGA_PCLK_85_50M						(85500)
#define INNOVGA_PCLK_83_50M						(83500)
#define INNOVGA_PCLK_75_00M						(75000)
#define INNOVGA_PCLK_74_25M						(74250)
#define INNOVGA_PCLK_72_00M						(72000)
#define INNOVGA_PCLK_65_00M						(65000)
#define INNOVGA_PCLK_40_00M						(40000)
#define INNOVGA_PCLK_36_00M						(36000)
#define INNOVGA_PCLK_27_00M						(27000)
#define INNOVGA_PCLK_25_175M					(25175)
#define INNOVGA_PCLK_AUTO_CALC 					(0)

#define VGA_MONITOR_MASK (0x3)
enum vga_monitor_mode {
	/* ddc channels access mode */
#define DDC_ACCESS_NONE  (0)
#define DDC_ACCESS_PMBUS (1)
#define DDC_ACCESS_GPIO  (2)
#define DDC_ACCESS_DDCCI (3)
	ddc_access_mode = 0,

	/* connection status detection mode */
#define CON_DETECT_CABLE (0)
#define CON_DETECT_DDC   (1)
	con_detect_mode = 1,

	/* vga auto set enalbe */
#define VGA_SETUP_DISABLE (0)
#define VGA_SETUP_EN      (1)
	vga_setup = 2,

	/* colorbar test enable */
#define COLORBAR_DISABLE (0)
#define COLORBAR_EN      (1)
	colorbar_display = 3,

	/* vga auto setup every mode */
#define VGA_SETUP_EVERY_MODE_DISABLE (0)
#define VGA_SETUP_EVERY_MODE_EN      (1)
	vga_setup_every_mode = 4,

	vga_monitor_max,
};

struct vga_chip_t {
	char *name;
	inno_dev *dev;
	inno_drm_device *drm_dev;
	inno_dev *parent;
	const inno_drm_display_mode *adjusted_mode;
	bool test_mode;

	unsigned int reg_module;
	unsigned int hal_module;
	unsigned int possible_crtc;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int max_pclk_rx;

	inno_mutex *edid_mutex;

	//struct mutex i2c_mlock;
	bool edid_used_i2c;
	//bool support_i2c;
	//struct i2c_adapter adapter;
	//struct i2c_algo_bit_data bit_data;
	int modes;
	int  hal_edid_mode;
	unsigned char edid_buf[INNOVGA_EDID_BUF_LEN];
	struct edid *vga_edid;

	int (*hw_init)(struct vga_chip_t *chip);
	void (*hw_fini)(struct vga_chip_t *chip);
	unsigned int (*irq_handle)(struct vga_chip_t *chip);
	void (*irq_enable)(struct vga_chip_t *chip, unsigned int flag);
	void (*irq_disable)(struct vga_chip_t *chip, unsigned int flag);

	// encoder funcs
	int (*encoder_atomic_check)(struct vga_chip_t *chip,
		inno_drm_crtc_state *crtc_state, inno_drm_connector_state *conn_state);
	inno_drm_mode_status (*encoder_mode_valid)(struct vga_chip_t *chip,
		const inno_drm_display_mode *mode);
	void (*encoder_modeset)(struct vga_chip_t *chip,
		int dpu_id, bool test_mode, inno_drm_display_mode *mode);
	void (*encoder_disable)(struct vga_chip_t *chip);
	void (*encoder_enable)(struct vga_chip_t *chip, inno_drm_crtc *crtc);

	// connector funcs
	int (*connector_get_edid)(struct vga_chip_t *chip);
	inno_drm_mode_status (*connector_mode_valid)(struct vga_chip_t *chip,
		inno_drm_display_mode *mode);
	int (*connector_detect)(struct vga_chip_t *chip);

	// edid info
	void (*vga_edid_parse)(inno_seq_file *seq, struct vga_chip_t *chip);

	void (*auto_calibration_get)(struct vga_chip_t *chip, u8 buf[], u8 len);
	void (*auto_calibration_set)(struct vga_chip_t *chip, u8 buf[], u8 len);
};

#endif

