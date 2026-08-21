#include <linux/version.h>
#include <linux/types.h>
#include <sound/pcm.h>
#include "inno_audio.h"
#include "inno_misc.h"

u32 fh2m_INNO_SNDRV_PCM_RATE_5512_FUNC(void)
{
	return SNDRV_PCM_RATE_5512;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_5512_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_8000_FUNC(void)
{
	return SNDRV_PCM_RATE_8000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_8000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_11025_FUNC(void)
{
	return SNDRV_PCM_RATE_11025;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_11025_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_16000_FUNC(void)
{
	return SNDRV_PCM_RATE_16000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_16000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_22050_FUNC(void)
{
	return SNDRV_PCM_RATE_22050;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_22050_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_32000_FUNC(void)
{
	return SNDRV_PCM_RATE_32000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_32000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_44100_FUNC(void)
{
	return SNDRV_PCM_RATE_44100;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_44100_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_48000_FUNC(void)
{
	return SNDRV_PCM_RATE_48000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_48000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_64000_FUNC(void)
{
	return SNDRV_PCM_RATE_64000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_64000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_88200_FUNC(void)
{
	return SNDRV_PCM_RATE_88200;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_88200_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_96000_FUNC(void)
{
	return SNDRV_PCM_RATE_96000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_96000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_176400_FUNC(void)
{
	return SNDRV_PCM_RATE_176400;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_176400_FUNC);

u32 fh2m_INNO_SNDRV_PCM_RATE_192000_FUNC(void)
{
	return SNDRV_PCM_RATE_192000;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_RATE_192000_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_STOP_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_STOP;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_STOP_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_START_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_START;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_START_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_PAUSE_PUSH;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_PAUSE_RELEASE;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_SUSPEND_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_SUSPEND;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_SUSPEND_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_RESUME_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_RESUME;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_RESUME_FUNC);

u32 fh2m_INNO_SNDRV_PCM_TRIGGER_DRAIN_FUNC(void)
{
	return SNDRV_PCM_TRIGGER_DRAIN;
}
INNO_EXT_SYM(fh2m_INNO_SNDRV_PCM_TRIGGER_DRAIN_FUNC);

void fh2m_inno_snd_pcm_period_elapsed(void *substream)
{
	snd_pcm_period_elapsed(substream);
}
INNO_EXT_SYM(fh2m_inno_snd_pcm_period_elapsed);

unsigned long long fh2m_inno_snd_pcm_playback_avail(void *substream)
{
	return snd_pcm_playback_avail(((struct snd_pcm_substream *)substream)->runtime);
}
INNO_EXT_SYM(fh2m_inno_snd_pcm_playback_avail);

unsigned int fh2m_inno_audio_get_runtime_sample_bits(
							inno_snd_pcm_substream *substream)
{
	return ((struct snd_pcm_substream *)substream)->runtime->sample_bits;
}
INNO_EXT_SYM(fh2m_inno_audio_get_runtime_sample_bits);

unsigned int fh2m_inno_audio_get_runtime_rate(
							inno_snd_pcm_substream *substream)
{
	return ((struct snd_pcm_substream *)substream)->runtime->rate;
}
INNO_EXT_SYM(fh2m_inno_audio_get_runtime_rate);

void fh2m_inno_audio_set_runtime_hw(inno_snd_pcm_substream *substream, void *hw)
{
	struct snd_pcm_substream *sub = (struct snd_pcm_substream *)substream;

	memcpy(&sub->runtime->hw, hw, sizeof(struct snd_pcm_hardware));
}
INNO_EXT_SYM(fh2m_inno_audio_set_runtime_hw);

void fh2m_inno_pcm_hw_set_buffer_bytes_max(inno_snd_pcm_hardware *pcm_hw, unsigned long val)
{
	struct snd_pcm_hardware *hw = (struct snd_pcm_hardware *)pcm_hw;
	hw->buffer_bytes_max = val;
}
INNO_EXT_SYM(fh2m_inno_pcm_hw_set_buffer_bytes_max);

void fh2m_inno_pcm_hw_set_period_bytes_max(inno_snd_pcm_hardware *pcm_hw, unsigned long val)
{
	struct snd_pcm_hardware *hw = (struct snd_pcm_hardware *)pcm_hw;
	hw->period_bytes_max = val;
}
INNO_EXT_SYM(fh2m_inno_pcm_hw_set_period_bytes_max);

void fh2m_inno_pcm_hw_set_period_bytes_min(inno_snd_pcm_hardware *pcm_hw, unsigned long val)
{
	struct snd_pcm_hardware *hw = (struct snd_pcm_hardware *)pcm_hw;
	hw->period_bytes_min = val;
}
INNO_EXT_SYM(fh2m_inno_pcm_hw_set_period_bytes_min);
