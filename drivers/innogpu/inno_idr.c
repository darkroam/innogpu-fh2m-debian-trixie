#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include "inno_debug.h"
#include "inno_misc.h"
#include "inno_idr.h"

inno_idr *fh2m_inno_idr_kalloc(void)
{
	struct idr *innoidr = (struct idr *)kzalloc(sizeof(struct idr), GFP_KERNEL);
	if (!innoidr) {
		inno_error("[%s: %d] kzalloc %d bytes failed\n", __func__, __LINE__, sizeof(struct idr));
		return NULL;
	}

	idr_init(innoidr);

	return innoidr;
}
INNO_EXT_SYM(fh2m_inno_idr_kalloc);

void fh2m_inno_idr_init(inno_idr *innoidr)
{
	idr_init((struct idr *)innoidr);
}
INNO_EXT_SYM(fh2m_inno_idr_init);

void fh2m_inno_idr_free(inno_idr *innoidr)
{
	kfree((struct idr*)innoidr);
}
INNO_EXT_SYM(fh2m_inno_idr_free);

int fh2m_inno_idr_alloc(inno_idr *innoidr, void *ptr, int start, int end, int *id)
{
	int result;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	idr_preload(GFP_KERNEL);
	*id=idr_alloc((struct idr*)innoidr, ptr, start, end + 1,0);
	idr_preload_end();

	result = *id;
#else
	do
	{
		if(idr_pre_get((struct idr*)innoidr, GFP_KERNEL)==0)
		{
			return -1;
		}
		result = idr_get_new_above((struct idr*)innoidr, ptr, start, *id);
	}while(result == -EAGAIN);

	if ((IMG_UINT32)*id > end)
	{
		idr_remove((struct idr*)innoidr, *id);
		result = -EAGAIN;
	}
#endif

	return result;
}
INNO_EXT_SYM(fh2m_inno_idr_alloc);

int fh2m_inno_idr_alloc_cyclic(inno_idr *innoidr, void *entry, int start, int end)
{
	return idr_alloc_cyclic((struct idr*)innoidr, entry, start, end, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_idr_alloc_cyclic);

void * fh2m_inno_idr_get_next(inno_idr *innoidr, int *nextid)
{
	return idr_get_next((struct idr*)innoidr, nextid);
}
INNO_EXT_SYM(fh2m_inno_idr_get_next);

void * fh2m_inno_idr_find(inno_idr *innoidr, int id)
{
	return idr_find((struct idr*)innoidr, id);
}
INNO_EXT_SYM(fh2m_inno_idr_find);

bool fh2m_inno_idr_is_empty(inno_idr *innoidr)
{
	return idr_is_empty((struct idr*)innoidr);
}
INNO_EXT_SYM(fh2m_inno_idr_is_empty);

void * fh2m_inno_idr_remove(inno_idr *innoidr, int id)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	idr_remove((struct idr*)innoidr, id);
	return NULL;
#else
	return idr_remove((struct idr*)innoidr, id);
#endif
}
INNO_EXT_SYM(fh2m_inno_idr_remove);

void fh2m_inno_idr_destroy(inno_idr *innoidr)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
	idr_remove_all((struct idr*)innoidr);
#endif
	idr_destroy((struct idr*)innoidr);
}
INNO_EXT_SYM(fh2m_inno_idr_destroy);

int fh2m_inno_idr_foreach(inno_idr *innoidr,
				int (*function)(int id, void *p, void *data), void *data)
{
	return idr_for_each((struct idr*)innoidr, function, data);
}
INNO_EXT_SYM(fh2m_inno_idr_foreach);

void *fh2m_inno_idr_replace(inno_idr *innoidr, void *pvData, int id)
{
	return idr_replace((struct idr*)innoidr, pvData, id);
}
INNO_EXT_SYM(fh2m_inno_idr_replace);
