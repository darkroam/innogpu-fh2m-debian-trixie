/*************************************************************************/ /*!
@File			innodpu_print.c
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
#include <linux/compiler.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include "inno_drm_version.h"

#if (DRM_VERSION >= KERNEL_VERSION(5, 15, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

#include "innodpu_common.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"
#include "innoaudio_print.h"
#include "inno_plat_dev.h"
#include "inno_misc.h"

extern bool s_audio_debug;

void innoaudio_info(inno_dev *dev,const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (!s_audio_debug)
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	fh2m_inno_printk(KERN_INFO"[audio:%ps] %pV", __builtin_return_address(0), &vaf);

	va_end(args);
}

void innoaudio_warn(inno_dev *dev, const char *format, ...)
{
	return;
}

void innoaudio_err(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	fh2m_inno_printk(KERN_ERR"[audio:%ps] <ERROR> %pV", __builtin_return_address(0), &vaf);

	va_end(args);
}
