/*******************************************************************************
@File			innodpu_compatibility.h
@Title
@Copyright		Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description		Implements the client side of the bridge for sync
			which is used in calls from Server context.
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
********************************************************************************/
#ifndef __INNODPU_COMPATIBILITY_H__
#define __INNODPU_COMPATIBILITY_H__
#include <linux/dma-buf.h>
#include "inno_drm_version.h"
#include <linux/slab.h>
#include <drm/drm_vma_manager.h>
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
#include <drm/drm_drv.h>
#endif
#include <drm/drm_gem.h>

#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
#include <drm/drm_prime.h>
#include <drm/drm_file.h>
#endif

#include <drm/drm_connector.h>
#include <drm/drm_framebuffer.h>

#if (DRM_VERSION < KERNEL_VERSION(5, 5, 0))
#include <drm/drmP.h>
#endif
#include "kernel_compatibility.h"

/* INNODPU DRM IOCTL FLAGS START */
#if (DRM_VERSION < KERNEL_VERSION(4, 18, 0))
	#define INNODPU_DRM_CONTROL_ALLOW DRM_CONTROL_ALLOW
#else
	#define INNODPU_DRM_CONTROL_ALLOW 0
#endif

#if (DRM_VERSION < KERNEL_VERSION(5, 4, 0))
	#define INNODPU_DRM_UNLOCKED DRM_UNLOCKED
#else
	#define INNODPU_DRM_UNLOCKED 0
#endif

#define INNODPU_DRM_RENDER_ALLOW DRM_RENDER_ALLOW

#define INNODPU_IOCTL_FLAGS (INNODPU_DRM_CONTROL_ALLOW | INNODPU_DRM_UNLOCKED | INNODPU_DRM_RENDER_ALLOW)

#if (DRM_VERSION <= KERNEL_VERSION(4, 18, 0))
#define idr_init_base(x,y) idr_init(x);
#endif

#if (DRM_VERSION >= KERNEL_VERSION(5, 2, 0))
#define	DRIVER_PRIME 0
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 17, 0))
typedef int vm_fault_t;
#endif

#if ((DRM_VERSION >= KERNEL_VERSION(5, 14, 0)))
#include <linux/dma-resv.h>

#define dma_resv_wait_timeout_rcu dma_resv_wait_timeout
#define dma_resv_test_signaled_rcu dma_resv_test_signaled

#elif (DRM_VERSION >= KERNEL_VERSION(5, 4, 0))
#include <linux/dma-resv.h>
#else
#include <linux/reservation.h>
#define dma_resv_lock reservation_object_lock
#define dma_resv_unlock reservation_object_unlock

/* Reservation object types */
#define dma_resv            reservation_object
#define dma_resv_list           reservation_object_list

/* Reservation object functions */
#define dma_resv_add_excl_fence     reservation_object_add_excl_fence
#define dma_resv_add_shared_fence   reservation_object_add_shared_fence
#define dma_resv_fini           reservation_object_fini
#define dma_resv_get_excl       reservation_object_get_excl
#define dma_resv_get_list       reservation_object_get_list
#define dma_resv_held           reservation_object_held
#define dma_resv_init           reservation_object_init
#define dma_resv_reserve_shared     reservation_object_reserve_shared
#define dma_resv_test_signaled_rcu  reservation_object_test_signaled_rcu
#define dma_resv_wait_timeout_rcu   reservation_object_wait_timeout_rcu
#define dma_resv_get_excl_rcu  reservation_object_get_excl_rcu
#define dma_resv_get_fences_rcu reservation_object_get_fences_rcu
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 20, 0))
#define vmf_insert_pfn vm_insert_pfn
#endif
#if (DRM_VERSION <= KERNEL_VERSION(4, 20, 0))
int drm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
#endif

static inline bool drm_valid_cea_vic(u8 vic)
{
	return ((vic > 0) && (vic < 108));
}

#if (DRM_VERSION <= KERNEL_VERSION(4, 19, 0))
static inline u32 inno_drm_connector_mask(const struct drm_connector *connector)
{
	return 1 << connector->index;
}
#else
static inline u32 inno_drm_connector_mask(const struct drm_connector *connector)
{
	return drm_connector_mask(connector);
}
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
#define for_each_oldnew_crtc_in_state(__state, crtc, old_crtc_state, new_crtc_state, __i) \
	for ((__i) = 0;							\
	     (__i) < (__state)->dev->mode_config.num_crtc;		\
	     (__i)++)							\
		for_each_if ((__state)->crtcs[__i].ptr &&		\
			     ((crtc) = (__state)->crtcs[__i].ptr,	\
			      (void)(crtc) /* Only to avoid unused-but-set-variable warning */, \
			     (old_crtc_state) = (__state)->crtcs[__i].ptr->state, \
			     (void)(old_crtc_state) /* Only to avoid unused-but-set-variable warning */, \
			     (new_crtc_state) = (__state)->crtcs[__i].state, \
			     (void)(new_crtc_state) /* Only to avoid unused-but-set-variable warning */, 1))

#define for_each_oldnew_connector_in_state(__state, connector, old_connector_state, new_connector_state, __i) \
	for ((__i) = 0;								\
	     (__i) < (__state)->num_connector;					\
	     (__i)++)								\
		for_each_if ((__state)->connectors[__i].ptr &&			\
			     ((connector) = (__state)->connectors[__i].ptr,	\
			     (void)(connector) /* Only to avoid unused-but-set-variable warning */, \
			     (old_connector_state) = (__state)->connectors[__i].ptr->state,	\
			     (new_connector_state) = (__state)->connectors[__i].state, 1))

#define for_each_oldnew_plane_in_state(__state, plane, old_plane_state, new_plane_state, __i) \
	for ((__i) = 0;							\
	     (__i) < (__state)->dev->mode_config.num_total_plane;	\
	     (__i)++)							\
		for_each_if ((__state)->planes[__i].ptr &&		\
			     ((plane) = (__state)->planes[__i].ptr,	\
			      (void)(plane) /* Only to avoid unused-but-set-variable warning */, \
			      (old_plane_state) = (__state)->planes[__i].ptr->state,\
			      (new_plane_state) = (__state)->planes[__i].state, 1))

#define drm_atomic_get_new_crtc_state(__state, crtc) \
	__state->crtcs[drm_crtc_index(crtc)].state \

#define drm_atomic_get_old_crtc_state(__state, crtc) \
	__state->crtcs[drm_crtc_index(crtc)].ptr->state;

#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
/**
 * DRM_MODE_FMT - printf string for &struct drm_display_mode
 */
#define DRM_MODE_FMT    "\"%s\": %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x"

/**
 * DRM_MODE_ARG - printf arguments for &struct drm_display_mode
 * @m: display mode
 */
#define DRM_MODE_ARG(m) \
	(m)->name, drm_mode_vrefresh(m), (m)->clock, \
	(m)->hdisplay, (m)->hsync_start, (m)->hsync_end, (m)->htotal, \
	(m)->vdisplay, (m)->vsync_start, (m)->vsync_end, (m)->vtotal, \
	(m)->type, (m)->flags
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 11, 0))
struct drm_connector_list_iter {
/* private: */
	struct drm_device *dev;
	struct drm_connector *conn;
};
void drm_connector_list_iter_begin(struct drm_device *dev,
				   struct drm_connector_list_iter *iter);
void drm_connector_list_iter_end(struct drm_connector_list_iter *iter);
#define drm_for_each_connector_iter(connector, iter) \
	    drm_for_each_connector(connector, ((struct drm_connector_list_iter*)iter)->dev)

inline unsigned int kref_read(const struct kref *r);

#define drm_helper_mode_fill_fb_struct(drm_dev, fb, mode_cmd) \
	drm_helper_mode_fill_fb_struct(fb, mode_cmd)
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 10, 0))
#define HDMI_PICTURE_ASPECT_64_27 0x3
#endif

#endif//__INNODPU_COMPATIBILITY_H__
