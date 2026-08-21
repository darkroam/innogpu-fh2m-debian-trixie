#ifndef __INNOAUDIO_CHIP_COMMON_H
#define __INNOAUDIO_CHIP_COMMON_H

#include <linux/types.h>

#define MAX_ELD_BYTES	128
#define MAX_EDID_BYTES	128


#define  PERIOD_BYTES_MIN 5
#define  PERIOD_BYTES_MAX 10
#define  BUFFER_BYTES_MAX 40

#define INNOAUDIO_CONNECTOR_TYPE_HDMI 		1
#define INNOAUDIO_CONNECTOR_TYPE_DP   		2
#define INNOAUDIO_CONNECTOR_TYPE_HDMI2DP 	3
#define INNOAUDIO_CONNECTOR_TYPE_DP2HDMI 	4

struct audio_chip_t {
	char *name;
	void *parent;
	void *dev;
	int id;
	int type;
	unsigned int reg_module;
	unsigned int hal_module;
	unsigned int audio_buf_size;
	void *audio_playback_sub;
	void *audio_reg_lock;
	int pm_st;

	bool irq_flag;
	u64 irq_cnt;
	int irq_buf;
	u64 irq_time;
	//next 3 members are used to adjust buffer in KB
	int buf_max;
	int pmax;
	int pmin;
	int off_fix; //in byte
	unsigned long offset_time;

	unsigned int buf_offset;
	unsigned int buf_period;
	unsigned int buf_size;
	u64 buf_start;
	u64 buf_end;

	int reg_len;

	int (*hw_init)(struct audio_chip_t *chip);
	void (*chip_fini)(struct audio_chip_t *chip);
	void (*hw_close)(struct audio_chip_t *chip);
	void (*irq_handle)(struct audio_chip_t *chip);
	void (*irq_enable)(struct audio_chip_t *chip);
	void (*irq_disable)(struct audio_chip_t *chip);
	void (*audio_buf_set)(struct audio_chip_t *chip);
	int (*get_offset)(struct audio_chip_t *chip);
	int (*audio_suspend)(struct audio_chip_t *chip);
	int (*audio_resume)(struct audio_chip_t *chip);
	void (*init_hw_param)(struct audio_chip_t *chip, void *hw_param);

	int (*get_curr_buf)(struct audio_chip_t *chip);
	void (*start)(struct audio_chip_t *chip);
	void (*stop)(struct audio_chip_t *chip);
	void (*pause)(struct audio_chip_t *chip, int en);
	void (*set_clk)(struct audio_chip_t *chip);
	void (*set_output)(struct audio_chip_t *chip, int id, int st);
	int (*init_connector)(struct audio_chip_t *chip, int type, int src_id, char *name, int name_len);

	int (*reg_show)(struct audio_chip_t *chip, char *buf);
	unsigned long paddr;		//dev pa
	unsigned long baddr;		//cpu pa
	void __iomem *vaddr;		//cpu va
};

extern int innoaudio_chip_init(struct audio_chip_t *chip);
extern void innoaudio_init_hw_param(struct audio_chip_t *chip);
extern int innoaudio_get_offset(struct audio_chip_t *chip);
extern int innoaudio_substream_ctrl(struct audio_chip_t *chip, int cmd);
extern void innoaudio_hw_init(struct audio_chip_t *chip);
extern void innoaudio_hw_close(struct audio_chip_t *chip);
extern void innoaudio_hw_pause(struct audio_chip_t *chip, int st);
extern void innoaudio_hw_stop(struct audio_chip_t *chip);
extern void innoaudio_hw_start(struct audio_chip_t *chip);
extern void innoaudio_hw_set_output(struct audio_chip_t *chip, int id, int st);
extern void innoaudio_chip_fini(struct audio_chip_t *chip);
extern int innoaudio_suspend(struct audio_chip_t *chip);
extern int innoaudio_resume(struct audio_chip_t *chip);

extern int g0_soc_audio_chip_init(struct audio_chip_t *chip);
extern int g0m_soc_audio_chip_init(struct audio_chip_t *chip);
extern int g1_soc_audio_chip_init(struct audio_chip_t *chip);
extern int g1p_soc_audio_chip_init(struct audio_chip_t *chip);

#endif
