#ifndef __INNOVGA_DEBUGFS__
#define __INNOVGA_DEBUGFS__

#include "img_defs.h"

int inno_vga_auto_setup_ctrl(struct vga_device_t *vga, bool enable);
int inno_vga_auto_setup(struct vga_device_t *vga, bool active);

#if defined(CONFIG_DEBUG_FS)
extern int inno_vga_custom_debugfs_create(struct drm_connector *connector);
#endif
#endif
