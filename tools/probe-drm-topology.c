#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <drm/drm_mode.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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

/* codex 初审 P1-3：采集失败必须显式 unavailable + 最终 rc≠0，
 * 不得把「未采集到」伪装成真实的 modes=0 / ddcci-props=none。 */
static int collect_failed = 0;

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

/* 内核 connector 命名：<类型名>-<type_id>（与 /sys/class/drm/cardN-<name> 一致） */
static const char *connector_type_name(uint32_t type)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_VGA: return "VGA";
	case DRM_MODE_CONNECTOR_DVII: return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID: return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA: return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite: return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO: return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS: return "LVDS";
	case DRM_MODE_CONNECTOR_Component: return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN: return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA: return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB: return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV: return "TV";
	case DRM_MODE_CONNECTOR_eDP: return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL: return "Virtual";
	case DRM_MODE_CONNECTOR_DSI: return "DSI";
	case DRM_MODE_CONNECTOR_DPI: return "DPI";
	case DRM_MODE_CONNECTOR_WRITEBACK: return "Writeback";
	case DRM_MODE_CONNECTOR_SPI: return "SPI";
	default: return "Unknown";
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

/*
 * F5 输出契约（validation-plan §1.2 R7-R9 证据格式，固定）：
 *   connector 段：每 connector 一行
 *     `connector <id> <name> status=<connected|disconnected> modes=<count>`
 *     + 全部 mode 行 `<hdisp>x<vdisp>@<vrefresh>`；
 *   DDCCI 属性段：props 中名称匹配 (?i)ddcci 的
 *     `connector <id> prop <name>=<value>`；全无 →
 *     `connector <id> ddcci-props=none`；
 *   backlight 关联表：`<backlight_name> -> <drm_connector_path>`（经
 *     /sys/class/backlight/<name>/device symlink 解析；失败 →
 *     `<backlight_name> -> unresolved`，不计入任何 connector 判定）。
 * ns 前缀：生产=""；夹具="fixture_"。
 */

/* DDCCI props dump：DRM_IOCTL_MODE_OBJ_GETPROPERTIES + 逐 prop GETPROPERTY
 * （raw ioctl，无 libdrm 链接依赖，与仓库其余 C 探针构建口径一致） */
static void print_connector_props(int fd, uint32_t connector_id, const char *ns)
{
	struct drm_mode_obj_get_properties props = {0};
	props.obj_id = connector_id;
	props.obj_type = DRM_MODE_OBJECT_CONNECTOR;
	if (ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &props)) {
		collect_failed = 1;
		printf("  %sconnector %u ddcci-props=unavailable (props query failed: %s)\n",
		       ns, connector_id, strerror(errno));
		return;
	}
	if (!props.count_props) {
		printf("  %sconnector %u ddcci-props=none\n", ns, connector_id);
		return;
	}
	uint32_t *prop_ids = calloc(props.count_props, sizeof(*prop_ids));
	uint64_t *prop_vals = calloc(props.count_props, sizeof(*prop_vals));
	if (!prop_ids || !prop_vals) {
		free(prop_ids);
		free(prop_vals);
		collect_failed = 1;
		printf("  %sconnector %u ddcci-props=unavailable (alloc failed)\n", ns, connector_id);
		return;
	}
	props.props_ptr = (uintptr_t)prop_ids;
	props.prop_values_ptr = (uintptr_t)prop_vals;
	if (ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &props)) {
		free(prop_ids);
		free(prop_vals);
		collect_failed = 1;
		printf("  %sconnector %u ddcci-props=unavailable (props fetch failed: %s)\n",
		       ns, connector_id, strerror(errno));
		return;
	}
	int ddcci_found = 0;
	int prop_fetch_failed = 0;
	for (uint32_t i = 0; i < props.count_props; i++) {
		struct drm_mode_get_property prop = {.prop_id = prop_ids[i]};
		char name[DRM_PROP_NAME_LEN + 1] = {0};
		if (ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop)) {
			/* codex 复审 P1-2：单个属性查询失败不得伪装 none */
			prop_fetch_failed = 1;
			continue;
		}
		memcpy(name, prop.name, DRM_PROP_NAME_LEN);
		if (!strcasestr(name, "ddcci"))
			continue;
		printf("  %sconnector %u prop %s=%llu\n", ns, connector_id, name,
		       (unsigned long long)prop_vals[i]);
		ddcci_found = 1;
	}
	if (!ddcci_found) {
		if (prop_fetch_failed) {
			collect_failed = 1;
			printf("  %sconnector %u ddcci-props=unavailable (some prop fetch failed)\n",
			       ns, connector_id);
		} else {
			printf("  %sconnector %u ddcci-props=none\n", ns, connector_id);
		}
	} else if (prop_fetch_failed) {
		collect_failed = 1;
		printf("  %sconnector %u ddcci-props=unavailable (some props unreadable)\n",
		       ns, connector_id);
	}
	free(prop_ids);
	free(prop_vals);
}

/* 单 connector 的 F5 契约行 + mode 枚举 + DDCCI props；modes_ok=0 时
 * modes 采集失败 → 契约行 modes=unavailable（不伪装 0）。 */
static void print_connector_contract(int fd,
				     const struct drm_mode_get_connector *connector,
				     struct drm_mode_modeinfo *modes,
				     const char *ns, int modes_ok)
{
	const char *cname = connector_type_name(connector->connector_type);
	if (modes_ok) {
		printf("  %sconnector %u %s-%u status=%s modes=%u\n",
		       ns, connector->connector_id, cname, connector->connector_type_id,
		       connection_name(connector->connection), connector->count_modes);
		for (uint32_t i = 0; i < connector->count_modes; i++)
			printf("  %s%ux%u@%u\n", ns,
			       modes[i].hdisplay, modes[i].vdisplay, modes[i].vrefresh);
	} else {
		printf("  %sconnector %u %s-%u status=%s modes=unavailable\n",
		       ns, connector->connector_id, cname, connector->connector_type_id,
		       connection_name(connector->connection));
	}
	print_connector_props(fd, connector->connector_id, ns);
}

/* backlight 关联表：/sys/class/backlight/<name>/device symlink 解析到
 * drm connector 路径；解析失败、目标非 drm、或形态不匹配
 * /drm/card<u>/card<u>-<connector> → unresolved（禁止全局误归因）；
 * 目录不可读 → 显式 unavailable + collect_failed（codex 复审 P2-5）。 */
static void print_backlight_table(const char *backlight_root, const char *ns)
{
	DIR *dir = opendir(backlight_root);
	if (!dir) {
		collect_failed = 1;
		printf("  %sbacklight=unavailable (opendir %s failed: %s)\n",
		       ns, backlight_root, strerror(errno));
		return;
	}
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		char devlink[PATH_MAX];
		snprintf(devlink, sizeof(devlink), "%s/%s/device",
			 backlight_root, ent->d_name);
		char resolved[PATH_MAX];
		ssize_t n = readlink(devlink, resolved, sizeof(resolved) - 1);
		if (n < 0) {
			printf("  %s%s -> unresolved\n", ns, ent->d_name);
			continue;
		}
		resolved[n] = '\0';
		char abslink[PATH_MAX];
		snprintf(abslink, sizeof(abslink), "%s/%s/", backlight_root,
			 ent->d_name);
		char *rp = resolved[0] == '/' ? resolved : NULL;
		char joined[PATH_MAX * 2];
		if (!rp) {
			snprintf(joined, sizeof(joined), "%s%s", abslink, resolved);
			rp = realpath(joined, NULL);
		} else {
			rp = realpath(resolved, NULL);
		}
		int matched = 0;
		if (rp) {
			/* 形态限定：<...>/drm/card<u>/card<u>-<connector>，
			 * 两个 card 编号必须一致，且路径到 connector 名即
			 * 结束（codex 复审 P2：/drm/card0/card1-eDP-1 或带
			 * 额外路径组件必须拒绝） */
			unsigned int ca, cb;
			char cname[64];
			int consumed = 0;
			char *p = strstr(rp, "/drm/");
			if (p && sscanf(p, "/drm/card%u/card%u-%63[^/]%n",
					&ca, &cb, cname, &consumed) == 3 &&
			    ca == cb && p[consumed] == '\0')
				matched = 1;
		}
		if (matched) {
			printf("  %s%s -> %s\n", ns, ent->d_name, rp);
		} else {
			printf("  %s%s -> unresolved\n", ns, ent->d_name);
		}
		free(rp);
	}
	closedir(dir);
}

/* 聚合运行模式开关：仅当环境变量显式为 "1" 才启用（codex 复审 P2：
 * =0/空值/任意非 "1" 值不得绕过 F5 采集） */
static int crtc_only_mode(void)
{
	const char *v = getenv("INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY");
	return v && strcmp(v, "1") == 0;
}

#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS
/*
 * 契约测试（仅测试构建，-DINNOGPU_DMABUF_FIXTURE_HOOKS）：设置
 * INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 时跳过真实 ioctl，用伪造 CRTC/connector
 * 数据驱动生产输出函数（CRTC 三态 + F5 connector/mode/DDCCI 段）；
 * backlight 关联从 INNOGPU_DMABUF_BACKLIGHT_ROOT 读取（夹具可注入含
 * unresolved 的假目录）。生产构建不定义该宏，环境变量无任何效果。
 */
static int fixture_topology_contract(void)
{
	const char *ns = "fixture_";
	struct drm_mode_crtc crtcs[3] = {0};
	uint32_t ids[3] = {10, 11, 12};

	/* 采集失败路径契约样本（codex 初审 P1-3 负向）：modes/ddcci 采集
	 * 失败必须输出 unavailable 且 rc=1，不得伪装 0/none。 */
	if (getenv("INNOGPU_DMABUF_TOPOLOGY_FIXTURE_FAIL")) {
		printf("%sdevice=fixture crtcs=0 connectors=1 encoders=0\n", ns);
		printf("CRTCs:\n");
		printf("Connectors:\n");
		printf("  %sconnector 51 eDP-1 status=connected modes=unavailable\n", ns);
		printf("  %sconnector 51 ddcci-props=unavailable (props query failed: fixture)\n", ns);
		printf("Backlight:\n");
		printf("%stopology_collect=partial\n", ns);
		return 1;
	}

	crtcs[0].crtc_id = ids[0];
	crtcs[0].mode_valid = 0;
	crtcs[1].crtc_id = ids[1];
	crtcs[1].mode_valid = 1;
	crtcs[1].fb_id = 1;
	crtcs[1].mode.hdisplay = 1920;
	crtcs[1].mode.vdisplay = 1080;
	snprintf(crtcs[1].mode.name, DRM_DISPLAY_MODE_LEN, "1920x1080");
	crtcs[1].mode.vrefresh = 60;
	crtcs[2].crtc_id = ids[2];
	crtcs[2].mode_valid = 1;
	crtcs[2].fb_id = 1;
	crtcs[2].mode.hdisplay = 1920;
	crtcs[2].mode.vdisplay = 1080;
	crtcs[2].mode.name[0] = '\0';
	crtcs[2].mode.vrefresh = 60;

	/* 聚合运行模式（dmabuf R3+R6 拓扑门禁只消费 CRTC 段）：
	 * INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY=1 时跳过 F5 契约段与 backlight，
	 * rc 仅反映 CRTC 段完整性；R7-R9 证据运行不得设置（完整采集，
	 * partial 输出必须被消费者拒绝）。 */
	if (crtc_only_mode()) {
		printf("%sdevice=fixture crtcs=3 connectors=0 encoders=0\n", ns);
		printf("CRTCs:\n");
		for (uint32_t i = 0; i < 3; i++)
			print_crtc_line(i, &crtcs[i]);
		printf("%stopology_collect=complete\n", ns);
		return 0;
	}

	printf("%sdevice=fixture crtcs=3 connectors=2 encoders=1\n", ns);
	printf("CRTCs:\n");
	for (uint32_t i = 0; i < 3; i++)
		print_crtc_line(i, &crtcs[i]);

	/* F5 契约段：connected eDP-1（2 modes，无 ddcci props）+ disconnected
	 * HDMI-A-1（0 modes）——输出函数直接驱动（print_connector_contract 的
	 * mode/props 部分在夹具侧用契约行替换，props 查询需要真实 fd）。 */
	printf("Connectors:\n");
	printf("  %sconnector 51 eDP-1 status=connected modes=2\n", ns);
	printf("  %s1920x1200@60\n", ns);
	printf("  %s1920x1200@48\n", ns);
	printf("  %sconnector 51 ddcci-props=none\n", ns);
	printf("  %sconnector 52 HDMI-A-1 status=disconnected modes=0\n", ns);
	printf("  %sconnector 52 ddcci-props=none\n", ns);

	printf("Backlight:\n");
	const char *blroot = getenv("INNOGPU_DMABUF_BACKLIGHT_ROOT");
	if (blroot && *blroot)
		print_backlight_table(blroot, ns);
	else
		printf("  %sbacklight=not-injected (set INNOGPU_DMABUF_BACKLIGHT_ROOT)\n", ns);
	printf("%stopology_collect=%s\n", ns, collect_failed ? "partial" : "complete");
	return collect_failed ? 1 : 0;
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
		return fixture_topology_contract();
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
			/* codex 复审 P1-4：CRTC 查询失败计入整体失败 */
			collect_failed = 1;
			printf("  index=%u id=%u error=%s\n", i, crtc_ids[i],
			       strerror(errno));
			continue;
		}

		print_crtc_line(i, &crtc);
	}

	/* 聚合运行模式（dmabuf R3+R6 拓扑门禁只消费 CRTC 段）：
	 * INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY=1 跳过 F5 契约段与 backlight；
	 * R7-R9 证据运行不得设置（完整采集，partial 必须被消费者拒绝）。 */
	if (!crtc_only_mode()) {
		printf("Connectors:\n");
		for (uint32_t i = 0; i < resources.count_connectors; i++) {
			struct drm_mode_get_connector connector = {
				.connector_id = connector_ids[i],
			};
			struct drm_mode_get_encoder encoder = {0};
			uint32_t encoder_crtc = 0;
			int index = -1;

			if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector)) {
				collect_failed = 1;
				printf("  id=%u error=%s\n", connector_ids[i],
				       strerror(errno));
				printf("  connector %u unavailable (GETCONNECTOR failed: %s)\n",
				       connector_ids[i], strerror(errno));
				continue;
			}

			if (connector.encoder_id) {
				encoder.encoder_id = connector.encoder_id;
				if (!ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &encoder)) {
					encoder_crtc = encoder.crtc_id;
					index = crtc_index(&resources, crtc_ids, encoder_crtc);
				} else {
					/* codex 复审 P1-4：encoder 查询失败计入整体失败 */
					collect_failed = 1;
				}
			}

			printf("  type=%u type_id=%u id=%u status=%s mm=%ux%u modes=%u "
			       "encoder=%u crtc_id=%u crtc_index=%d\n",
			       connector.connector_type, connector.connector_type_id,
			       connector.connector_id, connection_name(connector.connection),
			       connector.mm_width, connector.mm_height, connector.count_modes,
			       connector.encoder_id, encoder_crtc, index);

			/* F5 契约段：connector 契约行 + mode 枚举 + DDCCI props */
			struct drm_mode_modeinfo *modes = NULL;
			uint32_t count_modes = connector.count_modes;
			int modes_ok = 0;
			if (count_modes) {
				modes = calloc(count_modes, sizeof(*modes));
				if (modes) {
					connector.modes_ptr = (uintptr_t)modes;
					connector.count_props = 0;
					connector.props_ptr = 0;
					connector.prop_values_ptr = 0;
					if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector)) {
						free(modes);
						modes = NULL;
						count_modes = 0;
					} else {
						modes_ok = 1;
					}
				} else {
					count_modes = 0;
				}
			} else {
				modes_ok = 1;   /* 真实 0 modes 是合法采集结果 */
			}
			if (!modes_ok)
				collect_failed = 1;
			{
				struct drm_mode_get_connector contract = {
					.connector_id = connector_ids[i],
					.connector_type = connector.connector_type,
					.connector_type_id = connector.connector_type_id,
					.connection = connector.connection,
					.count_modes = count_modes,
				};
				print_connector_contract(fd, &contract, modes, "", modes_ok);
			}
			free(modes);
		}

		printf("Backlight:\n");
		print_backlight_table("/sys/class/backlight", "");
	}

	printf("topology_collect=%s\n", collect_failed ? "partial" : "complete");
	result = collect_failed ? 1 : 0;

out_free:
	free(encoder_ids);
	free(connector_ids);
	free(crtc_ids);
out_close:
	close(fd);
	return result;
}
