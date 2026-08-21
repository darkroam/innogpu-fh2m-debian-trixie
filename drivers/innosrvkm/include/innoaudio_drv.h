/*************************************************************************/ /*!
@File			innoaudio_drv.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/


#ifndef __INNOAUDIO_H_
#define __INNOAUDIO_H_
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/suspend.h>


#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>
#include <sound/jack.h>

#include <asm/device.h>
#include "innoaudio_chip_common.h"
#include "innoaudio_print.h"

#include "hal_interface.h"
#include "hal.h"


#define INNOAUDIO_SAMPLE_RATE_128					(128000)
#define INNOAUDIO_SAMPLE_RATE_96					(96000)
#define INNOAUDIO_SAMPLE_RATE_48					(48000)

#define	PLL5_1_FOR_48_RATE							(0x08004348)
#define	PLL5_2_FOR_48_RATE							(0x00000019)
#define	PLL5_1_FOR_96_RATE							(0x10014348)
#define	PLL5_2_FOR_96_RATE							(0x00000005)
#define	PLL5_1_FOR_192_RATE							(0x20014348)	//(0x08004348)
#define	PLL5_2_FOR_192_RATE							(0x00000005)	//(0x00000006)
#define	PLL5_1_FOR_44_RATE							(0x07504348)
#define	PLL5_2_FOR_44_RATE							(0x99999919)

/*======================================== function =====================================*/

#define INNOAUDIO_CONNECTOR_NUM 3
#define INNOAUDIO_HDMI_NUMS 1	//HAL_MAX_HDMI_NUMS todo
#define INNOAUDIO_DP_NUMS   HAL_MAX_DP_NUMS
#define DP_INDEX  HDMI_NUMS

#define INNOAUDIO_CTRL_TYPE_SWITCH 0
#define INNOAUDIO_CTRL_TYPE_ELD  1

struct audio_notify_data {
	int format;
	unsigned int rate;
	unsigned int channels;
	unsigned long period_size;
	unsigned int periods;
	unsigned int frame_bits;
	unsigned int sample_bits;
	int consumed;
};

struct inno_ctrl_t {
	const struct snd_kcontrol_new *kctrl;
	//struct audio_conn *ac;

	char name[8];
	unsigned int type;
};

struct audio_conn {
	void *dev;    //connector dev
	char name[8];
	int conn_st;  //plug-in: 1, pulg-out: 0
	int audio_st;
	int id;		  //0~15
	int type;	  //INNOAUDIO_CONNECTOR_TYPE_HDMI/_DP
	int has_audio;//is hdmi/dp audio detected
	int edid_changed;
	int dpms;
	int is_choosen;
	unsigned char *eld;
	unsigned char *edid;
	void *priv;  // used by connector

	struct audio_device_t *inno_audio;
	struct audio_notify_data data;
	struct list_head list;

	struct snd_jack *jack;
	unsigned int jack_st;

	struct inno_ctrl_t *sw_ctrl;

	void (*enable)(struct audio_conn *ac);
	void (*disable)(struct audio_conn *ac);
	void (*report_jack)(struct audio_conn *ac, int dpms_st, int conn_st);
	void (*update_eld)(struct audio_conn *ac, char *buf, int size);
};

struct audio_device_t {
	struct platform_device *pfdev;
	struct dev_rsrc *pdev_rsrc;
	struct timer_list chk_tm;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct list_head conn_list;

	int card_id;
	int ac_num;
	struct mutex mutex; /* for protecting chmap and eld */
	struct audio_chip_t chip;
	struct notifier_block pm_nb;
};

enum sample_rates {
	SAMPLE_RATE_48 = 1,
	SAMPLE_RATE_96,
	SAMPLE_RATE_192 = 4,
};

extern int  fh2m_innoaudio_register_connector(struct         audio_conn *ac);
extern int  fh2m_innoaudio_unregister_connector(struct          audio_conn *ac);

#endif
