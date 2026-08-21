#include "innoaudio_drv.h"
#include "g0_soc_audio.h"
#include "g1_soc_audio.h"
#include "inno_audio.h"
#include "inno_lock.h"
#include "inno_timer.h"
#include "inno_misc.h"


#define TRANS_BYTE_IN_1MS 150 //todo


static struct snd_pcm_hardware hardware_default = {
	.info = (SNDRV_PCM_INFO_MMAP |
			 SNDRV_PCM_INFO_INTERLEAVED |
			 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
			 SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE),
	.rates = (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000),
	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 40 * 1024,
	.period_bytes_min = 5 * 1024,
	.period_bytes_max = 10 * 1024,
	.periods_min = PERIOD_MIN,
	.periods_max = PERIOD_MAX,
};

void innoaudio_init_hw_param(struct audio_chip_t *chip)
{
	struct snd_pcm_hardware hw_param;

	if (!chip) {
		return ;
	}

	memcpy(&hw_param, &hardware_default, sizeof(struct snd_pcm_hardware));

	if (chip->init_hw_param){
		chip->init_hw_param(chip, &hw_param);
		return;
	}

	fh2m_inno_pcm_hw_set_buffer_bytes_max(&hw_param, chip->buf_max * 1024);
	fh2m_inno_pcm_hw_set_period_bytes_max(&hw_param, chip->pmax * 1024);
	fh2m_inno_pcm_hw_set_period_bytes_min(&hw_param, chip->pmin * 1024);
	fh2m_inno_audio_set_runtime_hw(chip->audio_playback_sub, &hw_param);

	innoaudio_info(chip->dev, "buf_max: %d, pmax: %d, pmin: %d, off_fix:%d",
					chip->buf_max*1024, chip->pmax*1024, chip->pmin*1024, chip->off_fix);

}

static void innoaudio_clk_set(struct audio_chip_t *chip)
{
	unsigned int pll0, pll1, sample_rate;

	if (!chip) {
		return ;
	}

	if (chip->set_clk){
		chip->set_clk(chip);
		return;
	}

	sample_rate = fh2m_inno_audio_get_runtime_rate(chip->audio_playback_sub);
	// Set PLL = 24*(fbdiv       + frac      /2^24) / prediv      * postdiv   * 5
	// 	         24*(pll0[31:20] + pll1[31:8]/2^24) / pll0[19:14] * pll1[6:0] * 5

	switch(sample_rate) {
		case 24000:
			pll0 = (128<<20) | (1<<14) | (0x0348);
			pll1 = 50;
			break;
		case 32000:
			pll0 = (236<<20) | (3<<14) | (0x0348);
			pll1 = 25;
			break;
		case 44100:
			pll0 = 0x07504348;//(117<<20) | (1<<14) | (0x0348);
			pll1 = 0x99999919;//(0x999999 << 8) | 0x19;
			break;
		case 48000:
			pll0 = (128<<20) | (1<<14) | (0x0348);
			pll1 = 25;
			break;
		case 96000:
			pll0 = (256<<20) | (5<<14) | (0x0348);
			pll1 = 5;
			break;
		case 192000:
			pll0 = (512<<20) | (5<<14) | (0x0348);
			pll1 = 5;
			break;
		default:
			pll0 = (128<<20) | (1<<14) | (0x0348);
			pll1 = 25;
			innoaudio_err("sample_rate : %d unsupport, use 48000!!!\n", sample_rate);
	}
	innoaudio_info(chip->dev, "sample_rate: %d, pll0 : 0x%x, pll1 : 0x%x", sample_rate, pll0, pll1);
	fh2m_hal_audio_set_pll(chip->parent, chip->reg_module, pll0, pll1);
}

void innoaudio_hw_init(struct audio_chip_t *chip)
{
	if (!chip) {
		innoaudio_err("chip null\n");
		return ;
	}

	innoaudio_clk_set(chip);

	if(chip->hw_init)
		chip->hw_init(chip);
}

void innoaudio_hw_close(struct audio_chip_t *chip)
{
	if (!chip) {
		innoaudio_err("chip null\n");
		return ;
	}

	chip->hw_close(chip);
}

int innoaudio_chip_init(struct audio_chip_t *chip)
{
	chip_type_e plat;
	int retcode = 0;

	plat = fh2m_hal_get_chiptype(chip->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		innoaudio_info(chip->dev, "start init g1 audio-%d.\n", chip->id);
		retcode = g1_soc_audio_chip_init(chip);
		break;
	case CHIP_G0_SOC:
		innoaudio_info(chip->dev, "start init g0 audio-%d.\n", chip->id);
		retcode = g0_soc_audio_chip_init(chip);
		break;
	case CHIP_G0M_SOC:
		innoaudio_info(chip->dev, "start init g0m audio-%d.\n", chip->id);
		retcode = g0m_soc_audio_chip_init(chip);
		break;
	case CHIP_G1P_SOC:
		innoaudio_info(chip->dev, "start init g1p audio-%d.\n", chip->id);
		retcode = g1p_soc_audio_chip_init(chip);
		break;
	default:
		innoaudio_err("Does not currently support %d platform.\n", plat);
		retcode = -EINVAL;
		break;
	}

	chip->type = plat;
	return retcode;
}


void innoaudio_chip_fini(struct audio_chip_t *chip)
{
	if (!chip) {
		innoaudio_err("chip null\n");
		return ;
	}

	chip->chip_fini(chip);

	return;
}

/* audio dma have no hardware pointer,so we have to simulate it with software.
   version 1: get the start address of the current buffer and returns directly
              in this version, the sound effect is as follows:
              login/logout music    :        no noise
              video player          :        no noise
              music player          :        no noise
              aiqiy online video    :        no noise
              tencent online video  :        no noise
              bilibili online video :        no noise
              youku online video    :        nosie if adjust volume
           so, there is version 2
   version 2: the alsa-lib constantly call get_offset(),we adjust the returned
              buffer pointer based on the number of calls
              the sound effect is as follows:
              login/logout music    :      noise
              video player		    : 	   no noise
              music player		    : 	   no noise
              aiqiy online video	: 	   no noise
              tencent online video  : 	   no noise
              bilibili online video : 	   no noise
              youku online video	: 	   no noise
   version 3: we perform special processing on login music, use version 1
              login/logout music	: 	 noise but very small
              video player		    : 	 no noise
              music player		    : 	 no noise
              aiqiy online video	: 	 no noise
              tencent online video  : 	 no noise
              bilibili online video : 	 no noise
              youku online video	: 	 no noise
   version 4: we use jiffies to localization the dma ptr
              login/logout music	: 	 no noise
              video player		    : 	 no noise
              music player		    : 	 no noise
              aiqiy online video	: 	 no noise
              tencent online video  : 	 no noise
              bilibili online video : 	 no noise
              youku online video	: 	 no noise
*/
int innoaudio_get_offset(struct audio_chip_t *chip)
{
	int ret_offset = 0;
	unsigned long delt_tm = 0, delt_off = 0, onebuf_tm = 0;
	unsigned int onebuf_len = 0;

	if (!chip) {
		return -EINVAL;
	}

	if (chip->get_offset){
		return chip->get_offset(chip);
	}
	chip->offset_time = jiffies;

	onebuf_len =  chip->buf_period;
	onebuf_tm = onebuf_len / TRANS_BYTE_IN_1MS;

	delt_tm = fh2m_inno_jiffies_to_msecs(fh2m_inno_abs(chip->offset_time - chip->irq_time));
	if(delt_tm > onebuf_tm * 4) {
		delt_tm = delt_tm % (((onebuf_len * 4) / TRANS_BYTE_IN_1MS));
	}
	delt_off = delt_tm * TRANS_BYTE_IN_1MS;
	ret_offset = onebuf_len * chip->irq_buf + delt_off;
	if(ret_offset > onebuf_len * 4)
		ret_offset -= onebuf_len * 4;

	return ret_offset;
}

void innoaudio_hw_start(struct audio_chip_t *chip)
{
	if(chip->irq_enable)
		chip->irq_enable(chip);

	if(chip->start)
		chip->start(chip);
}

void innoaudio_hw_stop(struct audio_chip_t *chip)
{
	if(chip->irq_disable)
		chip->irq_disable(chip);

	if(chip->stop)
		chip->stop(chip);
}

void innoaudio_hw_pause(struct audio_chip_t *chip, int st)
{
	if(chip->irq_disable)
		chip->irq_disable(chip);

	if(chip->pause)
		chip->pause(chip, st);
}

void innoaudio_hw_set_output(struct audio_chip_t *chip, int id, int st)
{
	if(chip->set_output)
		chip->set_output(chip, id, st);
}

int innoaudio_substream_ctrl(struct audio_chip_t *chip, int cmd)
{
	unsigned long flags;
	int err = 0;

	fh2m_inno_spin_lock_irqsave(chip->audio_reg_lock, &flags);

	if((cmd == INNO_SNDRV_PCM_TRIGGER_START) || \
			(cmd == INNO_SNDRV_PCM_TRIGGER_RESUME)){
		innoaudio_info(chip->dev, "playback start, cmd=%d, buf:%d, byte/ms:%d\n",
									cmd, chip->buf_max, TRANS_BYTE_IN_1MS);
		chip->irq_flag = true;
		innoaudio_hw_start(chip);
	}else if((cmd == INNO_SNDRV_PCM_TRIGGER_STOP) || \
			(cmd == INNO_SNDRV_PCM_TRIGGER_SUSPEND)) {
		innoaudio_info(chip->dev, "playback stop, cmd=%d\n", cmd);
		chip->irq_flag = false;
		innoaudio_hw_stop(chip);
	}else if(cmd == INNO_SNDRV_PCM_TRIGGER_PAUSE_PUSH){
		innoaudio_info(chip->dev, "playback pause, cmd=%d\n", cmd);
		innoaudio_hw_pause(chip, 1);
	}else if(cmd == INNO_SNDRV_PCM_TRIGGER_PAUSE_RELEASE){
		innoaudio_info(chip->dev, "playback pause release, cmd=%d\n", cmd);
		innoaudio_hw_pause(chip, 0);
	}else {
		err = -EINVAL;
	}

	chip->offset_time = jiffies;
	chip->irq_time = jiffies;
	chip->irq_buf = chip->get_curr_buf(chip);;

	fh2m_inno_spin_unlock_irqrestore(chip->audio_reg_lock, flags);
	return err;
}

int innoaudio_suspend(struct audio_chip_t *chip)
{
	if (!chip) {
		innoaudio_err("suspend failed!, chip is NULL\n");
		return -EINVAL;
	}

	fh2m_inno_memset_io_portable(chip->vaddr, 0, chip->audio_buf_size);
	fh2m_hal_dev_disable_irq(chip->parent, chip->hal_module);

	if (chip->audio_suspend){
		return chip->audio_suspend(chip);
	}

	innoaudio_hw_stop(chip);
	innoaudio_info(chip->dev, "innoaudio suspend\n");
	return 0;
}

int innoaudio_resume(struct audio_chip_t *chip)
{
	if (!chip) {
		innoaudio_err("resume failed!, chip is NULL\n");
		return -EINVAL;
	}

	innoaudio_info(chip->dev, "innoaudio wakeup\n");

	fh2m_inno_memset_io_portable(chip->vaddr, 0, chip->audio_buf_size);
	fh2m_hal_dev_enable_irq(chip->parent, chip->hal_module);

	if (chip->audio_resume){
		return chip->audio_resume(chip);
	}

	innoaudio_hw_start(chip);
	return 0;
}

