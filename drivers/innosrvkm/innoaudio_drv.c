/*************************************************************************/ /*!
@File			innoaudio_drv.c
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
#include "inno_misc.h"
#include "inno_timer.h"
#include "linux/stddef.h"
#include <linux/mod_devicetable.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include "innoaudio_drv.h"
#include "kernel_compatibility.h"
#include "syscommon.h"
#include "innoaudio_chip_common.h"
#include "innoaudio_print.h"
#include "inno_mm.h"
#include "inno_drm_version.h"

bool s_audio_debug = false;
module_param(s_audio_debug, bool, 0600);
MODULE_PARM_DESC(s_audio_debug, "audio debug info");

#define CARD_CTRL_HDMI 0
#define CARD_CTRL_DP   2
#define CARD_CTRL_ELD  4

#define CONNECTOR_DPMS_ST_ON 	0
#define CONNECTOR_DPMS_ST_OFF 	3
#define CONNECTOR_JACK_ON 	SND_JACK_AVOUT
#define CONNECTOR_JACK_OFF	0

struct audio_device_t *s_card_data[SNDRV_CARDS];

static int connector_is_choosen(struct audio_conn *pac)
{
	return pac->is_choosen;
}


static void audio_notifier_data_get(struct audio_device_t *inno_audio,
									struct snd_pcm_runtime *runtime)
{
	struct audio_conn *pac = NULL;

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(connector_is_choosen(pac)){
			innoaudio_info(&inno_audio->pfdev->dev, "notify connector %d", pac->id);
			pac->data.rate = runtime->rate;
			pac->data.channels = runtime->channels;
			pac->data.format = runtime->format;
			pac->data.sample_bits = runtime->sample_bits;
			pac->data.consumed = 0;
		}
	}
}

static int ctl_conn_info(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_info *uinfo)
{
	struct audio_conn *ac = kcontrol->private_data;
	struct audio_device_t *inno_audio = ac->inno_audio;
	unsigned long flags;

	if(!inno_audio){
		innoaudio_err("ctl conn get error\n");
		return -EFAULT;
	}

	fh2m_inno_spin_lock_irqsave(inno_audio->chip.audio_reg_lock, &flags);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	fh2m_inno_spin_unlock_irqrestore(inno_audio->chip.audio_reg_lock, flags);

	innoaudio_info(&inno_audio->pfdev->dev, "\"%s\" typed", kcontrol->id.name);
	return 0;
}

static int ctl_conn_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct audio_conn *ac = kcontrol->private_data;
	struct audio_device_t *inno_audio = ac->inno_audio;
	unsigned long flags;

	if(!inno_audio){
		innoaudio_err("ctl conn get error\n");
		return -EFAULT;
	}

	innoaudio_info(&inno_audio->pfdev->dev, "\"%s\" typed, get val: %d",
							kcontrol->id.name, ac->is_choosen);
	fh2m_inno_spin_lock_irqsave(inno_audio->chip.audio_reg_lock, &flags);
	ucontrol->value.integer.value[0] = ac->is_choosen;
	fh2m_inno_spin_unlock_irqrestore(inno_audio->chip.audio_reg_lock, flags);

	return 0;
}

static int ctl_conn_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct audio_conn *ac = kcontrol->private_data, *pac;
	struct audio_device_t *inno_audio = ac->inno_audio;
	unsigned int change = 0;
	unsigned long flags;

	if(!inno_audio){
		innoaudio_err("ctl conn get error\n");
		return -EFAULT;
	}

	if (!ac->enable || !ac->disable) {
		innoaudio_info(&inno_audio->pfdev->dev, "connectro audio enable/disable null !!!");
		return  change;
	}

	innoaudio_info(&inno_audio->pfdev->dev, "\"%s\" typed, put val: %ld, old val: %d",
			kcontrol->id.name, ucontrol->value.integer.value[0], ac->is_choosen);

	fh2m_inno_spin_lock_irqsave(inno_audio->chip.audio_reg_lock, &flags);

	if(ac->is_choosen != ucontrol->value.integer.value[0]){
		change = 1;
	}

	ac->is_choosen = ucontrol->value.integer.value[0];

	if(ac->is_choosen){
		innoaudio_info(&inno_audio->pfdev->dev, "choose connector %d audio", ac->id);
		ac->enable(ac);
	}else{
		innoaudio_info(&inno_audio->pfdev->dev, "unchoose connector %d audio", ac->id);
		ac->disable(ac);
	}

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if (!pac)
			continue;
		if ((ac != pac) && (ac->is_choosen == 1)) {
			pac->is_choosen = 0;
		}
		innoaudio_hw_set_output(&inno_audio->chip, pac->id, pac->is_choosen);
	}

	fh2m_inno_spin_unlock_irqrestore(inno_audio->chip.audio_reg_lock, flags);

	return change;
}

static int had_ctl_eld_info(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = MAX_ELD_BYTES;
	return 0;
}

static int had_ctl_eld_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct audio_conn *ac = kcontrol->private_data;
	struct audio_device_t *inno_audio = ac->inno_audio;

	innoaudio_info(&inno_audio->pfdev->dev, "%s typed", kcontrol->id.name);

	mutex_lock(&inno_audio->mutex);
	memcpy(ucontrol->value.bytes.data, ac->eld, MAX_ELD_BYTES);
	mutex_unlock(&inno_audio->mutex);

	//print_hex_dump(KERN_ERR, " \t", 0, 16, 1, inno_audio->chip.eld, 128, false);
	return 0;
}

static const struct snd_kcontrol_new conn_controls[] = {
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
					SNDRV_CTL_ELEM_ACCESS_WRITE),
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "HDMI0 Switch",
		.info = ctl_conn_info,
		.get = ctl_conn_get,
		.put = ctl_conn_put,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
					SNDRV_CTL_ELEM_ACCESS_WRITE),
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "HDMI1 Switch",
		.info = ctl_conn_info,
		.get = ctl_conn_get,
		.put = ctl_conn_put,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
					SNDRV_CTL_ELEM_ACCESS_WRITE),
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "DP0 Switch",
		.info = ctl_conn_info,
		.get = ctl_conn_get,
		.put = ctl_conn_put,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
					SNDRV_CTL_ELEM_ACCESS_WRITE),
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "DP1 Switch",
		.info = ctl_conn_info,
		.get = ctl_conn_get,
		.put = ctl_conn_put,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
					SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "ELD",
		.info = had_ctl_eld_info,
		.get = had_ctl_eld_get,
	},
};

static struct inno_ctrl_t inno_ctrl[] = {
	[0] = {
		.name = "HDMI0",
		.kctrl = &conn_controls[0],
		.type = INNOAUDIO_CTRL_TYPE_SWITCH,
	},
	[1] = {
		.name = "HDMI1",
		.kctrl = &conn_controls[1],
		.type = INNOAUDIO_CTRL_TYPE_SWITCH,
	},
	[2] = {
		.name = "DP0",
		.kctrl = &conn_controls[2],
		.type = INNOAUDIO_CTRL_TYPE_SWITCH,
	},
	[3] = {
		.name = "DP1",
		.kctrl = &conn_controls[3],
		.type = INNOAUDIO_CTRL_TYPE_SWITCH,
	},
	[4] = {
		.name = "ELD0",
		.kctrl = &conn_controls[4],
		.type = INNOAUDIO_CTRL_TYPE_ELD,
	},
};

static void innoaudio_runtime_info(struct audio_device_t *inno_audio)
{
	struct snd_pcm_substream *audio_playback_sub;

	if (inno_audio->chip.audio_playback_sub) {
		audio_playback_sub = (struct snd_pcm_substream *)inno_audio->chip.audio_playback_sub;

		innoaudio_info(&inno_audio->pfdev->dev,
						"****************** runtime params start *******************\n");
		innoaudio_info(&inno_audio->pfdev->dev,"substream number:%d", audio_playback_sub->number);
		innoaudio_info(&inno_audio->pfdev->dev,"avail_max:%ld", (unsigned long)(audio_playback_sub->runtime->avail_max));
		innoaudio_info(&inno_audio->pfdev->dev,"hw_ptr_jiffies:%ld", (unsigned long)(audio_playback_sub->runtime->hw_ptr_jiffies));
		innoaudio_info(&inno_audio->pfdev->dev,"hw_ptr_buffer_jiffies:%ld", (unsigned long)(audio_playback_sub->runtime->hw_ptr_buffer_jiffies));
		innoaudio_info(&inno_audio->pfdev->dev,"delay:%ld", (signed long)(audio_playback_sub->runtime->delay));
		innoaudio_info(&inno_audio->pfdev->dev,"hw_ptr_wrap:%lld", audio_playback_sub->runtime->hw_ptr_wrap);
		innoaudio_info(&inno_audio->pfdev->dev,"format:%d", (int)(audio_playback_sub->runtime->format));
		innoaudio_info(&inno_audio->pfdev->dev,"subformat:%d", (int)(audio_playback_sub->runtime->subformat));
		innoaudio_info(&inno_audio->pfdev->dev,"rate:%d", audio_playback_sub->runtime->rate);
		innoaudio_info(&inno_audio->pfdev->dev,"channels:%d", audio_playback_sub->runtime->channels);
		innoaudio_info(&inno_audio->pfdev->dev,"period_size:%ld", (unsigned long)(audio_playback_sub->runtime->period_size));
		innoaudio_info(&inno_audio->pfdev->dev,"periods:%d", audio_playback_sub->runtime->periods);
		innoaudio_info(&inno_audio->pfdev->dev,"buffer_size:%ld", (unsigned long)(audio_playback_sub->runtime->buffer_size));
		innoaudio_info(&inno_audio->pfdev->dev,"min_align:%ld", (unsigned long)(audio_playback_sub->runtime->min_align));
		innoaudio_info(&inno_audio->pfdev->dev,"byte_align:%ld", (unsigned long)(audio_playback_sub->runtime->byte_align));
		innoaudio_info(&inno_audio->pfdev->dev,"frame_bits:%d", audio_playback_sub->runtime->frame_bits);
		innoaudio_info(&inno_audio->pfdev->dev,"sample_bits:%d", audio_playback_sub->runtime->sample_bits);
		innoaudio_info(&inno_audio->pfdev->dev,"info:%d", audio_playback_sub->runtime->info);
		innoaudio_info(&inno_audio->pfdev->dev,"rate_num:%d", audio_playback_sub->runtime->rate_num);
		innoaudio_info(&inno_audio->pfdev->dev,"rate_den:%d", audio_playback_sub->runtime->rate_den);
		innoaudio_info(&inno_audio->pfdev->dev,"no_period_wakeup:%d", audio_playback_sub->runtime->no_period_wakeup);
		innoaudio_info(&inno_audio->pfdev->dev,"tstamp_mode:%d", audio_playback_sub->runtime->tstamp_mode);
		innoaudio_info(&inno_audio->pfdev->dev,"period_step:%d", audio_playback_sub->runtime->period_step);
		innoaudio_info(&inno_audio->pfdev->dev,"start_threshold:%ld", audio_playback_sub->runtime->start_threshold);
		innoaudio_info(&inno_audio->pfdev->dev,"stop_threshold:%ld", (unsigned long)(audio_playback_sub->runtime->stop_threshold));
		innoaudio_info(&inno_audio->pfdev->dev,"silence_threshold:%ld", (unsigned long)(audio_playback_sub->runtime->silence_threshold));
		innoaudio_info(&inno_audio->pfdev->dev,"silence_size:%ld", (unsigned long)(audio_playback_sub->runtime->silence_size));
		innoaudio_info(&inno_audio->pfdev->dev,"boundary:%ld", (unsigned long)(audio_playback_sub->runtime->boundary));
		innoaudio_info(&inno_audio->pfdev->dev,"silence_start:%ld", (unsigned long)(audio_playback_sub->runtime->silence_start));
		innoaudio_info(&inno_audio->pfdev->dev,"silence_filled:%ld", (unsigned long)(audio_playback_sub->runtime->silence_filled));
		innoaudio_info(&inno_audio->pfdev->dev,"twake:%ld", (unsigned long)(audio_playback_sub->runtime->twake));
		innoaudio_info(&inno_audio->pfdev->dev,"dma_bytes:%ld", audio_playback_sub->runtime->dma_bytes);
		innoaudio_info(&inno_audio->pfdev->dev,"avail_min:%ld\n", audio_playback_sub->runtime->control->avail_min);
		innoaudio_info(&inno_audio->pfdev->dev,"appl_ptr:%ld\n",audio_playback_sub->runtime->control->appl_ptr);

		innoaudio_info(&inno_audio->pfdev->dev,
						"****************** runtime params end *******************\n");
	}
}

static void innoaudio_ptr_info(struct audio_device_t *inno_audio, bool flag, const char *func_name)
{
	u32 avail;
	unsigned long flags;
	struct snd_pcm_substream *audio_playback_sub;

	spin_lock_irqsave(inno_audio->chip.audio_reg_lock, flags);
	if (flag) {
		if (inno_audio->chip.audio_playback_sub) {
			audio_playback_sub = (struct snd_pcm_substream *)inno_audio->chip.audio_playback_sub;
			avail = snd_pcm_playback_avail(audio_playback_sub->runtime);

			innoaudio_info(&inno_audio->pfdev->dev,
							"[%s] hw_ptr_base:%#lx, appl_ptr:%#lx, hw_ptr:%#lx, avail_min:%ld, avail:%#x\n",
	                        func_name,
	                        audio_playback_sub->runtime->hw_ptr_base,
	                        audio_playback_sub->runtime->control->appl_ptr,
	                        audio_playback_sub->runtime->status->hw_ptr,
	                        audio_playback_sub->runtime->control->avail_min,
	                        avail);
		}
	}
	spin_unlock_irqrestore(inno_audio->chip.audio_reg_lock, flags);
}

static snd_pcm_uframes_t innoaudio_playback_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	int cur_offset = 0;
	snd_pcm_uframes_t frames;

	if(!inno_audio)
		return 0;

	cur_offset = innoaudio_get_offset(&inno_audio->chip);
	frames = bytes_to_frames(runtime, cur_offset);
	if (frames >= runtime->buffer_size)
		frames -= runtime->buffer_size;//以frame为单位

	innoaudio_ptr_info(inno_audio, false, "innoaudio_playback_pcm_pointer");
	return frames;
}

static int innoaudio_playback_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	int err = 0;

	if(!inno_audio)
		return 0;

	inno_audio->chip.audio_playback_sub = (struct snd_pcm_substream *)substream;

	innoaudio_substream_ctrl(&inno_audio->chip, cmd);

	innoaudio_runtime_info(inno_audio);
	return err;
}

static int innoaudio_playback_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	struct audio_chip_t *chip;
	struct snd_pcm_substream *audio_playback_sub;
	struct audio_conn *pac = NULL;

	if(!inno_audio)
		return 0;

	audio_playback_sub = (struct snd_pcm_substream *)inno_audio->chip.audio_playback_sub;

	innoaudio_info(&inno_audio->pfdev->dev, "%s():\n", __func__);

	inno_audio->chip.buf_start = substream->dma_buffer.addr;
	inno_audio->chip.buf_end = substream->dma_buffer.addr + substream->dma_buffer.bytes;
	inno_audio->chip.buf_period = snd_pcm_lib_period_bytes(substream);
	inno_audio->chip.buf_size = snd_pcm_lib_buffer_bytes(substream);
	audio_playback_sub->runtime->boundary = inno_audio->chip.buf_size;
	audio_playback_sub->runtime->delay = audio_playback_sub->runtime->period_size;

	chip = &inno_audio->chip;

	fh2m_hal_dev_enable_irq(chip->parent, chip->hal_module);
	innoaudio_hw_init(&inno_audio->chip);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if (!pac)
			continue;
		if (!pac->enable || !pac->disable) {
			innoaudio_info(&inno_audio->pfdev->dev, "connectro audio enable/disable null !!!");
			continue;
		}
		if(connector_is_choosen(pac)){
			innoaudio_info(&inno_audio->pfdev->dev, "enable connector %d audio", pac->id);
			pac->enable(pac);
		}else {
			innoaudio_info(&inno_audio->pfdev->dev, "disable connector %d audio", pac->id);
			pac->disable(pac);
		}
		innoaudio_hw_set_output(&inno_audio->chip, pac->id, pac->is_choosen);
	}

	audio_notifier_data_get(inno_audio, substream->runtime);

	return 0;
}

static int innoaudio_playback_hw_free(struct snd_pcm_substream *substream)
{
	//snd_pcm_lib_free_pages(substream);
	snd_pcm_set_runtime_buffer(substream, NULL);
	substream->dma_buffer.area = NULL;
	return 0;
}

static int innoaudio_playback_hw_params(struct snd_pcm_substream *substream,
										struct snd_pcm_hw_params *hw_params)
{
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);

	if(!inno_audio)
		return 0;

	innoaudio_info(&inno_audio->pfdev->dev, " init hw params\n");

	substream->dma_buffer.addr = inno_audio->chip.paddr;
	substream->dma_buffer.bytes = inno_audio->chip.audio_buf_size;
	substream->dma_buffer.dev.type = SNDRV_DMA_TYPE_UNKNOWN;
	substream->dma_buffer.dev.dev = inno_audio->card->dev;

	substream->dma_buffer.area = inno_audio->chip.vaddr;

	innoaudio_info(&inno_audio->pfdev->dev, "dma_buffer.addr:0x%llx,dma_buffer.bytes:0x%lx",
			  substream->dma_buffer.addr, substream->dma_buffer.bytes);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int innoaudio_playback_open(struct snd_pcm_substream *substream)
{
	int retval;
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if(!inno_audio)
		return 0;

	innoaudio_info(&inno_audio->pfdev->dev, "Playback open\n");
	inno_audio->chip.audio_playback_sub = (struct snd_pcm_substream *)substream;

	innoaudio_init_hw_param(&inno_audio->chip);

	retval = snd_pcm_hw_constraint_integer(runtime,
			 SNDRV_PCM_HW_PARAM_PERIODS);
	if (retval < 0)
		goto error;

	/* Make sure, that the period size is always aligned
	 * 64byte boundary
	 */
	retval = snd_pcm_hw_constraint_step(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 64);
	if (retval < 0)
		goto error;

	retval = snd_pcm_hw_constraint_msbits(runtime, 0, 32, 24);
	if (retval < 0)
		goto error;

error:
	return 0;
}

static int innoaudio_playback_close(struct snd_pcm_substream *substream)
{
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	struct audio_chip_t *chip;
	struct audio_conn *pac = NULL;

	if(!inno_audio)
		return 0;

	innoaudio_info(&inno_audio->pfdev->dev, "%s()\n", __func__);

	chip = &inno_audio->chip;
	fh2m_hal_dev_disable_irq(chip->parent, chip->hal_module);
	innoaudio_hw_close(&inno_audio->chip);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if (!pac)
			continue;
		if (!pac->enable || !pac->disable) {
			innoaudio_info(&inno_audio->pfdev->dev, "connectro audio enable/disable null !!!");
			return  0;
		}
		if(connector_is_choosen(pac)){
			innoaudio_info(&inno_audio->pfdev->dev, "disable connector %d audio", pac->id);
			pac->disable(pac);
		}
		innoaudio_hw_set_output(&inno_audio->chip, pac->id, 0);
	}
	return 0;
}

static int innoaudio_playback_dev_free(struct audio_device_t *inno_audio)
{
	struct role_target role;

	innoaudio_info(inno_audio->pfdev->dev.parent, "innoaudio_playback_dev_free");

	role.vram_role = HAL_VRAM_ROLE_AUDIO;
	role.id = 0;
	role.sub_id = 0;

	if (inno_audio->chip.vaddr) {
		iounmap(inno_audio->chip.vaddr);
		fh2m_hal_vram_free(inno_audio->pfdev->dev.parent, &role, inno_audio->chip.paddr);
		inno_audio->chip.vaddr = NULL;
	}

	innoaudio_chip_fini(&inno_audio->chip);
	return 0;
}

static void innoaudio_playback_handle_irq(void *data)
{
	struct platform_device *pfdev = (struct platform_device *)data;
	struct audio_device_t *inno_audio = NULL;

	inno_audio = fh2m_inno_platform_get_drvdata(pfdev);
	if (inno_audio == NULL) {
		innoaudio_err("innoaudio handle is NULL\n");
		return;
	}

	if (inno_audio->chip.irq_handle)
		inno_audio->chip.irq_handle(&inno_audio->chip);
	innoaudio_ptr_info(inno_audio, false, "innoaudio_playback_handle_irq");
}

/*
 * ALSA PCM mmap callback
 */
static int innoaudio_playback_pcm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	struct audio_device_t *inno_audio = snd_pcm_substream_chip(substream);
	dma_addr_t remap_addr;

	if(!inno_audio)
		return 0;

	remap_addr = fh2m_dev_paddr_to_cpu_paddr(inno_audio->pfdev->dev.parent,
								substream->dma_buffer.addr);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(6, 3, 0))
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
#else
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
#endif
	fh2m_inno_pgprot_writecombine(&(vma->vm_page_prot), &(vma->vm_page_prot));
	return remap_pfn_range(vma, vma->vm_start,
			remap_addr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
}


static __maybe_unused void *get_runtime_dma_ptr(struct snd_pcm_runtime *runtime,
			   int channel, unsigned long hwoff)
{
	return runtime->dma_area + hwoff +
		channel * (runtime->dma_bytes / runtime->channels);
}

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
int innoaudio_copy_user(struct snd_pcm_substream *substream, int channel,
                 unsigned long pos, void __user *buf, unsigned long bytes)
{
	void *dma_ptr = get_runtime_dma_ptr(substream->runtime, channel, pos);

#ifdef CONFIG_SW64
	void *tmp = kmalloc(bytes, fh2m_hal_get_inno_gfp_kernel());
	if(!tmp)
		return -EFAULT;
	if (fh2m_inno_copy_from_user(tmp, (void __user *)buf, bytes)){
		kfree(tmp);
		return -EFAULT;
	}
	memcpy_toio(dma_ptr, tmp, bytes);
	kfree(tmp);
#else
	if (fh2m_inno_copy_from_user(dma_ptr, (void __user *)buf, bytes))
		return -EFAULT;
#endif

	return 0;
}
#endif

static const struct snd_pcm_ops g_innoaudio_playback_ops = {
	.open = innoaudio_playback_open,
	.close = innoaudio_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = innoaudio_playback_hw_params,
	.hw_free = innoaudio_playback_hw_free,
	.prepare = innoaudio_playback_pcm_prepare,
	.trigger = innoaudio_playback_pcm_trigger,
	.pointer = innoaudio_playback_pcm_pointer,
	.mmap = innoaudio_playback_pcm_mmap,
#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	.copy_user = innoaudio_copy_user,
#endif
};

static ssize_t debug_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	u32 flags = simple_strtoul(buf, NULL, 0);

	if (flags){
		s_audio_debug = true;
	} else {
		s_audio_debug = false;
	}

	return len;
}

static ssize_t off_fix_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	inno_audio->chip.off_fix = simple_strtoul(buf, NULL, 0);
	innoaudio_info(dev, "off_fix: %d", inno_audio->chip.off_fix);
	return len;
}

static ssize_t buf_max_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	inno_audio->chip.buf_max = simple_strtoul(buf, NULL, 0);
	innoaudio_info(dev, "buf_max: %d", inno_audio->chip.buf_max);

	return len;
}

static ssize_t pmax_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	inno_audio->chip.pmax = simple_strtoul(buf, NULL, 0);
	innoaudio_info(dev, "pmax: %d", inno_audio->chip.pmax);

	return len;
}

static ssize_t pmin_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	inno_audio->chip.pmin = simple_strtoul(buf, NULL, 0);
	innoaudio_info(dev, "pmin: %d", inno_audio->chip.pmin);

	return len;
}

static ssize_t jack_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);
	struct audio_conn *pac = NULL;
	int id = 0, st = 0;

	sscanf(buf, "%d %d", &id, &st);
	innoaudio_info(dev, "jack id: %d, st: %d", id, st);


	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(connector_is_choosen(pac) && (pac->id == id)){
			pac->conn_st = st;
			pac->has_audio = st;
			pac->report_jack(pac, st, st);
			pac->report_jack(pac, st, st);
		}
	}

	return len;
}

static ssize_t jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);
	struct audio_conn *pac = NULL;
	int count = 0;

	list_for_each_entry(pac, &inno_audio->conn_list, list){
			count +=  sprintf(buf + count, "jack id: %d, st: %s, is_choosen: %d\n",
						pac->id,  pac->jack_st ? "in":"out", pac->is_choosen);
	}

	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	if(inno_audio->chip.reg_show)
		return inno_audio->chip.reg_show(&inno_audio->chip, buf);

	return count;
}

static ssize_t card_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);

	count +=  sprintf(buf + count, "shortname: %s\n", inno_audio->card->shortname);
	count +=  sprintf(buf + count, "longname : %s\n", inno_audio->card->longname);

	return count;
}

static ssize_t path_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t len)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);
	int id = 0, st = 0;

	sscanf(buf, "%d %d", &id, &st);
	innoaudio_info(dev, "conn id: %d, st: %d", id, st);

	innoaudio_hw_set_output(&inno_audio->chip, id, st);


	return len;
}

static ssize_t runtime_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct audio_device_t *inno_audio = dev_get_drvdata(dev);
	struct snd_pcm_substream *audio_playback_sub;
	int count = 0;

	if (inno_audio->chip.audio_playback_sub) {
		audio_playback_sub = (struct snd_pcm_substream *)inno_audio->chip.audio_playback_sub;

		count +=  sprintf(buf + count, "******** Runtime params start ********\n");
		count +=  sprintf(buf + count, "substream number:%d\n", audio_playback_sub->number);
		count +=  sprintf(buf + count, "avail_max:%ld\n", (unsigned long)(audio_playback_sub->runtime->avail_max));
		count +=  sprintf(buf + count, "hw_ptr_jiffies:%ld\n", (unsigned long)(audio_playback_sub->runtime->hw_ptr_jiffies));
		count +=  sprintf(buf + count, "hw_ptr_buffer_jiffies:%ld\n", (unsigned long)(audio_playback_sub->runtime->hw_ptr_buffer_jiffies));
		count +=  sprintf(buf + count, "delay:%ld\n", (signed long)(audio_playback_sub->runtime->delay));
		count +=  sprintf(buf + count, "hw_ptr_wrap:%lld\n", audio_playback_sub->runtime->hw_ptr_wrap);
		count +=  sprintf(buf + count, "format:%d\n", (int)(audio_playback_sub->runtime->format));
		count +=  sprintf(buf + count, "subformat:%d\n", (int)(audio_playback_sub->runtime->subformat));
		count +=  sprintf(buf + count, "rate:%d\n", audio_playback_sub->runtime->rate);
		count +=  sprintf(buf + count, "channels:%d\n", audio_playback_sub->runtime->channels);
		count +=  sprintf(buf + count, "period_size:%ld\n", (unsigned long)(audio_playback_sub->runtime->period_size));
		count +=  sprintf(buf + count, "periods:%d\n", audio_playback_sub->runtime->periods);
		count +=  sprintf(buf + count, "buffer_size:%ld\n", (unsigned long)(audio_playback_sub->runtime->buffer_size));
		count +=  sprintf(buf + count, "min_align:%ld\n", (unsigned long)(audio_playback_sub->runtime->min_align));
		count +=  sprintf(buf + count, "byte_align:%ld\n", (unsigned long)(audio_playback_sub->runtime->byte_align));
		count +=  sprintf(buf + count, "frame_bits:%d\n", audio_playback_sub->runtime->frame_bits);
		count +=  sprintf(buf + count, "sample_bits:%d\n", audio_playback_sub->runtime->sample_bits);
		count +=  sprintf(buf + count, "info:%d\n", audio_playback_sub->runtime->info);
		count +=  sprintf(buf + count, "rate_num:%d\n", audio_playback_sub->runtime->rate_num);
		count +=  sprintf(buf + count, "rate_den:%d\n", audio_playback_sub->runtime->rate_den);
		count +=  sprintf(buf + count, "no_period_wakeup:%d\n", audio_playback_sub->runtime->no_period_wakeup);
		count +=  sprintf(buf + count, "tstamp_mode:%d\n", audio_playback_sub->runtime->tstamp_mode);
		count +=  sprintf(buf + count, "period_step:%d\n", audio_playback_sub->runtime->period_step);
		count +=  sprintf(buf + count, "start_threshold:%ld\n", audio_playback_sub->runtime->start_threshold);
		count +=  sprintf(buf + count, "stop_threshold:%ld\n", (unsigned long)(audio_playback_sub->runtime->stop_threshold));
		count +=  sprintf(buf + count, "silence_threshold:%ld\n", (unsigned long)(audio_playback_sub->runtime->silence_threshold));
		count +=  sprintf(buf + count, "silence_size:%ld\n", (unsigned long)(audio_playback_sub->runtime->silence_size));
		count +=  sprintf(buf + count, "boundary:%ld\n", (unsigned long)(audio_playback_sub->runtime->boundary));
		count +=  sprintf(buf + count, "silence_start:%ld\n", (unsigned long)(audio_playback_sub->runtime->silence_start));
		count +=  sprintf(buf + count, "silence_filled:%ld\n", (unsigned long)(audio_playback_sub->runtime->silence_filled));
		count +=  sprintf(buf + count, "twake:%ld\n", (unsigned long)(audio_playback_sub->runtime->twake));
		count +=  sprintf(buf + count, "dma_bytes:%ld\n", audio_playback_sub->runtime->dma_bytes);
		count +=  sprintf(buf + count, "avail_min:%ld\n", audio_playback_sub->runtime->control->avail_min);
		count +=  sprintf(buf + count, "appl_ptr:%ld\n",audio_playback_sub->runtime->control->appl_ptr);
		count +=  sprintf(buf + count, "******** Runtime params end ********\n");
	}else {
		count +=  sprintf(buf + count, "Innosilicon sound card stopped !!!\n");
	}

	return count;
}

static DEVICE_ATTR_RO(reg);
static DEVICE_ATTR_RO(runtime);
static DEVICE_ATTR_RO(card_info);
static DEVICE_ATTR_WO(debug);
static DEVICE_ATTR_WO(off_fix);
static DEVICE_ATTR_WO(buf_max);
static DEVICE_ATTR_WO(pmax);
static DEVICE_ATTR_WO(pmin);
static DEVICE_ATTR_WO(path);
static DEVICE_ATTR(jack,S_IRUGO|S_IWUSR,jack_show, jack_store);


static struct attribute *innoaudio_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_runtime.attr,
	&dev_attr_card_info.attr,
	&dev_attr_debug.attr,
	&dev_attr_off_fix.attr,
	&dev_attr_buf_max.attr,
	&dev_attr_pmax.attr,
	&dev_attr_pmin.attr,
	&dev_attr_jack.attr,
	&dev_attr_path.attr,
	NULL
};


static const struct attribute_group innoaudio_attr_group = {
	.attrs = innoaudio_attributes,
};

static int innoaudio_pm_notifier(struct notifier_block *nb,
							unsigned long val, void *ign)
{
	struct audio_device_t *inno_audio;

	inno_audio = container_of(nb, struct audio_device_t, pm_nb);

	innoaudio_info(&inno_audio->pfdev->dev, "Get pm notifier\n");

	inno_audio->chip.pm_st = val;
	switch (val) {
	case PM_HIBERNATION_PREPARE:
		if (inno_audio->chip.audio_suspend){
			inno_audio->chip.audio_suspend(&inno_audio->chip);
			return NOTIFY_OK;
		}

		innoaudio_hw_stop(&inno_audio->chip);

		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_HIBERNATION_PREPARE\n");
		break;
	case PM_POST_HIBERNATION:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_POST_HIBERNATION\n");
		break;
	case PM_SUSPEND_PREPARE:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_SUSPEND_PREPARE\n");
		break;
	case PM_POST_SUSPEND:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_POST_SUSPEND\n");
		break;
	case PM_RESTORE_PREPARE:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_RESTORE_PREPARE\n");
		break;
	case PM_POST_RESTORE:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: PM_POST_RESTORE\n");
		break;
	default:
		innoaudio_info(&inno_audio->pfdev->dev, "Notifier: ERROR\n");
		break;
	}
	return NOTIFY_OK;
}

static int create_ctrl(struct audio_conn *ac)
{
	struct audio_device_t *inno_audio = ac->inno_audio;
	struct snd_kcontrol *kctl;
	char nm[80], jk_nm[20];
	int index = 0, err;

	if(!inno_audio || !inno_audio->card || !ac->sw_ctrl || !ac->sw_ctrl->kctrl){
		innoaudio_err("Create ctrl error\n");
		return -EFAULT;
	}

	innoaudio_info(&ac->inno_audio->pfdev->dev, "Create Ctrl %s", ac->sw_ctrl->name);

	snprintf(jk_nm, sizeof(jk_nm), "%s", ac->name);
	snprintf(nm, sizeof(nm), "%s-%s", inno_audio->card->longname, ac->name);
	snprintf(inno_audio->card->longname, sizeof(inno_audio->card->longname), "%s", nm);

	if(index >= sizeof(conn_controls)/sizeof(conn_controls[0])){
		innoaudio_err("Create ctrl error, index:%d\n", index);
		return -EFAULT;
	}

	kctl = snd_ctl_new1(ac->sw_ctrl->kctrl, ac);
	kctl->id.device = inno_audio->pcm->device;
	snd_ctl_add(inno_audio->card, kctl);

	err = snd_jack_new(inno_audio->card, jk_nm, SND_JACK_AVOUT, &ac->jack,
						true, false);
	if(err){
		innoaudio_err("Alloc jack failed!\n");
		return -EFAULT;
	}

	return 0;
}

static int create_card(struct platform_device *pfdev)
{
	struct audio_device_t *inno_audio = fh2m_inno_platform_get_drvdata(pfdev);
	struct snd_card *card;
	struct snd_pcm *pcm;
	int err;

	err = snd_card_new(&pfdev->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1, THIS_MODULE, 0, &card);
	if (err < 0) {
		innoaudio_err("snd_card_new failed!\n");
		return -err;
	}

	inno_audio->card = card;
	card->private_data = inno_audio;
	err = snd_pcm_new(inno_audio->card, "inno_audio_pcm", 0, 1, 0, &pcm);
	if (err < 0) {
		innoaudio_err("snd_pcm_new failed!\n");
		goto err_snd_dev_new;
	}
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &g_innoaudio_playback_ops);

	pcm->private_data = inno_audio;
	pcm->info_flags = SNDRV_PCM_INFO_HALF_DUPLEX;
	strcpy(pcm->name, "inno_audio_pcm");
	inno_audio->pcm = pcm;

	strcpy(card->driver, "InnosiliconCard");
	strcpy(card->shortname, "InnosiliconCard");

	sprintf(card->longname, "%s%d", card->shortname, inno_audio->card_id);

	err = snd_card_register(card);
	if (err < 0) {
		innoaudio_err("register sound card failed\n");
		goto err_snd_dev_new;
	}

	return 0;
err_snd_dev_new:
	snd_card_free(card);

	return err;
}

static void innoaudio_update_eld(struct audio_conn *ac, char *buf, int size)
{
	if(NULL == buf) {
		innoaudio_err("buf is NULL!!\n");
		return;
	}
	if (size > MAX_ELD_BYTES) {
		innoaudio_err("ELD max size: 128\n");
		return;
	}

	if (ac->eld && memcmp(ac->eld, buf, size) != 0){
		memcpy(ac->eld, buf, size);
	}
}

static void innoaudio_report_jack(struct audio_conn *ac, int dpms_st, int conn_st)
{
	if(!ac || !ac->jack ){
		innoaudio_err("Report jack error!\n");
		return ;
	}

	ac->dpms = dpms_st;
	ac->conn_st = conn_st;
	ac->jack_st = (ac->has_audio && conn_st && (dpms_st == CONNECTOR_DPMS_ST_ON)) ? \
							CONNECTOR_JACK_ON : CONNECTOR_JACK_OFF;
	innoaudio_info(ac->dev, "Get %s jack report: 0x%x, conn_st: %d, dpms_st: %d",
							ac->name, ac->jack_st, conn_st, dpms_st);

	return ;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
void time_callback(unsigned long arg)
{
	struct audio_device_t *inno_audio = (struct audio_device_t *)arg;
	struct audio_conn *pac;
	int changed = 0;
	static int jack_st[INNOAUDIO_CONNECTOR_NUM];


	mod_timer(&inno_audio->chk_tm, jiffies + 2*HZ);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(!pac || (pac->id >= INNOAUDIO_CONNECTOR_NUM)){
			innoaudio_err("jack report error");
			break;
		}

		if((pac->inno_audio->chip.pm_st == PM_HIBERNATION_PREPARE ||
			pac->inno_audio->chip.pm_st == PM_SUSPEND_PREPARE) && (pac->conn_st)){
			innoaudio_info(pac->dev, "s3/s4 do not report jack in");
			return ;
		}

		if (jack_st[pac->id] != pac->jack_st) {
			jack_st[pac->id] = pac->jack_st;
			changed = 1;
		}

		innoaudio_info(pac->dev, "%s id: %d, jack_st new: 0x%x, old: 0x%x",
								pac->name, pac->id, pac->jack_st, jack_st[pac->id]);

		if (changed) {
			snd_jack_report(pac->jack,pac->jack_st);
			innoaudio_info(pac->dev, "%s report--> conn id: %d, changed: %d, jack state: %s\n",
										pac->name, pac->id, changed, pac->jack_st ? "plug in":"plug out");
			changed = 0;
		}
	}

}
#else
static void time_callback(struct timer_list *t)
{
	struct audio_device_t *inno_audio = from_timer(inno_audio, t, chk_tm);
	struct audio_conn *pac;
	int changed = 0;
	static int jack_st[INNOAUDIO_CONNECTOR_NUM];

	if(!inno_audio)
		return;

	mod_timer(&inno_audio->chk_tm, jiffies + 2*HZ);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(!pac || (pac->id >= INNOAUDIO_CONNECTOR_NUM)){
			innoaudio_err("jack report error");
			break;
		}

		if((pac->inno_audio->chip.pm_st == PM_HIBERNATION_PREPARE ||
			pac->inno_audio->chip.pm_st == PM_SUSPEND_PREPARE) && (pac->conn_st)){
			innoaudio_info(pac->dev, "s3/s4 do not report jack in");
			return ;
		}

		if (jack_st[pac->id] != pac->jack_st) {
			jack_st[pac->id] = pac->jack_st;
			changed = 1;
		}

		innoaudio_info(pac->dev, "%s id: %d, jack_st new: 0x%x, old: 0x%x",
								pac->name, pac->id, pac->jack_st, jack_st[pac->id]);

		if (changed) {
			snd_jack_report(pac->jack,pac->jack_st);
			innoaudio_info(pac->dev, "%s report--> conn id: %d, changed: %d, jack state: %s\n",
										pac->name, pac->id, changed, pac->jack_st ? "plug in":"plug out");
			changed = 0;
		}
	}
}
#endif

int fh2m_innoaudio_register_connector(struct audio_conn *ac)
{
	struct audio_device_t *inno_audio;
	struct audio_chip_t *chip;
	int i, count;

	if(!ac->dev || ac->id > 31){
		innoaudio_err("Register connector error!\n");
		return 	-EINVAL;
	}

	for(i = 0; i < SNDRV_CARDS; i++){
		if (s_card_data[i]) {
			inno_audio = s_card_data[i];
			break;
		} else {
			return 	-EINVAL;
		}
	}

	chip = &inno_audio->chip;

	ac->id = chip->init_connector(chip, ac->type, ac->id, ac->name, sizeof(ac->name));
	if((ac->id < 0) || (ac->id >= INNOAUDIO_CONNECTOR_NUM)){
		innoaudio_err("set connector id error!");
		return -1;
	}

	innoaudio_info(ac->dev, "Get connector id: %d, name: %s", ac->id, ac->name);

	count = sizeof(inno_ctrl)/sizeof(inno_ctrl[0]);
	for (i = 0; i < count; i++) {
		if (!strcmp(inno_ctrl[i].name, ac->name)) {
			ac->sw_ctrl = &inno_ctrl[i];
			innoaudio_info(ac->dev, "Connector %s bind Ctrl %s", ac->name, ac->sw_ctrl->name);
			break;
		}
	}
	if (i >= count) {
		innoaudio_err("binding control error!");
		return -EFAULT;
	}

	ac->eld = kzalloc(MAX_ELD_BYTES, fh2m_hal_get_inno_gfp_kernel());
	if (ac->eld == NULL) {
		innoaudio_err("Alloc connector eld failed!\n");
		return -ENOMEM;
	}

	ac->audio_st = 0;
	ac->dpms = CONNECTOR_DPMS_ST_OFF;
	ac->is_choosen = 0;
	ac->data.sample_bits = 16;
	ac->inno_audio = inno_audio;
	ac->report_jack = innoaudio_report_jack;
	ac->update_eld = innoaudio_update_eld;
	list_add(&ac->list, &inno_audio->conn_list);

	create_ctrl(ac);

	inno_audio->ac_num++;
	return 0;
}
INNO_EXT_SYM(fh2m_innoaudio_register_connector);

int fh2m_innoaudio_unregister_connector(struct audio_conn *ac)
{
	if(!ac->dev){
		innoaudio_err("unregister connector error!\n");
		return 	-EINVAL;
	}

	//todo destroy ctrl

	list_del(&ac->list);

	s_card_data[ac->inno_audio->card_id] = NULL;
	ac->report_jack = NULL;
	if(ac->eld){
		kfree(ac->eld);
	}
	return 0;
}
INNO_EXT_SYM(fh2m_innoaudio_unregister_connector);

static int innoaudio_bind(struct device *dev)
{
	struct platform_device *pfdev = to_platform_device(dev);
	plat_data_t *pdata =  dev_get_platdata(&pfdev->dev);
	struct dev_rsrc *pdev_rsrc;
	struct audio_device_t *inno_audio;
	struct role_target role;
	u64 cpu_addr, dev_addr;
	int err = 0;
	static int cnt;

	if (pdata->dev_idx >= SNDRV_CARDS) {
		return -ENODEV;
	}

	role.vram_role = HAL_VRAM_ROLE_AUDIO;
	role.id = 0;
	role.sub_id = 0;

	inno_audio = kzalloc(sizeof(struct audio_device_t), fh2m_hal_get_inno_gfp_kernel());
	if (inno_audio == NULL) {
		innoaudio_err("inno audio kzalloc failed!\n");
		return -ENOMEM;
	}
	inno_audio->card_id = cnt;
	s_card_data[inno_audio->card_id] = inno_audio;
	cnt++;

	INIT_LIST_HEAD(&inno_audio->conn_list);

	inno_audio->pfdev = pfdev;
	pdev_rsrc = pdata->pdev_rsrc;
	inno_audio->pdev_rsrc = pdev_rsrc;

	inno_audio->chip.dev = get_device(&pfdev->dev);
	inno_audio->chip.parent = fh2m_inno_dev_get_parent(inno_audio->chip.dev);
	inno_audio->chip.id = 0;
	err = innoaudio_chip_init(&inno_audio->chip);
	if (err < 0) {
		innoaudio_err("audio chip init failed!\n");
		err = -EINVAL;
		goto err_chip_init;
	}

	innoaudio_info(&pfdev->dev, "%s buffer size is %#.8x\n",
								inno_audio->chip.name, inno_audio->chip.audio_buf_size);
	dev_addr = fh2m_hal_vram_alloc(pfdev->dev.parent, &role, true, inno_audio->chip.audio_buf_size, 0);
	if(!dev_addr){
		innoaudio_err("can not alloc vram for audio!\n");
		err = -EINVAL;
		goto err_chip_init;
	}
	cpu_addr = fh2m_dev_paddr_to_cpu_paddr(pfdev->dev.parent, dev_addr);

	inno_audio->chip.paddr = dev_addr;
	inno_audio->chip.baddr = cpu_addr;
	inno_audio->chip.vaddr = (void __iomem *)fh2m_inno_ioremap_wc_portable(cpu_addr, inno_audio->chip.audio_buf_size);
	innoaudio_info(&pfdev->dev, "dev_addr=0x%llx cpu_addr=0x%llx inno_audio->vaddr=0x%llx\n",
								dev_addr, cpu_addr, inno_audio->chip.vaddr);

	mutex_init(&inno_audio->mutex);
	fh2m_inno_platform_set_drvdata(pfdev, inno_audio);
	err = fh2m_hal_set_irq_handler(pfdev->dev.parent, inno_audio->chip.hal_module,
								innoaudio_playback_handle_irq, pfdev);
	if (err) {
		innoaudio_err("failed to set interrupt handler (err=%d)\n", err);
		err = -EINVAL;
		goto err_irq_init;
	}

	innoaudio_info(&pfdev->dev, "inno audio probe success!\n");

	inno_audio->pm_nb.notifier_call = innoaudio_pm_notifier;
	inno_audio->pm_nb.priority = 0;
	register_pm_notifier(&inno_audio->pm_nb);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
	init_timer(&inno_audio->chk_tm);
	inno_audio->chk_tm.expires = jiffies + 2*HZ;
	inno_audio->chk_tm.function = time_callback;
	inno_audio->chk_tm.data = (unsigned long)inno_audio;
	add_timer(&inno_audio->chk_tm);
#else
	timer_setup(&inno_audio->chk_tm, time_callback, 0);
	mod_timer(&inno_audio->chk_tm, jiffies + HZ);
#endif

	err = sysfs_create_group(&pfdev->dev.kobj, &innoaudio_attr_group);
	if (err) {
		innoaudio_err("Create debugfs error\n", err);
		err = -EINVAL;
		goto err_sysfs;
	}

	err = create_card(inno_audio->pfdev);
	if(err){
		innoaudio_info(&inno_audio->pfdev->dev, "Innosilicon sound card created failed!\n");
		err = -EINVAL;
		goto err_sysfs;
	}
	return 0;

err_sysfs:
	fh2m_hal_set_irq_handler(pfdev->dev.parent, inno_audio->chip.hal_module, NULL, NULL);
err_irq_init:
	innoaudio_chip_fini(&inno_audio->chip);
err_chip_init:
	kfree(inno_audio);

	return err;
}

static int innoaudio_unbind(struct device *dev)
{
	struct audio_device_t  *inno_audio = dev_get_drvdata(dev);
	if (inno_audio == NULL) {
		innoaudio_err("innoaudio handle is NULL\n");
		return 0;
	}
	innoaudio_info(&inno_audio->pfdev->dev, "unbind innoaudio!\n");

	if(inno_audio->chip.audio_playback_sub)
		snd_pcm_stop(inno_audio->chip.audio_playback_sub, SNDRV_PCM_STATE_DISCONNECTED);

	del_timer_sync(&inno_audio->chk_tm);
	return 0;
}

static int innoaudio_playback_suspend(struct device *dev)
{
	struct audio_device_t *inno_audio = NULL;
	struct audio_conn *pac = NULL;

	inno_audio = dev_get_drvdata(dev);
	if (inno_audio == NULL) {
		innoaudio_err("innoaudio handle is NULL\n");
		return 0;
	}

	if (!inno_audio->chip.audio_playback_sub) {
		innoaudio_info(&inno_audio->pfdev->dev, "pcm closed !!!");
		return 0;
	}

	innoaudio_suspend(&inno_audio->chip);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(!pac)
			continue;

		if (!pac->enable || !pac->disable) {
			innoaudio_info(&inno_audio->pfdev->dev, "connectro audio enable/disable null !!!");
			continue;
		}

		if(connector_is_choosen(pac)){
			innoaudio_info(&inno_audio->pfdev->dev, "enable connector %d audio", pac->id);
			pac->disable(pac);
		}
		innoaudio_hw_set_output(&inno_audio->chip, pac->id, 0);
	}
	return 0;
}

static int innoaudio_playback_resume(struct device *dev)
{
	struct audio_device_t *inno_audio = NULL;
	struct audio_conn *pac = NULL;

	inno_audio = dev_get_drvdata(dev);
	if (inno_audio == NULL) {
		innoaudio_err("innoaudio handle is NULL\n");
		return 0;
	}

	mod_timer(&inno_audio->chk_tm, jiffies + 2*HZ);

	if (!inno_audio->chip.audio_playback_sub) {
		innoaudio_info(&inno_audio->pfdev->dev, "pcm closed !!!");
		return 0;
	}

	innoaudio_resume(&inno_audio->chip);

	list_for_each_entry(pac, &inno_audio->conn_list, list){
		if(!pac)
			continue;

		if (!pac->enable || !pac->disable) {
			innoaudio_info(&inno_audio->pfdev->dev, "connectro audio enable/disable null !!!");
			continue;
		}

		if(connector_is_choosen(pac)){
			innoaudio_info(&inno_audio->pfdev->dev, "enable connector %d audio", pac->id);
			pac->enable(pac);
		}
		innoaudio_hw_set_output(&inno_audio->chip, pac->id, connector_is_choosen(pac));
	}

	return 0;
}

static int innoaudio_playback_s4_suspend(struct device *dev)
{
	innoaudio_playback_suspend(dev);

	return 0;
}

static void innoaudio_playback_shutdown(struct platform_device *dev)
{
	innoaudio_playback_suspend(&dev->dev);
}

static int innoaudio_playback_s4_resume(struct device *dev)
{
	innoaudio_playback_resume(dev);

	return 0;
}

static const struct dev_pm_ops innoaudio_playback_pm_ops = {
	.suspend = innoaudio_playback_suspend,
	.resume = innoaudio_playback_resume,
	.freeze = innoaudio_playback_suspend,
	.thaw = innoaudio_playback_resume,
	.poweroff = innoaudio_playback_s4_suspend,
	.restore = innoaudio_playback_s4_resume,
};

static int innoaudio_component_bind(struct device *dev, struct device *master, void *data)
{
	dev_info(dev, "bind audio device");

	return innoaudio_bind(dev);
}

static void innoaudio_component_unbind(struct device *dev, struct device *master, void *data)
{
	dev_info(dev, "unbind audio device");

	innoaudio_unbind(dev);
}

static const struct component_ops innoaudio_component_ops = {
	.bind   = innoaudio_component_bind,
	.unbind = innoaudio_component_unbind,
};

static int innoaudio_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &innoaudio_component_ops);
}

static int innoaudio_remove(struct platform_device *pdev)
{
	struct audio_device_t  *inno_audio = dev_get_drvdata(&pdev->dev);
	if (inno_audio == NULL) {
		innoaudio_err("innoaudio handle is NULL\n");
		return 0;
	}
	innoaudio_info(&inno_audio->pfdev->dev, "remove innoaudio!\n");

	innoaudio_playback_dev_free(inno_audio);
	unregister_pm_notifier(&inno_audio->pm_nb);
	snd_card_free(inno_audio->card);
	sysfs_remove_group(&inno_audio->pfdev->dev.kobj, &innoaudio_attr_group);
	kfree(inno_audio);

	component_del(&pdev->dev, &innoaudio_component_ops);
	return 0;
}


static struct platform_device_id s_inno_audio_platform_device_id_table[] = {
	{.name = INNO_AUDIO_DEVICE_NAME,.driver_data = 0},
	{},
};

MODULE_DEVICE_TABLE(platform, s_inno_audio_platform_device_id_table);

struct platform_driver g_innoaudio_playback_driver = {
	.probe = innoaudio_probe,
	.remove = innoaudio_remove,
	.shutdown = innoaudio_playback_shutdown,
	.driver = {
			   .name = INNO_AUDIO_DEVICE_NAME,
			   .pm = &innoaudio_playback_pm_ops,
			   },
	.id_table = s_inno_audio_platform_device_id_table,
};
