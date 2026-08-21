/*
* Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
* Dual MIT/GPLv2
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

#include <linux/interrupt.h>
#include <linux/version.h>
#include "inno_misc.h"
#include "inno_interrupt.h"

inno_irqreturn_t fh2m_INNO_IRQ_HANDLED(void)
{
	return (inno_irqreturn_t)IRQ_HANDLED;
}
INNO_EXT_SYM(fh2m_INNO_IRQ_HANDLED);

inno_irqreturn_t fh2m_INNO_IRQ_NONE(void)
{
	return (inno_irqreturn_t)IRQ_NONE;
}
INNO_EXT_SYM(fh2m_INNO_IRQ_NONE);

inno_irqreturn_t fh2m_INNO_IRQ_WAKE_THREAD(void)
{
	return (inno_irqreturn_t)IRQ_WAKE_THREAD;
}
INNO_EXT_SYM(fh2m_INNO_IRQ_WAKE_THREAD);

unsigned long fh2m_INNO_IRQF_TRIGGER_LOW(void)
{
	return (unsigned long)IRQF_TRIGGER_LOW;
}
INNO_EXT_SYM(fh2m_INNO_IRQF_TRIGGER_LOW);

unsigned long fh2m_INNO_IRQF_TRIGGER_HIGH(void)
{
	return (unsigned long)IRQF_TRIGGER_HIGH;
}
INNO_EXT_SYM(fh2m_INNO_IRQF_TRIGGER_HIGH);

unsigned long fh2m_INNO_IRQF_SHARED(void)
{
	return (unsigned long)IRQF_SHARED;
}
INNO_EXT_SYM(fh2m_INNO_IRQF_SHARED);

int fh2m_inno_request_irq(unsigned int irq, inno_irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{
	return request_irq(irq, (irq_handler_t)handler, flags, name, dev);
}
INNO_EXT_SYM(fh2m_inno_request_irq);

const void *fh2m_inno_free_irq(unsigned int irq, void *dev_id)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	free_irq(irq, dev_id);
	return NULL;
#else
	return free_irq(irq, dev_id);
#endif
}
INNO_EXT_SYM(fh2m_inno_free_irq);

void fh2m_inno_disable_irq_nosync(unsigned int irq)
{
	disable_irq_nosync(irq);
}
INNO_EXT_SYM(fh2m_inno_disable_irq_nosync);
