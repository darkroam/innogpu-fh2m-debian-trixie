#ifndef __INNO_AUDIO_OSFUNC_H
#define __INNO_AUDIO_OSFUNC_H

typedef void inno_snd_pcm_hardware ;
typedef void inno_snd_pcm_substream;
typedef void inno_snd_pcm_runtime;

u32 fh2m_INNO_SNDRV_PCM_RATE_5512_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_8000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_11025_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_16000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_22050_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_32000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_44100_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_48000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_64000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_88200_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_96000_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_176400_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_RATE_192000_FUNC(void);
#define INNO_SNDRV_PCM_RATE_5512	fh2m_INNO_SNDRV_PCM_RATE_5512_FUNC()
#define INNO_SNDRV_PCM_RATE_8000	fh2m_INNO_SNDRV_PCM_RATE_8000_FUNC()
#define INNO_SNDRV_PCM_RATE_11025	fh2m_INNO_SNDRV_PCM_RATE_11025_FUNC()
#define INNO_SNDRV_PCM_RATE_16000	fh2m_INNO_SNDRV_PCM_RATE_16000_FUNC()
#define INNO_SNDRV_PCM_RATE_22050	fh2m_INNO_SNDRV_PCM_RATE_22050_FUNC()
#define INNO_SNDRV_PCM_RATE_32000	fh2m_INNO_SNDRV_PCM_RATE_32000_FUNC()
#define INNO_SNDRV_PCM_RATE_44100	fh2m_INNO_SNDRV_PCM_RATE_44100_FUNC()
#define INNO_SNDRV_PCM_RATE_48000	fh2m_INNO_SNDRV_PCM_RATE_48000_FUNC()
#define INNO_SNDRV_PCM_RATE_64000	fh2m_INNO_SNDRV_PCM_RATE_64000_FUNC()
#define INNO_SNDRV_PCM_RATE_88200	fh2m_INNO_SNDRV_PCM_RATE_88200_FUNC()
#define INNO_SNDRV_PCM_RATE_96000	fh2m_INNO_SNDRV_PCM_RATE_96000_FUNC()
#define INNO_SNDRV_PCM_RATE_176400	fh2m_INNO_SNDRV_PCM_RATE_176400_FUNC()
#define INNO_SNDRV_PCM_RATE_192000	fh2m_INNO_SNDRV_PCM_RATE_192000_FUNC()


u32 fh2m_INNO_SNDRV_PCM_TRIGGER_STOP_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_START_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_SUSPEND_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_RESUME_FUNC(void);
u32 fh2m_INNO_SNDRV_PCM_TRIGGER_DRAIN_FUNC(void);

#define INNO_SNDRV_PCM_TRIGGER_STOP             fh2m_INNO_SNDRV_PCM_TRIGGER_STOP_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_START            fh2m_INNO_SNDRV_PCM_TRIGGER_START_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH       fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE    fh2m_INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_SUSPEND          fh2m_INNO_SNDRV_PCM_TRIGGER_SUSPEND_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_RESUME           fh2m_INNO_SNDRV_PCM_TRIGGER_RESUME_FUNC()
#define INNO_SNDRV_PCM_TRIGGER_DRAIN            fh2m_INNO_SNDRV_PCM_TRIGGER_DRAIN_FUNC()

void fh2m_inno_snd_pcm_period_elapsed(void *substream);
unsigned long long fh2m_inno_snd_pcm_playback_avail(void *runtime);
unsigned int fh2m_inno_audio_get_runtime_sample_bits(inno_snd_pcm_substream *substream);
unsigned int fh2m_inno_audio_get_runtime_rate(inno_snd_pcm_substream *substream);
void fh2m_inno_audio_set_runtime_hw(inno_snd_pcm_substream *substream, void *hw);
void fh2m_inno_pcm_hw_set_buffer_bytes_max(inno_snd_pcm_hardware *pcm_hw, unsigned long val);
void fh2m_inno_pcm_hw_set_period_bytes_max(inno_snd_pcm_hardware *pcm_hw, unsigned long val);
void fh2m_inno_pcm_hw_set_period_bytes_min(inno_snd_pcm_hardware *pcm_hw, unsigned long val);

#endif