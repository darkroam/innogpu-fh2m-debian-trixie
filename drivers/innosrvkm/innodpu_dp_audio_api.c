
/*************************************************************************/ /*!
@File			innodpu_dp_audio_api.c
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

#include <linux/platform_device.h>
#include "innodpu_dp.h"
#include "innodpu_dp_audio_api.h"

int innodpu_dp_audio_is_support(struct platform_device* pdev)
{
	int ret;
	struct dp_device_t *inno_dp = fh2m_inno_platform_get_drvdata(pdev);

	if (!inno_dp)
		 return -EINVAL;

	ret = dp_get_audio_status(&inno_dp->chip);

	return ret;
}

int innodpu_dp_audio_query_display_connected(struct platform_device* pdev)
{
	int status;
	struct dp_device_t *inno_dp = fh2m_inno_platform_get_drvdata(pdev);

	if (!inno_dp)
		 return -EINVAL;

	status = READ_ONCE(inno_dp->connector.status);

	switch (status) {
	case connector_status_unknown:
	case connector_status_disconnected:
		return 0;
	case connector_status_connected:
		return 1;
	default:
		return -EIO;
	}
}

int innodpu_dp_audio_query_display_status(struct platform_device* pdev)
{
	int  dpms;
	bool enabled;

	struct dp_device_t *inno_dp = fh2m_inno_platform_get_drvdata(pdev);

	if (!inno_dp)
		 return -EINVAL;

	dpms = READ_ONCE(inno_dp->connector.dpms);
	enabled = READ_ONCE(inno_dp->connector.encoder);

	if (enabled && (dpms == DRM_MODE_DPMS_ON))
		return 1;

	return 0;
}

int innodpu_dp_audio_get_eld(struct platform_device* pdev, char *buf, int buf_size)
{
	int size;
	struct dp_device_t *inno_dp = fh2m_inno_platform_get_drvdata(pdev);

	if (!inno_dp)
		 return -EINVAL;

#define MAX_ELD_BYTES	128
	size = (buf_size < MAX_ELD_BYTES) ? buf_size : MAX_ELD_BYTES;
	fh2m_inno_memcpy(buf, &inno_dp->connector.eld, size);

	return size;
}
