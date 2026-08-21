/*
 * * Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
 * * Dual MIT/GPLv2
 * *
 * * The contents of this file are subject to the MIT license as set out below.
 * *
 * * Permission is hereby granted, free of charge, to any person obtaining a copy
 * * of this software and associated documentation files (the "Software"), to deal
 * * in the Software without restriction, including without limitation the rights
 * * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * * copies of the Software, and to permit persons to whom the Software is
 * * furnished to do so, subject to the following conditions:
 * *
 * * The above copyright notice and this permission notice shall be included in
 * * all copies or substantial portions of the Software.
 * *
 * * Alternatively, the contents of this file may be used under the terms of
 * * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * * of GPL are applicable instead of those above.
 * *
 * * If you wish to allow use of your version of this file only under the terms of
 * * GPL, and not to allow others to use your version of this file under the terms
 * * of the MIT license, indicate your decision by deleting the provisions above
 * * and replace them with the notice and other provisions required by GPL as set
 * * out in the file called "GPL-COPYING" included in this distribution. If you do
 * * not delete the provisions above, a recipient may use your version of this file
 * * under the terms of either the MIT license or GPL.
 * *
 * * This License is also included in this distribution in the file called
 * * "MIT-COPYING".
 * *
 * * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * */
#include <linux/uuid.h>
#include "inno_uuid.h"
#include "inno_mm.h"
#include "inno_misc.h"

bool fh2m_inno_uuid_equal(const unsigned char *u1, const unsigned char *u2)
{
	return fh2m_inno_memcmp(u1, u2, INNO_UUID_SIZE) == 0;
}
INNO_EXT_SYM(fh2m_inno_uuid_equal);

void fh2m_inno_uuid_copy(unsigned char *dst, const unsigned char *src)
{
	fh2m_inno_memcpy(dst, src, INNO_UUID_SIZE);
}
INNO_EXT_SYM(fh2m_inno_uuid_copy);

bool fh2m_inno_uuid_is_null(const unsigned char *uuid)
{
	const static unsigned char inno_uuid_null[INNO_UUID_SIZE] = { 0 };
	return fh2m_inno_uuid_equal(uuid, inno_uuid_null);
}
INNO_EXT_SYM(fh2m_inno_uuid_is_null);

void fh2m_inno_generate_random_uuid(unsigned char uuid[16]) {
    generate_random_uuid(uuid);
}
INNO_EXT_SYM(fh2m_inno_generate_random_uuid);

bool fh2m_inno_uuid_is_valid(const char *uuid) {
    return uuid_is_valid(uuid);
}
INNO_EXT_SYM(fh2m_inno_uuid_is_valid);

