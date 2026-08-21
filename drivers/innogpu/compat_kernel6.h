#ifndef _COMPAT_KERNEL6_H
#define _COMPAT_KERNEL6_H

#include <linux/version.h>
#include <linux/string.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)

#include <drm/drm_edid.h>

/*
 * drm_do_get_edid was removed in kernel 6.9+.
 * Implement using drm_edid_read_custom (same callback signature).
 * Returns a kmalloc'd struct edid* that caller must kfree.
 */
static inline struct edid *drm_do_get_edid(struct drm_connector *connector,
    int (*get_edid_block)(void *data, u8 *buf, unsigned int block, size_t len),
    void *data)
{
    const struct drm_edid *drm_edid;
    const struct edid *raw;
    struct edid *edid_copy;
    size_t edid_size;

    drm_edid = drm_edid_read_custom(connector, get_edid_block, data);
    if (!drm_edid)
        return NULL;

    raw = drm_edid_raw(drm_edid);
    if (!raw) {
        drm_edid_free(drm_edid);
        return NULL;
    }

    /* EDID is 128 bytes per block (base + extensions) */
    edid_size = (raw->extensions + 1) * EDID_LENGTH;
    edid_copy = kmalloc(edid_size, GFP_KERNEL);
    if (edid_copy)
        memcpy(edid_copy, raw, edid_size);

    drm_edid_free(drm_edid);
    return edid_copy;
}

/* drm_edid_block_valid was removed in kernel 6.9+ */
static inline int drm_edid_block_valid(u8 *raw_edid, int block, bool print_bad_edid, bool *edid_corrupt)
{
    /* Check EDID header for block 0 */
    if (block == 0) {
        static const u8 header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
        if (memcmp(raw_edid, header, sizeof(header)) == 0)
            return 1;
        return 0;
    }
    /* Extension blocks: assume valid */
    return 1;
}

#endif /* KERNEL_VERSION >= 6.9 */
#endif /* _COMPAT_KERNEL6_H */
