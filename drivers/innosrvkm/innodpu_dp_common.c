#include "innodpu_common.h"
#include "hal_interface.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif

#include "innodpu_dp.h"
#include "innodpu_dp_common.h"
#include "innodpu_connector.h"

struct dp_ext_t {
	inno_waitqueue_head *dp_enc_wq;
	atomic64_t enc_status;
	inno_waitqueue_head *dp_dec_wq;
	atomic64_t dec_status;

	inno_waitqueue_head *dp_aux_wq;
	atomic64_t aux_status;

	spinlock_t intr_slock;

	atomic64_t hpg_status;

	atomic64_t audio_status;

	struct i2c_adapter adapter;
	struct i2c_algo_dp_aux_data algo;

	struct edid *edid;
};

void dp_set_aux_status(struct dp_chip_t *chip, int val)
{
	atomic64_set(&chip->dp_ext->aux_status, val);
}

long long dp_get_aux_status(struct dp_chip_t *chip)
{
	return atomic64_read(&chip->dp_ext->aux_status);
}

void dp_set_hpg_status(struct dp_chip_t *chip, int val)
{
	atomic64_set(&chip->dp_ext->hpg_status, val);
}
long long dp_get_hpg_status(struct dp_chip_t *chip)
{
	return atomic64_read(&chip->dp_ext->hpg_status);
}

void dp_set_audio_status(struct dp_chip_t *chip, int val)
{
	atomic64_set(&chip->dp_ext->audio_status, val);
}
long long dp_get_audio_status(struct dp_chip_t *chip)
{
	return atomic64_read(&chip->dp_ext->audio_status);
}

void *dp_get_edid(struct dp_chip_t *chip)
{
	return chip->dp_ext->edid;
}
void dp_set_edid(struct dp_chip_t *chip, void *val)
{
	chip->dp_ext->edid = (struct edid *)val;
}

int dp_ext_init(struct dp_chip_t *chip)
{
	struct dp_ext_t *ext = NULL;

	ext = fh2m_inno_kzalloc_kernel(sizeof(struct dp_ext_t));
	if (!ext) {
		fh2m_innodpu_err(chip->dev, "dp ext init faild\n");
		return -ENOMEM;
	}

	chip->dp_ext = ext;

	return 0;
}

void dp_ext_fini(struct dp_chip_t *chip)
{
	struct dp_ext_t *ext = chip->dp_ext;

	if (ext)
		fh2m_inno_kfree(ext);

	chip->dp_ext = NULL;
}
