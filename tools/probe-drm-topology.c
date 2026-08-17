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

		printf("  index=%u id=%u active=%s fb=%u position=%u,%u "
		       "size=%ux%u mode=%s refresh=%u\n",
		       i, crtc.crtc_id, crtc.mode_valid ? "yes" : "no", crtc.fb_id,
		       crtc.x, crtc.y, crtc.mode.hdisplay, crtc.mode.vdisplay,
		       crtc.mode_valid ? crtc.mode.name : "-",
		       crtc.mode_valid ? crtc.mode.vrefresh : 0);
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
