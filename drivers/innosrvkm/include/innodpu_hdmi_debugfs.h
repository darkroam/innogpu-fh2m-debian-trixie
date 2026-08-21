/*************************************************************************/ /*!
@File			innodpu_hdmi_debugfs.h
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
#ifndef __INNODPU_HDMI_DEBUGFS_H
#define __INNODPU_HDMI_DEBUGFS_H

#include "img_defs.h"

enum {
	HDMI_DEBUGS_STATUS_UNINITED,
	HDMI_DEBUGS_STATUS_INITED,
	HDMI_DEBUGS_STATUS_FAILED,
};


enum  {
	G0_HDMI_DEBUGFS_TYPE_CONTROL = 0,
	G0_HDMI_DEBUGFS_TYPE_INTERRUPT,
	G0_HDMI_DEBUGFS_TYPE_MAX,
};


struct hdmi_entity_map_t {
	char    *name;
	uint32_t entity;
};

struct hdmi_debugfs_item_t {
	struct hdmi_chip_t *chip;
	struct list_head list;

	struct dentry *ent;
	int    status;

	void   *data;
};

#if defined(CONFIG_DEBUG_FS)
extern int  inno_hdmi_custom_debugfs_create(struct dentry *entry, struct hdmi_chip_t *chip);
extern void inno_hdmi_custom_debugfs_remove(struct dentry *entry, struct hdmi_chip_t *chip);
#endif
#endif//__INNODPU_HDMI_H

