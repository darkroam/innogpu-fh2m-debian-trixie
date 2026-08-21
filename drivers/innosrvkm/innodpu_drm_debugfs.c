/*************************************************************************/ /*!
@File			innodpu_drm_debugfs.c
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
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
*/ /**************************************************************************/
#include "innodpu_common.h"
#include "innodpu_drm_debugfs.h"
#include "innodpu_drm_drv.h"
#include "pdp0_drv.h"
#include "pdp_drm.h"
#include "pdp0_hw.h"
#include "innogpu_drm.h"
#include "inno_debug.h"

#define PDP_DEBUGFS_DISPLAY_ENABLED "display_enabled"

static int __maybe_unused innodpu_vram_one_info(int id, void *ptr, void *data)
{
	struct seq_file *m = data;
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	char *gem_type = NULL;
	innodpu_gem_object *innodpu_obj = to_innodpu_obj(obj);
	phys_addr_t cpu_paddr = 0;
	dma_addr_t dev_paddr = 0;


	if (innodpu_obj->mem_manager->pos == SYS_GTT_POSITION) {
		gem_type = "gtt";
	} else if (innodpu_obj->mem_manager->visible) {
		if (innodpu_obj->class == CONTINUOUS_VRAM) {
			gem_type = "visible_continuous";
			cpu_paddr = innodpu_obj->cpu_paddr;
			dev_paddr = innodpu_obj->dev_paddr;
		} else {
			gem_type = "visible_nocontinuous";
			if (innodpu_obj->pmr->base_array_size == 1) {
				cpu_paddr = fh2m_dev_paddr_to_cpu_paddr(obj->dev->dev, innodpu_obj->pmr->base_array[0]);
				dev_paddr = innodpu_obj->pmr->base_array[0];
			}
		}
	} else {
		if (innodpu_obj->class == CONTINUOUS_VRAM) {
			gem_type = "invisible_continuous";
			dev_paddr = innodpu_obj->dev_paddr;
		} else {
			gem_type = "invisible_nocontinuous";
			if (innodpu_obj->pmr->base_array_size == 1) {
				dev_paddr = innodpu_obj->pmr->base_array[0];
			}
		}
	}

	seq_printf(m, "(%-3d)%-22s  %8zd     %7d %8d       %#.16llx %#.16llx (%s)\n",
		obj->name, innodpu_obj->name, obj->size, obj->handle_count, kref_read(&obj->refcount),
		cpu_paddr, dev_paddr, gem_type);

	return 0;
}

typedef enum gram_type_t {
	INNODPU_GRAM_VISIBLE = 0,
	INNODPU_GRAM_INVISIBLE,
	INNODPU_GRAM_SHARED,
} gram_type_e;

struct innodpu_gem_user_info {
	unsigned int visible_gram_size;
	unsigned int invisible_gram_size;
	unsigned int shared_gram_size;
	char name[64];
	struct list_head list;
};

struct innodpu_gram_gem_block {
	uint64_t gram_addr;
	unsigned int size;
	char name[60];
	struct list_head list;
};

static void innodpu_alloc_gram_block(innodpu_gem_object *innodpu_obj, struct list_head *block_list)
{
	struct innodpu_gram_gem_block *block = NULL;
	struct innodpu_gram_gem_block *cur = NULL, *temp = NULL;

	block = (struct innodpu_gram_gem_block *)fh2m_inno_vzalloc(sizeof(struct innodpu_gram_gem_block));
	if (block == NULL) {
		goto exit;
	}

	fh2m_inno_snprintf(block->name, 64, "%s", innodpu_obj->name);
	block->size = innodpu_obj->base.size;
	block->gram_addr = innodpu_obj->dev_paddr;
	INIT_LIST_HEAD(&block->list);

	/* add node to list and sort by gram_addr */
	if (list_empty(block_list)) {
		list_add_tail(&block->list, block_list);
		goto exit;
	}

	list_for_each_entry_safe(cur, temp, block_list, list) {
		if (cur->gram_addr > block->gram_addr) {
			block->list.next = &cur->list;
			block->list.prev = cur->list.prev;

			cur->list.prev->next = &block->list;
			cur->list.prev = &block->list;

			goto exit;
		}
	}
	list_add_tail(&block->list, block_list);

exit:
	return;
}

static void innodpu_gram_gem_block_show_and_free(innodpu_shared_mem *pshared_mem,
	struct list_head *block_list, struct seq_file *m)
{
	struct innodpu_gram_gem_block *cur = NULL, *temp = NULL;
	unsigned int used_size = 0, fragment_size = 0;
	uint64_t prev_block_addr = pshared_mem->dev_paddr;
	unsigned int prev_block_size = 0;
	int count = 0, fragment_count = 0;

	seq_printf(m, "\ndpu share mem view (fragment mean gram hole)\n%-30s %-18s %-10s %-6s\n", "name", "addr", "size(MB)", "size(KB)");

	list_for_each_entry_safe(cur, temp, block_list, list) {
		count++;

		if (count == 1 && (cur->gram_addr != pshared_mem->dev_paddr)) {
			seq_printf(m, "%-30s 0x%-16llx %-10d %-6d\n", "fragment",
				(uint64_t)pshared_mem->dev_paddr,
				(unsigned int)((cur->gram_addr - pshared_mem->dev_paddr) / 1024 / 1024),
				(unsigned int)((cur->gram_addr - pshared_mem->dev_paddr) / 1024));
			fragment_size += (unsigned int)(cur->gram_addr - pshared_mem->dev_paddr);
			fragment_count++;
		} else {
			if ((prev_block_addr + prev_block_size) < cur->gram_addr) {
				seq_printf(m, "%-30s 0x%-16llx %-10d %-6d\n", "fragment",
					prev_block_addr + prev_block_size,
					(unsigned int )((cur->gram_addr - prev_block_addr - prev_block_size) / 1024 / 1024),
					(unsigned int )((cur->gram_addr - prev_block_addr - prev_block_size) / 1024));
				fragment_size += (unsigned int )(cur->gram_addr - prev_block_addr - prev_block_size);
				fragment_count++;
			}
		}

		seq_printf(m, "%-30s 0x%-16llx %-10d %-6d\n", cur->name, cur->gram_addr, cur->size / 1024 / 1024, cur->size / 1024);

		prev_block_addr = cur->gram_addr;
		prev_block_size = cur->size;

		used_size += cur->size;

		list_del(&cur->list);
		fh2m_inno_vfree(cur);
	}

	seq_printf(m, "sharemem actual useSize=%dKB=%dMB fragmentSize=%dKB=%dMB"
		" blockCount=%d fragment_count=%d\n\n",
		used_size / 1024, used_size / 1024 / 1024,
		fragment_size / 1024, fragment_size / 1024 / 1024,
		count, fragment_count);

	return;
}

static void innodpu_update_gram_size_info(struct innodpu_gem_user_info *user_info,
	gram_type_e type, unsigned int size)
{
	switch (type) {
	case INNODPU_GRAM_VISIBLE:
		user_info->visible_gram_size += size;
		break;
	case INNODPU_GRAM_INVISIBLE:
		user_info->invisible_gram_size += size;
		break;
	case INNODPU_GRAM_SHARED:
		user_info->shared_gram_size += size;
		break;
	default:
		user_info->visible_gram_size += size;
		break;
	}

	return;
}

static struct innodpu_gem_user_info *innodpu_alloc_gem_user_info(innodpu_gem_object *innodpu_obj,
	struct list_head *user_list)
{
	struct innodpu_gem_user_info *info = NULL;

	info = (struct innodpu_gem_user_info *)fh2m_inno_vzalloc(sizeof(struct innodpu_gem_user_info));
	if (info == NULL) {
		goto exit;
	}

	fh2m_inno_snprintf(info->name, 64, "%s", innodpu_obj->name);
	INIT_LIST_HEAD(&info->list);
	list_add_tail(&info->list, user_list);

exit:
	return info;
}

static void innodpu_free_and_show_gem_user_info(struct list_head *user_list, struct seq_file *m)
{
	struct innodpu_gem_user_info *user_info = NULL, *temp_user_info = NULL;

	if (m != NULL) {
		seq_printf(m, "%-30s %-25s %-25s %-25s\n",
			"process", "visibleSize", "invisibleSize", "sharedSize");

		seq_printf(m, "%-30s %-6s %-18s %-6s %-18s %-6s %-18s\n",
			" ", "MB", "KB", "MB", "KB", "MB", "KB");
	}

	list_for_each_entry_safe(user_info, temp_user_info, user_list, list) {
		list_del(&user_info->list);
		if (m != NULL) {
			seq_printf(m, "%-30s %-6d %-18d %-6d %-18d %-6d %-18d\n",
				user_info->name,
				(user_info->visible_gram_size / 1024 / 1024), (user_info->visible_gram_size / 1024),
				(user_info->invisible_gram_size / 1024 / 1024), (user_info->invisible_gram_size / 1024),
				(user_info->shared_gram_size / 1024 / 1024), (user_info->shared_gram_size / 1024));
		}
		fh2m_inno_vfree(user_info);
	}

	return;
}

static bool innodpu_update_gem_info(innodpu_gem_object *innodpu_obj,
	struct list_head *user_list, gram_type_e type)
{
	struct innodpu_gem_user_info *user_info = NULL, *temp_user_info = NULL;

	list_for_each_entry_safe(user_info, temp_user_info, user_list, list) {
		if (fh2m_inno_strncmp(user_info->name, innodpu_obj->name, sizeof(innodpu_obj->name)) == 0) {
			innodpu_update_gram_size_info(user_info, type, innodpu_obj->base.size);
			return true;
		}
	}

	user_info = innodpu_alloc_gem_user_info(innodpu_obj, user_list);
	if (user_info != NULL) {
		innodpu_update_gram_size_info(user_info, type, innodpu_obj->base.size);
	} else {
		return false;
	}

	return true;
}

static int innodpu_vram_view(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct innodpu_drm_private *dev_priv = NULL;
	struct list_head user_list;
	struct list_head shared_mem_block_list;

	innodpu_mem_manager *visible_mem_manager = NULL;
	innodpu_mem_manager *invisible_mem_manager = NULL;
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;

	dev_priv = innogpu_drm_to_display_private(dev);
	if (!dev_priv) {
		fh2m_innodpu_err(dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	visible_mem_manager = dev_priv->visible_mem_manager;
	invisible_mem_manager = dev_priv->invisible_mem_manager;

	INIT_LIST_HEAD(&user_list);
	INIT_LIST_HEAD(&shared_mem_block_list);

	/* dpu visible memory */
	mutex_lock(&visible_mem_manager->mem_lock);
	list_for_each_entry_safe(innodpu_obj, tmp, &visible_mem_manager->mem_list, mem_node) {
		innodpu_update_gem_info(innodpu_obj, &user_list, INNODPU_GRAM_VISIBLE);
	}
	mutex_unlock(&visible_mem_manager->mem_lock);

	/* dpu invisible memory */
	if (invisible_mem_manager) {
		mutex_lock(&invisible_mem_manager->mem_lock);
		list_for_each_entry_safe(innodpu_obj, tmp, &invisible_mem_manager->mem_list, mem_node) {
			innodpu_update_gem_info(innodpu_obj, &user_list, INNODPU_GRAM_INVISIBLE);
		}
		mutex_unlock(&invisible_mem_manager->mem_lock);
	}

	/* dpu shared memory */
	if (dev_priv->has_shared_mem) {
		spin_lock(&dev_priv->shared_vram_info.user_idr_lock);

		if (dev_priv->shared_vram_info.current_user) {
			const struct drm_mm_node *entry = NULL;
			innodpu_mem_manager *shared_mem_manager =
				dev_priv->shared_vram_info.current_user->mem_manager;

			mutex_lock(&visible_mem_manager->mem_lock);
			drm_mm_for_each_node(entry, &shared_mem_manager->gem_mm) {
				innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
				innodpu_update_gem_info(innodpu_obj, &user_list, INNODPU_GRAM_SHARED);
				innodpu_alloc_gram_block(innodpu_obj, &shared_mem_block_list);
			}
			mutex_unlock(&visible_mem_manager->mem_lock);
		}

		spin_unlock(&dev_priv->shared_vram_info.user_idr_lock);
	}

	innodpu_free_and_show_gem_user_info(&user_list, m);
	if (dev_priv->has_shared_mem) {
		innodpu_gram_gem_block_show_and_free(&dev_priv->shared_vram_info, &shared_mem_block_list, m);
	}

	return 0;
}

static int innodpu_vram_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct innodpu_drm_private *dev_priv = NULL;
	struct drm_dpu_vram_count *vram = NULL;
	innodpu_mem_manager *visible_mem_manager = NULL;
	innodpu_mem_manager *invisible_mem_manager = NULL;
	innodpu_mem_manager *gtt_mem_manager = NULL;
	innodpu_gem_object *innodpu_obj = NULL, *tmp = NULL;
	unsigned long long used = 0;

	dev_priv = innogpu_drm_to_display_private(dev);
	if (!dev_priv) {
		fh2m_innodpu_err(dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	visible_mem_manager = dev_priv->visible_mem_manager;
	invisible_mem_manager = dev_priv->invisible_mem_manager;
	gtt_mem_manager = dev_priv->gtt_mem_manager;

	vram = kzalloc(sizeof(struct drm_dpu_vram_count), fh2m_hal_get_inno_gfp_kernel());
	if (vram == NULL) {
		return -1;
	}

	if (!dev_priv->invisible_mem_manager) {
		vram->invisiable_vram_size = 0;
		vram->invisiable_vram_usage = 0;
	} else {
		vram->invisiable_vram_size = invisible_mem_manager->size;
		vram->invisiable_vram_usage = 0;
	}


	seq_printf(m, "   name	                       "
			"size(byte) handlescount refcount      "
			"cpu_paddr           dev_paddr\n");
	mutex_lock(&dev->object_name_lock);
	idr_for_each(&dev->object_name_idr, innodpu_vram_one_info, m);
	idr_for_each(&dev->object_name_idr, innodpu_gem_vram_count, vram);
	mutex_unlock(&dev->object_name_lock);

	seq_printf(m, "\n(  visible)total_size:%#16llx  usage:%#16llx\n",
			vram->visiable_vram_size, vram->visiable_vram_usage);
	seq_printf(m, "(invisible)total_size:%#16llx  usage:%#16llx\n\n\n",
			vram->invisiable_vram_size, vram->invisiable_vram_usage);

	list_for_each_entry_safe(innodpu_obj, tmp, &visible_mem_manager->mem_list, mem_node) {
		used += innodpu_obj->base.size;
		seq_printf(m, "(%d)%-40s,[0x%-10llx + 0x%-10lx]\n",
			innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->dev_paddr, innodpu_obj->base.size);
	}
	seq_printf(m, "(  visible)total_size:%#lx  usage:%#16llx\n\n", visible_mem_manager->size, used);

	used = 0;
	if (dev_priv->invisible_mem_manager) {
		list_for_each_entry_safe(innodpu_obj, tmp, &invisible_mem_manager->mem_list, mem_node) {
			used += innodpu_obj->base.size;
			seq_printf(m, "(%d)%-40s,[0x%-10llx + 0x%-10lx]\n",
				innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->dev_paddr, innodpu_obj->base.size);
		}
		seq_printf(m, "(invisible)total_size:%#lx  usage:%#16llx\n\n",
				invisible_mem_manager->size, used);
	}

	used = 0;
	if (dev_priv->has_gtt_mem && dev_priv->gtt_mem_manager) {
		list_for_each_entry_safe(innodpu_obj, tmp, &gtt_mem_manager->mem_list, mem_node) {
			used += innodpu_obj->base.size;
			seq_printf(m, "(%d)%-40s,[0x%-10llx + 0x%-10lx]\n",
				innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->dev_paddr, innodpu_obj->base.size);
		}
		seq_printf(m, "(    gttmem)total_size:%#lx  usage:%#16llx\n\n",
				gtt_mem_manager->size, used);
	}


	/* multi-user infomation */
	{
		innodpu_shared_mem *pshared_mem = &dev_priv->shared_vram_info;
		innodpu_shared_mem_user *puser = NULL;
		int user_id = 0;

		seq_printf(m, "(share memory)dev_paddr:%#llx  cpu_paddr:%#llx, size:%#lx, share_is_visible-%d\n",
				pshared_mem->dev_paddr, pshared_mem->cpu_paddr,
				pshared_mem->size, pshared_mem->is_visible);

		// dump user infomation
		spin_lock(&pshared_mem->user_idr_lock);
		idr_for_each_entry(&pshared_mem->uer_idr, puser, user_id) {
			const struct drm_mm_node *entry = NULL;
			innodpu_gem_object *innodpu_obj = NULL;

			used = 0;
			if ((pshared_mem->current_user) && (pshared_mem->current_user == puser))
				seq_printf(m, "\tdump user-%p, user_id-%d(*)\n", puser, user_id);
			else
				seq_printf(m, "\tdump user-%p, user_id-%d\n", puser, user_id);
			drm_mm_for_each_node(entry, &puser->mem_manager->gem_mm) {
				innodpu_obj = container_of(entry, innodpu_gem_object, mm_node);
				used += innodpu_obj->base.size;
				seq_printf(m, "\t\t(%d)%-40s,[0x%-10llx + 0x%-10lx]\n",
						innodpu_obj->base.name, innodpu_obj->name, innodpu_obj->dev_paddr, innodpu_obj->base.size);
			}
			seq_printf(m, "(shared)total_size:%#lx  usage:%#16llx\n\n",
					pshared_mem->size, used);
		}
		spin_unlock(&pshared_mem->user_idr_lock);
	}

	kfree(vram);

	return 0;
}

static __maybe_unused int get_pdp0_info(struct seq_file *m, struct innodpu_pdp0_hw_device *hw_dev,
						int (*pdp0_info)(struct innodpu_pdp0_hw_device *hw_dev,
						u64 buf[]), u64 base_info[])
{
	if (pdp0_info(hw_dev, base_info)) {
		seq_printf(m, "layer disabled.\n");
		return -1;
	}

	return 0;
}

static int innodpu_pdp_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *drm_dev = node->minor->dev;
	struct innodpu_drm_private *dev_priv = NULL;

	dev_priv = innogpu_drm_to_display_private(drm_dev);
	if (!dev_priv) {
		inno_error("[%s %d]dev priv is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	fh2m_hal_module_loadtime_get(drm_dev->dev, m);
#if 0
	struct drm_crtc *crtc = NULL;
	struct innodpu_pdp0_drm *pdp0_drm;
	struct innodpu_pdp0_hw_device *hwdev;
	int dpu_id;
	u64 base_info[6] = {0};
	u64 dev_paddr = 0;
	u64 cpu_paddr = 0;
	u32 stride = 0;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		dpu_id = innodpu_get_dpuid_bycrtc(crtc);
		pdp0_drm = crtc_to_pdp0_device(crtc);
		hwdev = pdp0_drm->hwdev;

		if (!crtc->state->enable) {
			seq_printf(m, "PDP %d NULL\n", dpu_id);
		} else {
			seq_printf(m, "PDP %d", dpu_id);
			seq_printf(m, "\t%dx%d", crtc->mode.hdisplay, crtc->mode.vdisplay);
			if (get_pdp0_info(m, hwdev, hwdev->base_info, base_info)) {
				seq_printf(m, "get pdp info incorrectly.\n");
				return -1;
			}
			dev_paddr = base_info[0];
			cpu_paddr = base_info[1];
			stride = INNO_LOWER_32_BITS(base_info[4]);
			seq_printf(m, "	cpu_paddr: %#llx", cpu_paddr);
			seq_printf(m, "	dev_paddr: %#llx", dev_paddr);
			seq_printf(m, "	stride: %d(bytes)", stride);
			seq_printf(m, "\n");

			if (!hwdev->qos_info(hwdev))
				seq_printf(m, "qos: enable\n");
			else
				seq_printf(m, "qos: disable\n");

			if (!hwdev->bg_info(hwdev))
				seq_printf(m, "background: black\n");
			else
				seq_printf(m, "background: unknow\n");

			if (hwdev->pdp0_ow1_control_info(hwdev))
				seq_printf(m, "control register: %#.8x\n", hwdev->pdp0_ow1_control_info(hwdev));
		}
	}
#endif
	return 0;
}

static const struct drm_info_list s_innodpu_debugfs_list[] = {
	{"geminfo", innodpu_vram_info, DRIVER_GEM},
	{"gemview", innodpu_vram_view, DRIVER_GEM},
	{"pdp_status", innodpu_pdp_info, DRIVER_MODESET},
};

static int inno_debugfs_create(struct drm_minor *minor, const char *name,
							   umode_t mode, const struct file_operations *fops)
{
	struct drm_info_node *node = NULL;

	/*
	 * We can't get access to our driver private data when this function is
	 * called so we fake up a node so that we can clean up entries later on.
	 */
	node = kzalloc(sizeof(struct drm_info_node), fh2m_hal_get_inno_gfp_kernel());
	if (!node) {
		return -ENOMEM;
	}
	node->dent = debugfs_create_file(name, mode, minor->debugfs_root, minor->dev, fops);
	if (!node->dent) {
		kfree(node);
		return -ENOMEM;
	}

	node->minor = minor;
	node->info_ent = (void *)fops;

#if (DRM_VERSION < KERNEL_VERSION(6, 7, 0)) && defined(INNOGPU_DEBUGFS_PRESENT)
	mutex_lock(&minor->debugfs_lock);
	list_add(&node->list, &minor->debugfs_list);
	mutex_unlock(&minor->debugfs_lock);
#endif

	return 0;
}

#if (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
int innodpu_debugfs_init(struct drm_minor *minor)
#else
void innodpu_debugfs_init(struct drm_minor *minor)
#endif
{
	int __attribute__ ((unused)) err;

	err = inno_debugfs_create(minor, "dpu_log",
							  0100644, &fh2m_s_inno_dpu_log_fops);
	if (err) {
		inno_drm_err(NULL, "failed to create '%s' debugfs entry\n", "dpu_log");
	}

	// current minor is struct drm_device ddev.primary
	drm_debugfs_create_files(s_innodpu_debugfs_list, INNO_ARRAY_SIZE(s_innodpu_debugfs_list),
							 minor->debugfs_root, minor);
#if (DRM_VERSION < KERNEL_VERSION(5, 8, 0))
	return 0;
#else
	return;
#endif
}

#if (DRM_VERSION < KERNEL_VERSION(4, 12, 0))
static int innodpu_debugfs_destroy(struct drm_minor *minor, const struct file_operations *fops)
{
	struct list_head *pos, *q;
	struct drm_info_node *tmp;

	mutex_lock(&minor->debugfs_lock);

	list_for_each_safe(pos, q, &minor->debugfs_list) {
		tmp = list_entry(pos, struct drm_info_node, list);
		if (tmp->info_ent == (void*)fops) {
			debugfs_remove(tmp->dent);
			list_del(pos);
			kfree(tmp);
		}
	}

	mutex_unlock(&minor->debugfs_lock);

	return 0;
}

void innodpu_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files((struct drm_info_list *)s_innodpu_debugfs_list,
		INNO_ARRAY_SIZE(s_innodpu_debugfs_list), minor);

	innodpu_debugfs_destroy(minor, &fh2m_s_inno_dpu_log_fops);

	return;
}
#endif
