#define _POSIX_C_SOURCE 200809L

#include <drm/drm_mode.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int crtc_index(const struct drm_mode_card_res *resources,
		      const uint32_t *crtc_ids, uint32_t crtc_id)
{
	for (uint32_t i = 0; i < resources->count_crtcs; i++) {
		if (crtc_ids[i] == crtc_id)
			return (int)i;
	}

	return -1;
}

static const char *connection_name(uint32_t connection)
{
	switch (connection) {
	case 1:
		return "connected";
	case 2:
		return "disconnected";
	case 3:
		return "unknown";
	default:
		return "invalid";
	}
}

/* CRTC 行输出契约（真实 ioctl 路径与 fixture 契约测试共用）：
 * 三态 mode 名选择——inactive -> "-"；active 且 mode 名称非空 -> 原值；
 * active 且 mode 名称为空 -> 稳定占位 "<unnamed>"（空名称绝不产生空字段）。 */
static void print_crtc_line(uint32_t index, const struct drm_mode_crtc *crtc)
{
	const char *mode_name =
		!crtc->mode_valid ? "-"
		: crtc->mode.name[0] != '\0' ? crtc->mode.name
		: "<unnamed>";

	printf("  index=%u id=%u active=%s fb=%u position=%u,%u "
	       "size=%ux%u mode=%s refresh=%u\n",
	       index, crtc->crtc_id, crtc->mode_valid ? "yes" : "no", crtc->fb_id,
	       crtc->x, crtc->y, crtc->mode.hdisplay, crtc->mode.vdisplay,
	       mode_name,
	       crtc->mode_valid ? crtc->mode.vrefresh : 0);
}

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
/* 契约测试（仅测试构建，-DINNOGPU_DMABUF_FIXTURE_HOOKS）：当设置
 * INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 时跳过真实 ioctl，用伪造 CRTC 数据直接
 * 驱动 print_crtc_line 的三态选择（inactive/-、active 具名、active 无名），
 * 验证生产实现本身。生产构建不定义该宏，环境变量无任何效果。 */
static int fixture_crtc_contract(void)
{
	struct drm_mode_crtc crtcs[3] = {0};
	uint32_t ids[3] = {10, 11, 12};

	/* inactive：mode_valid=0 -> mode=- */
	crtcs[0].crtc_id = ids[0];
	crtcs[0].mode_valid = 0;
	/* active 且具名 -> 保留原 mode 名称 */
	crtcs[1].crtc_id = ids[1];
	crtcs[1].mode_valid = 1;
	crtcs[1].fb_id = 1;
	crtcs[1].mode.hdisplay = 1920;
	crtcs[1].mode.vdisplay = 1080;
	snprintf(crtcs[1].mode.name, DRM_DISPLAY_MODE_LEN, "1920x1080");
	crtcs[1].mode.vrefresh = 60;
	/* active 且 mode 名称为空 -> <unnamed> */
	crtcs[2].crtc_id = ids[2];
	crtcs[2].mode_valid = 1;
	crtcs[2].fb_id = 1;
	crtcs[2].mode.hdisplay = 1920;
	crtcs[2].mode.vdisplay = 1080;
	crtcs[2].mode.name[0] = '\0';
	crtcs[2].mode.vrefresh = 60;

	printf("device=fixture crtcs=3 connectors=0 encoders=0\n");
	printf("CRTCs:\n");
	for (uint32_t i = 0; i < 3; i++)
		print_crtc_line(i, &crtcs[i]);
	return 0;
}
#endif

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/dri/card0";
	struct drm_mode_card_res resources = {0};
	uint32_t *connector_ids = NULL;
	uint32_t *encoder_ids = NULL;
	uint32_t *crtc_ids = NULL;
	uint32_t crtc_capacity;
	uint32_t connector_capacity;
	int result = 1;
	int fd;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [card-device]\n", argv[0]);
		return 2;
	}

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
	if (getenv("INNOGPU_DMABUF_TOPOLOGY_FIXTURE"))
		return fixture_crtc_contract();
#endif

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
		return 1;
	}

	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources)) {
		fprintf(stderr, "DRM_IOCTL_MODE_GETRESOURCES failed: %s\n",
			strerror(errno));
		goto out_close;
	}

	crtc_capacity = resources.count_crtcs;
	connector_capacity = resources.count_connectors;
	crtc_ids = calloc(resources.count_crtcs, sizeof(*crtc_ids));
	connector_ids = calloc(resources.count_connectors, sizeof(*connector_ids));
	encoder_ids = calloc(resources.count_encoders, sizeof(*encoder_ids));
	if ((resources.count_crtcs && !crtc_ids) ||
	    (resources.count_connectors && !connector_ids) ||
	    (resources.count_encoders && !encoder_ids)) {
		fprintf(stderr, "resource array allocation failed\n");
		goto out_free;
	}

	resources.crtc_id_ptr = (uintptr_t)crtc_ids;
	resources.connector_id_ptr = (uintptr_t)connector_ids;
	resources.encoder_id_ptr = (uintptr_t)encoder_ids;
	resources.count_fbs = 0;
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources)) {
		fprintf(stderr, "second DRM_IOCTL_MODE_GETRESOURCES failed: %s\n",
			strerror(errno));
		goto out_free;
	}
	if (resources.count_crtcs > crtc_capacity ||
	    resources.count_connectors > connector_capacity) {
		fprintf(stderr, "DRM resources changed while probing; retry\n");
		goto out_free;
	}

	printf("device=%s crtcs=%u connectors=%u encoders=%u\n", device,
	       resources.count_crtcs, resources.count_connectors,
	       resources.count_encoders);
	printf("CRTCs:\n");
	for (uint32_t i = 0; i < resources.count_crtcs; i++) {
		struct drm_mode_crtc crtc = {.crtc_id = crtc_ids[i]};

		if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc)) {
			printf("  index=%u id=%u error=%s\n", i, crtc_ids[i],
			       strerror(errno));
			continue;
		}

		print_crtc_line(i, &crtc);
	}

	printf("Connectors:\n");
	for (uint32_t i = 0; i < resources.count_connectors; i++) {
		struct drm_mode_get_connector connector = {
			.connector_id = connector_ids[i],
		};
		struct drm_mode_get_encoder encoder = {0};
		uint32_t encoder_crtc = 0;
		int index = -1;

		if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector)) {
			printf("  id=%u error=%s\n", connector_ids[i],
			       strerror(errno));
			continue;
		}

		if (connector.encoder_id) {
			encoder.encoder_id = connector.encoder_id;
			if (!ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &encoder)) {
				encoder_crtc = encoder.crtc_id;
				index = crtc_index(&resources, crtc_ids, encoder_crtc);
			}
		}

		printf("  type=%u type_id=%u id=%u status=%s mm=%ux%u modes=%u "
		       "encoder=%u crtc_id=%u crtc_index=%d\n",
		       connector.connector_type, connector.connector_type_id,
		       connector.connector_id, connection_name(connector.connection),
		       connector.mm_width, connector.mm_height, connector.count_modes,
		       connector.encoder_id, encoder_crtc, index);
	}

	result = 0;

out_free:
	free(encoder_ids);
	free(connector_ids);
	free(crtc_ids);
out_close:
	close(fd);
	return result;
}
