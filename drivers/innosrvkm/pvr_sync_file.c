/*
 * @File        pvr_sync_file.c
 * @Title       Kernel driver for Android's sync mechanism
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
#include <linux/anon_inodes.h>
#endif

#include "services_kernel_client.h"
#include "pvr_drv.h"
#include "pvr_sync.h"
#include "pvr_fence.h"
#include "pvr_counting_timeline.h"
#include "pvr_export_fence.h"
#include "linux_sw_sync.h"
#include "pvr_sync_api.h"

#include "innogpu_drm.h"
#include "inno_task.h"
#include "inno_fence.h"
#include "inno_debug.h"
/* This header must always be included last */
#include "kernel_compatibility.h"
#include "pvr_linux_fence.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)) && !defined(CHROMIUMOS_KERNEL)
#define sync_file_user_name(s)	((s)->name)
#else
#define sync_file_user_name(s)	((s)->user_name)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#ifndef CONFIG_SYNC_FILE

#include <uapi/linux/sync_file.h>
static const struct file_operations sync_file_fops;
static struct sync_file *sync_file_alloc(void)
{
	struct sync_file *sync_file;

	sync_file = kzalloc(sizeof(*sync_file), GFP_KERNEL);
	if (!sync_file)
		return NULL;

	sync_file->file = anon_inode_getfile("sync_file", &sync_file_fops,
					     sync_file, 0);
	if (IS_ERR(sync_file->file))
		goto err;

	kref_init(&sync_file->kref);

	init_waitqueue_head(&sync_file->wq);

	INIT_LIST_HEAD(&sync_file->cb.node);

	return sync_file;

err:
	kfree(sync_file);
	return NULL;
}

static void fence_check_cb_func(struct fence *f, struct fence_cb *cb)
{
	struct sync_file *sync_file;

	sync_file = container_of(cb, struct sync_file, cb);

	wake_up_all(&sync_file->wq);
}

/**
 * sync_file_create() - creates a sync file
 * @fence:	fence to add to the sync_fence
 *
 * Creates a sync_file containg @fence. Once this is called, the sync_file
 * takes ownership of @fence. The sync_file can be released with
 * fput(sync_file->file). Returns the sync_file or NULL in case of error.
 */
struct sync_file *pvr_sync_file_create(struct fence *fence)
{
	struct sync_file *sync_file;

	sync_file = sync_file_alloc();
	if (!sync_file)
		return NULL;

#if 0
	sync_file->fence = fence;
#else
	sync_file->fence = fence_get(fence);
#endif

	snprintf(sync_file->name, sizeof(sync_file->name), "%s-%s%llu-%d",
		 fence->ops->get_driver_name(fence),
		 fence->ops->get_timeline_name(fence), fence->context,
		 fence->seqno);

	return sync_file;
}

/**
 * sync_file_fdget() - get a sync_file from an fd
 * @fd:		fd referencing a fence
 *
 * Ensures @fd references a valid sync_file, increments the refcount of the
 * backing file. Returns the sync_file or NULL in case of error.
 */
static struct sync_file *sync_file_fdget(int fd)
{
	struct file *file = fget(fd);

	if (!file)
		return NULL;

	if (file->f_op != &sync_file_fops)
		goto err;

	return file->private_data;

err:
	fput(file);
	return NULL;
}

/**
 * sync_file_get_fence - get the fence related to the sync_file fd
 * @fd:		sync_file fd to get the fence from
 *
 * Ensures @fd references a valid sync_file and returns a fence that
 * represents all fence in the sync_file. On error NULL is returned.
 */
struct fence *pvr_sync_file_get_fence(int fd)
{
	struct sync_file *sync_file;
	struct fence *fence;

	sync_file = sync_file_fdget(fd);
	if (!sync_file)
		return NULL;

	fence = fence_get(sync_file->fence);
	fput(sync_file->file);

	return fence;
}

static int sync_file_set_fence(struct sync_file *sync_file,
			       struct fence **fences, int num_fences)
{
	struct fence_array *array;

	/*
	 * The reference for the fences in the new sync_file and held
	 * in add_fence() during the merge procedure, so for num_fences == 1
	 * we already own a new reference to the fence. For num_fence > 1
	 * we own the reference of the fence_array creation.
	 */
	if (num_fences == 1) {
		sync_file->fence = fences[0];
		kfree(fences);
	} else {
		array = fence_array_create(num_fences, fences,
					   fence_context_alloc(1), 1, false);
		if (!array)
			return -ENOMEM;

		sync_file->fence = &array->base;
	}

	return 0;
}

static struct fence **get_fences(struct sync_file *sync_file, int *num_fences)
{
	if (fence_is_array(sync_file->fence)) {
		struct fence_array *array = to_fence_array(sync_file->fence);

		*num_fences = array->num_fences;
		return array->fences;
	}

	*num_fences = 1;
	return &sync_file->fence;
}

static void add_fence(struct fence **fences, int *i, struct fence *fence)
{
	fences[*i] = fence;

	if (!fence_is_signaled(fence)) {
		fence_get(fence);
		(*i)++;
	}
}

/**
 * sync_file_merge() - merge two sync_files
 * @name:	name of new fence
 * @a:		sync_file a
 * @b:		sync_file b
 *
 * Creates a new sync_file which contains copies of all the fences in both
 * @a and @b.  @a and @b remain valid, independent sync_file. Returns the
 * new merged sync_file or NULL in case of error.
 */
static struct sync_file *sync_file_merge(const char *name, struct sync_file *a,
					 struct sync_file *b)
{
	struct sync_file *sync_file;
	struct fence **fences, **nfences, **a_fences, **b_fences;
	int i, i_a, i_b, num_fences, a_num_fences, b_num_fences;

	sync_file = sync_file_alloc();
	if (!sync_file)
		return NULL;

	a_fences = get_fences(a, &a_num_fences);
	b_fences = get_fences(b, &b_num_fences);
	if (a_num_fences > INT_MAX - b_num_fences)
		return NULL;

	num_fences = a_num_fences + b_num_fences;

	fences = kcalloc(num_fences, sizeof(*fences), GFP_KERNEL);
	if (!fences)
		goto err;

	/*
	 * Assume sync_file a and b are both ordered and have no
	 * duplicates with the same context.
	 *
	 * If a sync_file can only be created with sync_file_merge
	 * and sync_file_create, this is a reasonable assumption.
	 */
	for (i = i_a = i_b = 0; i_a < a_num_fences && i_b < b_num_fences; ) {
		struct fence *pt_a = a_fences[i_a];
		struct fence *pt_b = b_fences[i_b];

		if (pt_a->context < pt_b->context) {
			add_fence(fences, &i, pt_a);

			i_a++;
		} else if (pt_a->context > pt_b->context) {
			add_fence(fences, &i, pt_b);

			i_b++;
		} else {
			if (pt_a->seqno - pt_b->seqno <= INT_MAX)
				add_fence(fences, &i, pt_a);
			else
				add_fence(fences, &i, pt_b);

			i_a++;
			i_b++;
		}
	}

	for (; i_a < a_num_fences; i_a++)
		add_fence(fences, &i, a_fences[i_a]);

	for (; i_b < b_num_fences; i_b++)
		add_fence(fences, &i, b_fences[i_b]);

	if (i == 0)
		fences[i++] = fence_get(a_fences[0]);

	if (num_fences > i) {
		nfences = krealloc(fences, i * sizeof(*fences),
				  GFP_KERNEL);
		if (!nfences)
			goto err;

		fences = nfences;
	}

	if (sync_file_set_fence(sync_file, fences, i) < 0) {
		kfree(fences);
		goto err;
	}

	fh2m_inno_strlcpy(sync_file->name, name, sizeof(sync_file->name));
	return sync_file;

err:
	fput(sync_file->file);
	return NULL;

}

static void sync_file_free(struct kref *kref)
{
	struct sync_file *sync_file = container_of(kref, struct sync_file,
						     kref);

	if (test_bit(POLL_ENABLED, &sync_file->fence->flags))
		fence_remove_callback(sync_file->fence, &sync_file->cb);
	fence_put(sync_file->fence);
	kfree(sync_file);
}

static int sync_file_release(struct inode *inode, struct file *file)
{
	struct sync_file *sync_file = file->private_data;

	kref_put(&sync_file->kref, sync_file_free);
	return 0;
}


static unsigned int sync_file_poll(struct file *file, poll_table *wait)
{
	struct sync_file *sync_file = file->private_data;

	poll_wait(file, &sync_file->wq, wait);

	if (!poll_does_not_wait(wait) &&
	    !test_and_set_bit(POLL_ENABLED, &sync_file->fence->flags)) {
		if (fence_add_callback(sync_file->fence, &sync_file->cb,
				       fence_check_cb_func) < 0)
			wake_up_all(&sync_file->wq);
	}

	return fence_is_signaled(sync_file->fence) ? POLLIN : 0;
}


static long sync_file_ioctl_merge(struct sync_file *sync_file,
				  unsigned long arg)
{
	int fd = get_unused_fd_flags(O_CLOEXEC);
	int err;
	struct sync_file *fence2, *fence3;
	struct sync_merge_data data;

	if (fd < 0)
		return fd;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
		err = -EFAULT;
		goto err_put_fd;
	}

	if (data.flags || data.pad) {
		err = -EINVAL;
		goto err_put_fd;
	}

	fence2 = sync_file_fdget(data.fd2);
	if (!fence2) {
		err = -ENOENT;
		goto err_put_fd;
	}

	data.name[sizeof(data.name) - 1] = '\0';
	fence3 = sync_file_merge(data.name, sync_file, fence2);
	if (!fence3) {
		err = -ENOMEM;
		goto err_put_fence2;
	}

	data.fence = fd;
	if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
		err = -EFAULT;
		goto err_put_fence3;
	}

	fd_install(fd, fence3->file);
	fput(fence2->file);
	return 0;

err_put_fence3:
	fput(fence3->file);

err_put_fence2:
	fput(fence2->file);

err_put_fd:
	put_unused_fd(fd);
	return err;
}

static void sync_fill_fence_info(struct fence *fence,
				 struct sync_fence_info *info)
{
	fh2m_inno_strlcpy(info->obj_name, fence->ops->get_timeline_name(fence),
		sizeof(info->obj_name));
	fh2m_inno_strlcpy(info->driver_name, fence->ops->get_driver_name(fence),
		sizeof(info->driver_name));
	if (fence_is_signaled(fence))
		info->status = fence->error >= 0 ? 1 : fence->error;
	else
		info->status = 0;
	info->timestamp_ns = ktime_to_ns(fence->timestamp);
}

static long sync_file_ioctl_fence_info(struct sync_file *sync_file,
				       unsigned long arg)
{
	struct sync_file_info info;
	struct sync_fence_info *fence_info = NULL;
	struct fence **fences;
	__u32 size;
	int num_fences, ret, i;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	if (info.flags || info.pad)
		return -EINVAL;

	fences = get_fences(sync_file, &num_fences);

	/*
	 * Passing num_fences = 0 means that userspace doesn't want to
	 * retrieve any sync_fence_info. If num_fences = 0 we skip filling
	 * sync_fence_info and return the actual number of fences on
	 * info->num_fences.
	 */
	if (!info.num_fences)
		goto no_fences;

	if (info.num_fences < num_fences)
		return -EINVAL;

	size = num_fences * sizeof(*fence_info);
	fence_info = kzalloc(size, GFP_KERNEL);
	if (!fence_info)
		return -ENOMEM;

	for (i = 0; i < num_fences; i++)
		sync_fill_fence_info(fences[i], &fence_info[i]);

	if (copy_to_user(u64_to_user_ptr(info.sync_fence_info), fence_info,
			 size)) {
		ret = -EFAULT;
		goto out;
	}

no_fences:
	fh2m_inno_strlcpy(info.name, sync_file->name, sizeof(info.name));
	info.status = fence_is_signaled(sync_file->fence);
	info.num_fences = num_fences;

	if (copy_to_user((void __user *)arg, &info, sizeof(info)))
		ret = -EFAULT;
	else
		ret = 0;

out:
	kfree(fence_info);

	return ret;
}

static long sync_file_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	struct sync_file *sync_file = file->private_data;

	switch (cmd) {
	case SYNC_IOC_MERGE:
		return sync_file_ioctl_merge(sync_file, arg);

	case SYNC_IOC_FILE_INFO:
		return sync_file_ioctl_fence_info(sync_file, arg);

	default:
		return -ENOTTY;
	}
}

static const struct file_operations sync_file_fops = {
	.release = sync_file_release,
	.poll = sync_file_poll,
	.unlocked_ioctl = sync_file_ioctl,
	.compat_ioctl = sync_file_ioctl,
};


#define sync_file_get_fence pvr_sync_file_get_fence
#define sync_file_create pvr_sync_file_create
#endif
#endif

#define PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf, pvDumpDebugFile, fmt, ...) \
	do {                                                             \
		if (pfnDumpDebugPrintf)                                  \
			pfnDumpDebugPrintf(pvDumpDebugFile, fmt,         \
					   ## __VA_ARGS__);              \
		else                                                     \
			inno_error(fmt "\n", ## __VA_ARGS__);                \
	} while (0)

#define	FILE_NAME "pvr_sync_file"

struct sw_sync_create_fence_data {
	__u32 value;
	char name[32];
	__s32 fence;
};
#define SW_SYNC_IOC_MAGIC 'W'
#define SW_SYNC_IOC_CREATE_FENCE \
	(_IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data))
#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

/* Global data for the sync driver */
static struct {
	struct pvr_fence_context *foreign_fence_context_list;
	PFN_SYNC_CHECKPOINT_STRUCT sync_checkpoint_ops;
} pvr_sync_data;

#if defined(NO_HARDWARE)
static DEFINE_MUTEX(pvr_timeline_active_list_lock);
static struct list_head pvr_timeline_active_list;
#endif

/* This is the actual timeline metadata. We might keep this around after the
 * base sync driver has destroyed the pvr_sync_timeline_wrapper object.
 */
struct pvr_sync_timeline {
	char name[32];
	void *file_handle;
	bool is_sw;
	bool is_export;
	/* Fence context used for hw fences */
	struct pvr_fence_context *hw_fence_context;
	/* Timeline and context for sw fences */
	union {
		struct pvr_counting_fence_timeline *sw_fence_timeline;
		struct pvr_fence_context *exp_fence_context;
	};
#if defined(NO_HARDWARE)
	/* List of all timelines (used to advance all timelines in nohw builds) */
	struct list_head list;
#endif
};

static
void pvr_sync_free_checkpoint_list_mem(void *mem_ptr)
{
	kfree(mem_ptr);
}

#if defined(NO_HARDWARE)
/* function used to signal pvr fence in nohw builds */
static
void pvr_sync_nohw_signal_fence(void *fence_data_to_signal)
{
	struct pvr_sync_timeline *this_timeline = NULL;

	mutex_lock(&pvr_timeline_active_list_lock);
	list_for_each_entry(this_timeline, &pvr_timeline_active_list, list) {
		pvr_fence_context_signal_fences_nohw(this_timeline->hw_fence_context);
	}
	mutex_unlock(&pvr_timeline_active_list_lock);
}
#endif

static struct pvr_sync_timeline *pvr_sync_timeline_fget(int fd)
{
	struct file *file = fget(fd);
	struct pvr_sync_timeline *timeline;

	if (!file)
		return NULL;

	timeline = pvr_sync_get_api_priv(file);
	if (!timeline)
		fput(file);

	return timeline;
}

static void pvr_sync_timeline_fput(struct pvr_sync_timeline *timeline)
{
	struct file *file = pvr_sync_get_file_struct(timeline->file_handle);

	if (file)
		fput(file);
	else
		inno_error(FILE_NAME ": %s: Timeline incomplete\n", __func__);
}

/* ioctl and fops handling */

int pvr_sync_api_init(void *file_handle, void **api_priv)
{
	struct pvr_sync_timeline *timeline;
	char task_comm[TASK_COMM_LEN];

	get_task_comm(task_comm, current);

	timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);
	if (!timeline)
		return -ENOMEM;

	fh2m_inno_strlcpy(timeline->name, task_comm, sizeof(timeline->name));
	timeline->file_handle = file_handle;
	timeline->is_sw = false;
	timeline->is_export = false;

	*api_priv = (void *)timeline;

	return 0;
}

int pvr_sync_api_deinit(void *api_priv, bool is_sw)
{
	struct pvr_sync_timeline *timeline = api_priv;

	if (!timeline)
		return 0;

	if (timeline->sw_fence_timeline) {
		/* This makes sure any outstanding SW syncs are marked as
		 * complete at timeline close time. Otherwise it'll leak the
		 * timeline (as outstanding fences hold a ref) and possibly
		 * wedge the system if something is waiting on one of those
		 * fences
		 */
		pvr_counting_fence_timeline_force_complete(
			timeline->sw_fence_timeline);
		pvr_counting_fence_timeline_put(timeline->sw_fence_timeline);
	}

	if (timeline->hw_fence_context) {
#if defined(NO_HARDWARE)
		mutex_lock(&pvr_timeline_active_list_lock);
		list_del(&timeline->list);
		mutex_unlock(&pvr_timeline_active_list_lock);
#endif
		pvr_fence_context_destroy(timeline->hw_fence_context);
	}

	kfree(timeline);

	return 0;
}

/*
 * This is the function that kick code will call in order to 'finalise' a
 * created output fence just prior to returning from the kick function.
 * The OS native sync code needs to implement a function meeting this
 * specification - the implementation may be a nop if the OS does not need
 * to perform any actions at this point.
 *
 * Input: fence_fd            The PVRSRV_FENCE to be 'finalised'. This value
 *                            will have been returned by an earlier call to
 *                            pvr_sync_create_fence().
 * Input: finalise_data       The finalise data returned by an earlier call
 *                            to pvr_sync_create_fence().
 */
static enum PVRSRV_ERROR_TAG
pvr_sync_finalise_fence(PVRSRV_FENCE fence_fd, void *finalise_data)
{
	struct sync_file *sync_file = finalise_data;
	struct pvr_fence *pvr_fence;

	if (!sync_file || (fence_fd < 0)) {
		inno_error(FILE_NAME ": %s: Invalid input fence\n", __func__);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	pvr_fence = to_pvr_fence(sync_file->fence);

	if (!pvr_fence) {
		inno_error(FILE_NAME ": %s: Fence not a pvr fence\n", __func__);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* pvr fences can be signalled any time after creation */
	dma_fence_enable_sw_signaling(&pvr_fence->base);

	fd_install(fence_fd, sync_file->file);

	return PVRSRV_OK;
}

/*
 * This is the function that kick code will call in order to obtain a new
 * PVRSRV_FENCE from the OS native sync code and the PSYNC_CHECKPOINT used
 * in that fence. The OS native sync code needs to implement a function
 * meeting this specification.
 *
 * Input: device                   Device node to use in creating a hw_fence_ctx
 * Input: fence_name               A string to annotate the fence with (for
 *                                 debug).
 * Input: timeline                 The timeline on which the new fence is to be
 *                                 created.
 * Output: new_fence               The new PVRSRV_FENCE to be returned by the
 *                                 kick call.
 * Output: fence_uid               Unique ID of the update fence.
 * Output: fence_finalise_data     Pointer to data needed to finalise the fence.
 * Output: new_checkpoint_handle   The PSYNC_CHECKPOINT used by the new fence.
 */
static enum PVRSRV_ERROR_TAG
pvr_sync_create_fence(
		      struct _PVRSRV_DEVICE_NODE_ *device,
		      const char *fence_name,
		      PVRSRV_TIMELINE new_fence_timeline,
		      PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
		      PVRSRV_FENCE *new_fence, u64 *fence_uid,
		      void **fence_finalise_data,
		      PSYNC_CHECKPOINT *new_checkpoint_handle,
		      void **timeline_update_sync,
		      __u32 *timeline_update_value)
{
	PVRSRV_ERROR err = PVRSRV_OK;
	PVRSRV_FENCE new_fence_fd = -1;
	struct pvr_sync_timeline *timeline;
	struct pvr_fence *pvr_fence;
	PSYNC_CHECKPOINT checkpoint;
	struct sync_file *sync_file;

	if (new_fence_timeline < 0 || !new_fence || !new_checkpoint_handle
		|| !fence_finalise_data) {
		inno_error(FILE_NAME ": %s: Invalid input params\n", __func__);
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	/* We reserve the new fence FD before taking any operations
	 * as we do not want to fail (e.g. run out of FDs)
	 */
	new_fence_fd = get_unused_fd_flags(O_CLOEXEC);
	if (new_fence_fd < 0) {
		inno_error(FILE_NAME ": %s: Failed to get fd. process_name:%s PID:%d\n", __func__ , fh2m_inno_task_name(), fh2m_inno_task_pid());
		err = PVRSRV_ERROR_UNABLE_TO_ADD_HANDLE;
		goto err_out;
	}

	timeline = pvr_sync_timeline_fget(new_fence_timeline);
	if (!timeline) {
		inno_error(FILE_NAME ": %s: Failed to open supplied timeline fd (%d)\n",
			__func__, new_fence_timeline);
		err = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_put_fd;
	}

	if (timeline->is_sw) {
		/* This should never happen! */
		inno_error(FILE_NAME ": %s: Request to create a pvr fence on sw timeline (%d)\n",
			__func__, new_fence_timeline);
		err = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_put_timeline;
	}

	if (!timeline->hw_fence_context) {
		/* First time we use this timeline, so create a context. */
		timeline->hw_fence_context =
			pvr_fence_context_create(
				device,
				NativeSyncGetFenceStatusWq(),
				timeline->name);
		if (!timeline->hw_fence_context) {
			inno_error(FILE_NAME ": %s: Failed to create fence context (%d)\n",
			       __func__, new_fence_timeline);
			err = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto err_put_timeline;
		}
#if defined(NO_HARDWARE)
		/* Add timeline to active list */
		INIT_LIST_HEAD(&timeline->list);
		mutex_lock(&pvr_timeline_active_list_lock);
		list_add_tail(&timeline->list, &pvr_timeline_active_list);
		mutex_unlock(&pvr_timeline_active_list_lock);
#endif
	}

	pvr_fence = pvr_fence_create(timeline->hw_fence_context,
								 psSyncCheckpointContext,
								 new_fence_timeline,
								 fence_name);
	if (!pvr_fence) {
		inno_error(FILE_NAME ": %s: Failed to create new pvr_fence\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_timeline;
	}

	checkpoint = pvr_fence_get_checkpoint(pvr_fence);
	if (!checkpoint) {
		inno_error(FILE_NAME ": %s: Failed to get fence checkpoint\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_destroy_fence;
	}

	sync_file = sync_file_create(&pvr_fence->base);
	if (!sync_file) {
		inno_error(FILE_NAME ": %s: Failed to create sync_file\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_destroy_fence;
	}
	fh2m_inno_strlcpy(sync_file_user_name(sync_file),
		pvr_fence->name,
		sizeof(sync_file_user_name(sync_file)));
	dma_fence_put(&pvr_fence->base);

	*new_fence = new_fence_fd;
	*fence_finalise_data = sync_file;
	*new_checkpoint_handle = checkpoint;
	*fence_uid = OSGetCurrentClientProcessIDKM();
	*fence_uid = (*fence_uid << 32) | (new_fence_fd & U32_MAX);
	/* not used but don't want to return dangling pointers */
	*timeline_update_sync = NULL;
	*timeline_update_value = 0;

	pvr_sync_timeline_fput(timeline);
err_out:
	return err;

err_destroy_fence:
	pvr_fence_destroy(pvr_fence);
err_put_timeline:
	pvr_sync_timeline_fput(timeline);
err_put_fd:
	put_unused_fd(new_fence_fd);
	*fence_uid = PVRSRV_NO_FENCE;
	goto err_out;
}

/*
 * This is the function that kick code will call in order to 'rollback' a
 * created output fence should an error occur when submitting the kick.
 * The OS native sync code needs to implement a function meeting this
 * specification.
 *
 * Input: fence_to_rollback The PVRSRV_FENCE to be 'rolled back'. The fence
 *                          should be destroyed and any actions taken due to
 *                          its creation that need to be undone should be
 *                          reverted.
 * Input: finalise_data     The finalise data for the fence to be 'rolled back'.
 */
static enum PVRSRV_ERROR_TAG
pvr_sync_rollback_fence_data(PVRSRV_FENCE fence_to_rollback,
			     void *fence_data_to_rollback)
{
	struct sync_file *sync_file = fence_data_to_rollback;
	struct pvr_fence *pvr_fence;

	if (!sync_file || fence_to_rollback < 0) {
		inno_error(FILE_NAME ": %s: Invalid fence (%d)\n", __func__,
			fence_to_rollback);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	pvr_fence = to_pvr_fence(sync_file->fence);
	if (!pvr_fence) {
		inno_error(FILE_NAME
			": %s: Non-PVR fence (%p)\n",
			__func__, sync_file->fence);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	fput(sync_file->file);

	put_unused_fd(fence_to_rollback);

	return PVRSRV_OK;
}

/*
 * This is the function that kick code will call in order to obtain a list of
 * the PSYNC_CHECKPOINTs for a given PVRSRV_FENCE passed to a kick function.
 * The OS native sync code will allocate the memory to hold the returned list
 * of PSYNC_CHECKPOINT ptrs. The caller will free this memory once it has
 * finished referencing it.
 *
 * Input: fence                     The input (check) fence
 * Output: nr_checkpoints           The number of PVRSRV_SYNC_CHECKPOINT ptrs
 *                                  returned in the checkpoint_handles
 *                                  parameter.
 * Output: fence_uid                Unique ID of the check fence
 * Input/Output: checkpoint_handles The returned list of PVRSRV_SYNC_CHECKPOINTs.
 */
static enum PVRSRV_ERROR_TAG
pvr_sync_resolve_fence(PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
		       PVRSRV_FENCE fence_to_resolve, u32 *nr_checkpoints,
		       PSYNC_CHECKPOINT **checkpoint_handles, u64 *fence_uid)
{
	PSYNC_CHECKPOINT *checkpoints = NULL;
	unsigned int i, num_fences = 0, num_used_fences = 0;
	struct dma_fence **fences = NULL;
	struct dma_fence *fence;
	PVRSRV_ERROR err = PVRSRV_OK;

	struct pvr_fence_context *ffctx;
	struct pvr_fence *pvr_fence;
	u32 core_id = SyncCheckpointGetCoreId(psSyncCheckpointContext);

	if (!nr_checkpoints || !checkpoint_handles || !fence_uid) {
		inno_error(FILE_NAME ": %s: Invalid input checkpoint pointer\n",
			__func__);
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	*nr_checkpoints = 0;
	*checkpoint_handles = NULL;
	*fence_uid = 0;

	if (fence_to_resolve < 0)
		goto err_out;

	fence = sync_file_get_fence(fence_to_resolve);
	if (!fence) {
		inno_error(FILE_NAME ": %s: Failed to read sync private data for fd %d\n",
			__func__, fence_to_resolve);
		err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
		goto err_out;
	}

	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);

		if (array) {
			fences = array->fences;
			num_fences = array->num_fences;
		}
	} else {
		fences = &fence;
		num_fences = 1;
	}

	checkpoints = kmalloc_array(num_fences, sizeof(PSYNC_CHECKPOINT),
			      GFP_KERNEL);
	if (!checkpoints) {
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_fence;
	}
	for (i = 0; i < num_fences; i++) {
		/*
		 * Only return the checkpoint if the fence is still active.
		 * Don't checked for signalled on PDUMP drivers as we need
		 * to make sure that all fences make it to the pdump.
		 */
#if !defined(PDUMP)
		if (!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
			      &fences[i]->flags))
#endif
		{
			for(ffctx = pvr_sync_data.foreign_fence_context_list;
				ffctx != NULL; ffctx = ffctx->psNext)
			{
				if(core_id == DeviceNodeHandleGetCoreId(ffctx->pdev_node))
					break;
			}
			pvr_fence = pvr_fence_create_from_fence(
					ffctx,
					psSyncCheckpointContext,
					fences[i],
					fence_to_resolve,
					"foreign");
			if (!pvr_fence) {
				inno_error(FILE_NAME ": %s: Failed to create fence\n",
				       __func__);
				err = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto err_free_checkpoints;
			}
			checkpoints[num_used_fences] =
				pvr_fence_get_checkpoint(pvr_fence);
			SyncCheckpointTakeRef(checkpoints[num_used_fences]);
			++num_used_fences;
			dma_fence_put(&pvr_fence->base);
		}
	}
	/* If we don't return any checkpoints, delete the array because
	 * the caller will not.
	 */
	if (num_used_fences == 0) {
		kfree(checkpoints);
		checkpoints = NULL;
	}

	*checkpoint_handles = checkpoints;
	*nr_checkpoints = num_used_fences;
	*fence_uid = OSGetCurrentClientProcessIDKM();
	*fence_uid = (*fence_uid << 32) | (fence_to_resolve & U32_MAX);

err_put_fence:
	dma_fence_put(fence);
err_out:
	return err;

err_free_checkpoints:
	for (i = 0; i < num_used_fences; i++) {
		if (checkpoints[i])
			SyncCheckpointDropRef(checkpoints[i]);
	}
	kfree(checkpoints);
	goto err_put_fence;
}

/*
 * This is the function that driver code will call in order to request the
 * sync implementation to output debug information relating to any sync
 * checkpoints it may have created which appear in the provided array of
 * FW addresses of Unified Fence Objects (UFOs).
 *
 * Input: fctx                The sync driver context
 * Input: nr_ufos             The number of FW addresses provided in the
 *                            vaddrs parameter.
 * Input: vaddrs              The array of FW addresses of UFOs. The sync
 *                            implementation should check each of these to
 *                            see if any relate to sync checkpoints it has
 *                            created and where they do output debug information
 *                            pertaining to the native/fallback sync with
 *                            which it is associated.
 */
static u32
pvr_sync_dump_info_single_on_stalled_ufos(struct pvr_fence_context *fctx, u32 nr_ufos, u32 *vaddrs)
{
	return pvr_fence_dump_info_on_stalled_ufos(fctx, nr_ufos, vaddrs);
}

/*
 * This is the function that driver code will call in order to request the
 * all sync implementation to output debug information relating to any sync
 * checkpoints it may have created which appear in the provided array of
 * FW addresses of Unified Fence Objects (UFOs).
 *
 * Input: nr_ufos             The number of FW addresses provided in the
 *                            vaddrs parameter.
 * Input: vaddrs              The array of FW addresses of UFOs. The sync
 *                            implementation should check each of these to
 *                            see if any relate to sync checkpoints it has
 *                            created and where they do output debug information
 *                            pertaining to the native/fallback sync with
 *                            which it is associated.
 */
static u32
pvr_sync_dump_info_on_stalled_ufos(u32 nr_ufos, u32 *vaddrs)
{
	u32 count = 0;
	struct pvr_fence_context *fctx;
	for(fctx = pvr_sync_data.foreign_fence_context_list; fctx != NULL; fctx = fctx->psNext)
	{
		count += pvr_sync_dump_info_single_on_stalled_ufos(fctx, nr_ufos, vaddrs);
	}
	return count;
}

#if defined(PDUMP)
static enum PVRSRV_ERROR_TAG
pvr_sync_fence_get_checkpoints(PVRSRV_FENCE fence_to_pdump, u32 *nr_checkpoints,
				struct SYNC_CHECKPOINT_TAG ***checkpoint_handles)
{
	struct dma_fence **fences = NULL;
	struct dma_fence *fence;
	struct pvr_fence *pvr_fence;
	struct SYNC_CHECKPOINT_TAG **checkpoints = NULL;
	unsigned int i, num_fences, num_used_fences = 0;
	enum PVRSRV_ERROR_TAG err;

	if (fence_to_pdump < 0) {
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	if (!nr_checkpoints || !checkpoint_handles) {
		inno_error(FILE_NAME ": %s: Invalid input checkpoint pointer\n",
			__func__);
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	fence = sync_file_get_fence(fence_to_pdump);
	if (!fence) {
		inno_error(FILE_NAME ": %s: Failed to read sync private data for fd %d\n",
			__func__, fence_to_pdump);
		err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
		goto err_out;
	}

	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);

		fences = array->fences;
		num_fences = array->num_fences;
	} else {
		fences = &fence;
		num_fences = 1;
	}

	checkpoints = kmalloc_array(num_fences, sizeof(*checkpoints),
			      GFP_KERNEL);
	if (!checkpoints) {
		inno_error("pvr_sync_file: %s: Failed to alloc memory for returned list of sync checkpoints\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_fence;
	}

	for (i = 0; i < num_fences; i++) {
		pvr_fence = to_pvr_fence(fences[i]);
		if (!pvr_fence)
			continue;
		checkpoints[num_used_fences] = pvr_fence_get_checkpoint(pvr_fence);
		++num_used_fences;
	}

	*checkpoint_handles = checkpoints;
	*nr_checkpoints = num_used_fences;
	err =  PVRSRV_OK;

err_put_fence:
	dma_fence_put(fence);
err_out:
	return err;
}
#endif

int pvr_sync_api_rename(void *api_priv, void *user_data)
{
	struct pvr_sync_timeline *timeline = api_priv;
	struct pvr_sync_rename_ioctl_data *data = user_data;

	data->szName[sizeof(data->szName) - 1] = '\0';
	fh2m_inno_strlcpy(timeline->name, data->szName, sizeof(timeline->name));
	if (timeline->hw_fence_context)
		fh2m_inno_strlcpy(timeline->hw_fence_context->name, data->szName,
			sizeof(timeline->hw_fence_context->name));

	return 0;
}

int pvr_sync_api_force_sw_only(void *api_priv, void **api_priv_new)
{
	struct pvr_sync_timeline *timeline = api_priv;

	/* Already in SW mode? */
	if (timeline->sw_fence_timeline)
		return 0;

	/* Create a sw_sync timeline with the old GPU timeline's name */
	timeline->sw_fence_timeline = pvr_counting_fence_timeline_create(
		timeline->name);
	if (!timeline->sw_fence_timeline)
		return -ENOMEM;

	timeline->is_sw = true;

	/* CHECK THIS !!! */
	*api_priv_new = (void *)timeline;	/* Compare to pvr_sync2.c code */

	return 0;
}

/* We simply treat an export-fence as a SW fence and tweak
 * the timeline structure to flag it as an 'is_export' type.
 */
int pvr_sync_api_force_exp_only(void *api_priv, void *api_data)
{
	struct pvr_sync_timeline *timeline = api_priv;

	if (timeline->is_export) {
		pr_err(FILE_NAME ": %s: Already marked export timeline\n", __func__);
		return 0;
	}

	timeline->is_export = true;

	return 0;
}

int pvr_sync_api_create_export_fence(void *api_priv, void *user_data)
{
	struct pvr_sync_timeline *timeline = api_priv;
	pvr_exp_sync_create_fence_data_t *data = user_data;
	struct sync_file *sync_file;
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct dma_fence *fence;
	int err;

	if (data == NULL) {
		pr_err(FILE_NAME ": %s: Unexpected NULL user_data\n", __func__);
		err = -EINVAL;
		goto err_out;
	}

	if (fd < 0) {
		pr_err(FILE_NAME ": %s: Failed to find unused fd (%d)\n",
		       __func__, fd);
		err = -EMFILE;
		goto err_out;
	}

	if (!timeline->is_export) {
		pr_err(FILE_NAME ": %s: Invalid timeline contents for fd (%d)\n",
		       __func__, fd);
		err = -EINVAL;
		goto err_put_fd;
	}

	fence = pvr_exp_fence_create(NULL);
	if (!fence) {
		pr_err(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
		       __func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	sync_file = sync_file_create(fence);
	dma_fence_put(fence);
	if (!sync_file) {
		pr_err(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
			__func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	data->fence = fd;

	fd_install(fd, sync_file->file);

	return 0;

err_put_fd:
	put_unused_fd(fd);
err_out:
	return err;
}

int pvr_sync_api_sw_create_fence(void *api_priv, void *user_data)
{
	struct pvr_sync_timeline *timeline = api_priv;
	struct pvr_sw_sync_create_fence_data *data = user_data;
	struct sync_file *sync_file;
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct dma_fence *fence;
	int err;

	if (fd < 0) {
		inno_error(FILE_NAME ": %s: Failed to find unused fd (%d)\n",
		       __func__, fd);
		err = -EMFILE;
		goto err_out;
	}

	fence = pvr_counting_fence_create(timeline->sw_fence_timeline, &data->sync_pt_idx);
	if (!fence) {
		inno_error(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
		       __func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	sync_file = sync_file_create(fence);
	dma_fence_put(fence);
	if (!sync_file) {
		inno_error(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
			__func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	data->fence = fd;

	fd_install(fd, sync_file->file);

	return 0;

err_put_fd:
	put_unused_fd(fd);
err_out:
	return err;
}

int pvr_sync_api_sw_create_fence_without_timeline(void *api_priv, void *user_data)
{
	struct pvr_sw_sync_create_fence_without_timeline_data *data = user_data;
	struct sync_file *sync_file;
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct dma_fence *fence;
	int err;

	if (fd < 0) {
		inno_error(FILE_NAME ": %s: Failed to find unused fd (%d)\n",
		       __func__, fd);
		err = -EMFILE;
		goto err_out;
	}

	fence = pvr_sw_fence_create_without_timeline();
	if (!fence) {
		inno_error(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
		       __func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	sync_file = sync_file_create(fence);
	if (!sync_file) {
		inno_error(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
			__func__, fd);
		err = -ENOMEM;
		goto err_free_fence;
	}
	dma_fence_put(fence);
	data->fence = fd;

	fd_install(fd, sync_file->file);

	return 0;

err_free_fence:
	dma_fence_put(fence);
err_put_fd:
	put_unused_fd(fd);
err_out:
	return err;
}

int pvr_sync_api_sw_signal_fence(void *api_priv, void *user_data)
{
	struct pvr_sw_sync_signal_fence_data *data = user_data;
	int fd = data->fence;
	struct dma_fence *fence;
	int err = 0;

	if (fd < 0) {
		inno_error(FILE_NAME ": %s: invalid  fd (%d)\n",
		       __func__, fd);
		err = -EINVAL;
		goto err_out;
	}

	fence = fh2m_inno_sync_file_get_fence(fd);
	if (fence) {
		dma_fence_signal(fence);
		dma_fence_put(fence);
	}
err_out:
	return err;
}

int pvr_sync_update_timestamp(int fd1, int fd2, int merge)
{
    struct dma_fence *src1, *src2, *output;
    src1 = sync_file_get_fence(fd1);
    src2 = sync_file_get_fence(fd2);
    output = sync_file_get_fence(merge);

    if (src1 == NULL || src2 == NULL || output == NULL)
	return 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	if ((dma_fence_get_status(output) > 0) &&
        (output->timestamp <= src1->timestamp ||
	 output->timestamp <= src2->timestamp)) {
        output->timestamp = src1->timestamp >= src2->timestamp ?
				    src1->timestamp : src2->timestamp;
    }
#else
	if ((fence_get_status(output) > 0) &&
        (output->timestamp.tv64 <= src1->timestamp.tv64 ||
	 output->timestamp.tv64 <= src2->timestamp.tv64)) {
        output->timestamp.tv64 = src1->timestamp.tv64 >= src2->timestamp.tv64 ?
				    src1->timestamp.tv64 : src2->timestamp.tv64;
    }
#endif

    dma_fence_put(src1);
    dma_fence_put(src2);
    dma_fence_put(output);
    return 0;
}
int pvr_sync_api_sw_inc(void *api_priv, void *user_data)
{
	struct pvr_sync_timeline *timeline = api_priv;
	struct pvr_sw_timeline_advance_data *data = user_data;
	bool res;

	res = pvr_counting_fence_timeline_inc(timeline->sw_fence_timeline, &data->sync_pt_idx);

	/* pvr_counting_fence_timeline_inc won't allow sw timeline to be
	 * advanced beyond the last defined point
	 */
	if (!res) {
		inno_error("pvr_sync_file: attempt to advance SW timeline beyond last defined point\n");
		return -EPERM;
	}

	return 0;
}

static void
pvr_sync_debug_request_heading(void *data, u32 verbosity,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	if (DD_VERB_LVL_ENABLED(verbosity, DEBUG_REQUEST_VERBOSITY_MEDIUM))
		PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf, pvDumpDebugFile,
				  "------[ Native Fence Sync: timelines ]------");
}

enum PVRSRV_ERROR_TAG pvr_sync_register_functions(void)
{
	/* Register the resolve fence and create fence functions with
	 * sync_checkpoint.c
	 * The pvr_fence context registers its own EventObject callback to
	 * update sync status
	 */
	/* Initialise struct and register with sync_checkpoint.c */
	pvr_sync_data.sync_checkpoint_ops.pfnFenceResolve = pvr_sync_resolve_fence;
	pvr_sync_data.sync_checkpoint_ops.pfnFenceCreate = pvr_sync_create_fence;
	pvr_sync_data.sync_checkpoint_ops.pfnFenceDataRollback = pvr_sync_rollback_fence_data;
	pvr_sync_data.sync_checkpoint_ops.pfnFenceFinalise = pvr_sync_finalise_fence;
#if defined(NO_HARDWARE)
	pvr_sync_data.sync_checkpoint_ops.pfnNoHWUpdateTimelines = pvr_sync_nohw_signal_fence;
#else
	pvr_sync_data.sync_checkpoint_ops.pfnNoHWUpdateTimelines = NULL;
#endif
	pvr_sync_data.sync_checkpoint_ops.pfnFreeCheckpointListMem =
		pvr_sync_free_checkpoint_list_mem;
	pvr_sync_data.sync_checkpoint_ops.pfnDumpInfoOnStalledUFOs =
		pvr_sync_dump_info_on_stalled_ufos;
	fh2m_inno_strlcpy(pvr_sync_data.sync_checkpoint_ops.pszImplName, "pvr_sync_file",
		SYNC_CHECKPOINT_IMPL_MAX_STRLEN);
#if defined(PDUMP)
	pvr_sync_data.sync_checkpoint_ops.pfnSyncFenceGetCheckpoints =
		pvr_sync_fence_get_checkpoints;
#endif

	return SyncCheckpointRegisterFunctions(&pvr_sync_data.sync_checkpoint_ops);
}

int pvr_sync_init(PVRSRV_DEVICE_NODE_HANDLE psDeviceNode)
{
	struct pvr_fence_context *foreign_fence_context =
			pvr_fence_foreign_context_create(
					psDeviceNode,
					NativeSyncGetFenceStatusWq(),
					"foreign_sync");

	if (!foreign_fence_context) {
		pr_err(FILE_NAME ": %s: Failed to create foreign sync context\n", __func__);
		return -ENOMEM;
	}

	list_pvr_fence_context_insert_tail(&(pvr_sync_data.foreign_fence_context_list), foreign_fence_context);

#if defined(NO_HARDWARE)
	INIT_LIST_HEAD(&pvr_timeline_active_list);
#endif

	return 0;
}

void pvr_sync_deinit(void *psDeviceNode)
{
	pvr_fence_context_destroy_by_node(pvr_sync_data.foreign_fence_context_list, (PVRSRV_DEVICE_NODE_HANDLE)psDeviceNode);
	pvr_fence_cleanup();
}

enum PVRSRV_ERROR_TAG pvr_sync_device_init(struct device *dev)
{
	struct drm_device *ddev = innogpu_get_drm_from_pdev(dev);
	struct pvr_drm_private *priv = innogpu_drm_to_pvr_private(ddev);
	enum PVRSRV_ERROR_TAG error;
	struct pvr_fence_context *ffctx;

	error = PVRSRVRegisterDeviceDbgRequestNotify(
				&priv->sync_debug_notify_handle,
				priv->dev_node,
				pvr_sync_debug_request_heading,
				DEBUG_REQUEST_LINUXFENCE,
				NULL);
	if (error != PVRSRV_OK) {
		inno_error("%s: failed to register debug request callback (%s)\n",
		       __func__, fh2m_PVRSRVGetErrorString(error));
		goto err_out;
	}

	for(ffctx = pvr_sync_data.foreign_fence_context_list; ffctx != NULL; ffctx = ffctx->psNext) {
		if(DeviceNodeHandleGetCoreId((PVRSRV_DEVICE_NODE_HANDLE)(priv->dev_node)) == DeviceNodeHandleGetCoreId(ffctx->pdev_node))
			break;
	}

	/* Register the foreign sync context debug notifier on each device */
	error = pvr_fence_context_register_dbg(
				&priv->sync_foreign_debug_notify_handle,
				priv->dev_node,
				ffctx);
	if (error != PVRSRV_OK) {
		inno_error("%s: failed to register fence debug request callback (%s)\n",
		       __func__, fh2m_PVRSRVGetErrorString(error));
		goto err_context_regdbg;
	}

#if defined(NO_HARDWARE)
	INIT_LIST_HEAD(&pvr_timeline_active_list);
#endif

	return PVRSRV_OK;

err_context_regdbg:
	PVRSRVUnregisterDeviceDbgRequestNotify(priv->sync_debug_notify_handle);
err_out:
	return error;
}

void pvr_sync_device_deinit(struct device *dev)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct pvr_drm_private *priv = innogpu_drm_to_pvr_private(ddev);

	PVRSRVUnregisterDeviceDbgRequestNotify(priv->sync_foreign_debug_notify_handle);
	PVRSRVUnregisterDeviceDbgRequestNotify(priv->sync_debug_notify_handle);
}

enum PVRSRV_ERROR_TAG pvr_sync_fence_wait(void *fence, u32 timeout_in_ms)
{
	long timeout = msecs_to_jiffies(timeout_in_ms);
	int err;

	err = dma_fence_wait_timeout(fence, true, timeout);
	/*
	 * dma_fence_wait_timeout returns:
	 * - the remaining timeout on success
	 * - 0 on timeout
	 * - -ERESTARTSYS if interrupted
	 */
	if (err > 0)
		return PVRSRV_OK;
	else if (err == 0)
		return PVRSRV_ERROR_TIMEOUT;

	return PVRSRV_ERROR_FAILED_DEPENDENCIES;
}

enum PVRSRV_ERROR_TAG pvr_sync_fence_release(void *fence)
{
	dma_fence_put(fence);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR_TAG pvr_sync_fence_get(int fence_fd, void **fence_out)
{
	struct dma_fence *fence;

	fence = sync_file_get_fence(fence_fd);
	if (fence == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	*fence_out = fence;

	return PVRSRV_OK;
}

enum PVRSRV_ERROR_TAG
pvr_sync_sw_timeline_fence_create(struct _PVRSRV_DEVICE_NODE_ *pvrsrv_dev_node,
				  int timeline_fd,
				  const char *fence_name,
				  int *fence_fd_out,
				  u64 *sync_pt_idx)
{
	enum PVRSRV_ERROR_TAG srv_err;
	struct pvr_sync_timeline *timeline;
	struct dma_fence *fence = NULL;
	struct sync_file *sync_file = NULL;
	int fd;

	(void)(pvrsrv_dev_node);

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return PVRSRV_ERROR_UNABLE_TO_ADD_HANDLE;

	timeline = pvr_sync_timeline_fget(timeline_fd);
	if (!timeline) {
		/* unrecognised timeline */
		srv_err = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto err_put_fd;
	}
	if (!timeline->is_sw) {
		pvr_sync_timeline_fput(timeline);
		srv_err = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_put_fd;
	}

	fence = pvr_counting_fence_create(timeline->sw_fence_timeline, sync_pt_idx);
	pvr_sync_timeline_fput(timeline);
	if (!fence) {
		srv_err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_fd;
	}

	sync_file = sync_file_create(fence);
	dma_fence_put(fence);
	if (!sync_file) {
		srv_err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_fd;
	}

	fd_install(fd, sync_file->file);

	*fence_fd_out = fd;

	return PVRSRV_OK;

err_put_fd:
	put_unused_fd(fd);
	return srv_err;
}

enum PVRSRV_ERROR_TAG pvr_sync_sw_timeline_advance(void *timeline, u64 *sync_pt_idx)
{
	if (timeline == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	pvr_counting_fence_timeline_inc(timeline, sync_pt_idx);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR_TAG pvr_sync_sw_timeline_release(void *timeline)
{
	if (timeline == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;

	pvr_counting_fence_timeline_put(timeline);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR_TAG pvr_sync_sw_timeline_get(int timeline_fd,
					   void **timeline_out)
{
	struct pvr_counting_fence_timeline *sw_timeline;
	struct pvr_sync_timeline *timeline;

	timeline = pvr_sync_timeline_fget(timeline_fd);
	if (!timeline)
		return PVRSRV_ERROR_INVALID_PARAMS;

	sw_timeline =
		pvr_counting_fence_timeline_get(timeline->sw_fence_timeline);
	pvr_sync_timeline_fput(timeline);
	if (!sw_timeline)
		return PVRSRV_ERROR_INVALID_PARAMS;

	*timeline_out = sw_timeline;

	return PVRSRV_OK;
}

static void _dump_sync_point(struct dma_fence *fence,
							  DUMPDEBUG_PRINTF_FUNC *dump_debug_printf,
							  void *dump_debug_file)
{
	const struct dma_fence_ops *fence_ops = fence->ops;
	bool signaled = dma_fence_is_signaled(fence);
	char time[16] = { '\0' };

	fence_ops->timeline_value_str(fence, time, sizeof(time));

	PVR_DUMPDEBUG_LOG(dump_debug_printf,
					  dump_debug_file,
					  "<%p> Seq#=%llu TS=%s State=%s TLN=%s",
					  fence,
					  (u64) fence->seqno,
					  time,
					  (signaled) ? "Signalled" : "Active",
					  fence_ops->get_timeline_name(fence));
}

static void _dump_fence(struct dma_fence *fence,
			DUMPDEBUG_PRINTF_FUNC *dump_debug_printf,
			void *dump_debug_file)
{
	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *fence_array = to_dma_fence_array(fence);
		int i;

		if (fence_array) {
			PVR_DUMPDEBUG_LOG(dump_debug_printf,
					  dump_debug_file,
					  "Fence: [%p] Sync Points:\n",
					  fence_array);

			for (i = 0; i < fence_array->num_fences; i++)
				_dump_sync_point(fence_array->fences[i],
						 dump_debug_printf,
						 dump_debug_file);
		}

	} else {
		_dump_sync_point(fence, dump_debug_printf, dump_debug_file);
	}
}

enum PVRSRV_ERROR_TAG
sync_dump_fence(void *sw_fence_obj,
		DUMPDEBUG_PRINTF_FUNC *dump_debug_printf,
		void *dump_debug_file)
{
	struct dma_fence *fence = (struct dma_fence *) sw_fence_obj;

	_dump_fence(fence, dump_debug_printf, dump_debug_file);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR_TAG
sync_sw_dump_timeline(void *sw_timeline_obj,
		      DUMPDEBUG_PRINTF_FUNC *dump_debug_printf,
		      void *dump_debug_file)
{
	pvr_counting_fence_timeline_dump_timeline(sw_timeline_obj,
						  dump_debug_printf,
						  dump_debug_file);

	return PVRSRV_OK;
}
