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
#ifndef __INNO_TRACE_H__
#define __INNO_TRACE_H__

#include <linux/types.h>

#define INNO_PROTO(args...) args
#define INNO_ARGS(args...)
#define INNO_DECLARE_TRACE(name, proto, args)  void fh2m_inno_trace_##name(proto);

#define INNO_TRACE_DECLARE_ENTRY() \
	INNO_DECLARE_TRACE(rogue_fence_update, \
		INNO_PROTO(const char *comm, const char *cmd, const char *dm, u32 gpu_id, u32 ctx_id, u32 offset, \
			u32 sync_fwaddr, u32 sync_value), \
		INNO_ARGS(comm, cmd, dm, gpu_id, ctx_id, offset, sync_fwaddr, sync_value)) \
\
	INNO_DECLARE_TRACE(rogue_fence_check, \
		INNO_PROTO(const char *comm, const char *cmd, const char *dm, u32 gpu_id, u32 ctx_id, u32 offset, \
			u32 sync_fwaddr, u32 sync_value), \
		INNO_ARGS(comm, cmd, dm, gpu_id, ctx_id, offset, sync_fwaddr, sync_value)) \
\
	INNO_DECLARE_TRACE(rogue_job_enqueue, \
		INNO_PROTO(u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, const char *kick_type), \
		INNO_ARGS(gpu_id, ctx_id, int_id, ext_id, kick_type)) \
\
	INNO_DECLARE_TRACE(rogue_sched_switch, \
		INNO_PROTO(const char *work_type, u32 switch_type, u64 timestamp, u32 gpu_id, u32 next_ctx_id, \
			u32 next_prio, u32 next_int_id, u32 next_ext_id), \
		INNO_ARGS(work_type, switch_type, timestamp, next_ctx_id, next_prio, next_int_id, gpu_id,  next_ext_id)) \
\
	INNO_DECLARE_TRACE(rogue_create_fw_context, \
		INNO_PROTO(const char *comm, const char *dm, u32 gpu_id, u32 ctx_id), \
		INNO_ARGS(comm, dm, gpu_id, ctx_id)) \
\
	INNO_DECLARE_TRACE(rogue_ufo_update, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, \
			u32 fwaddr, u32 old_value, u32 new_value), \
		INNO_ARGS(timestamp, gpu_id, ctx_id, int_id, ext_id, fwaddr, old_value, new_value)) \
\
	INNO_DECLARE_TRACE(rogue_ufo_check_fail, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, \
			 u32 fwaddr, u32 value, u32 required), \
		INNO_ARGS(timestamp, gpu_id, ctx_id, int_id, ext_id, fwaddr, value, required)) \
\
	INNO_DECLARE_TRACE(rogue_ufo_pr_check_fail, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, \
			 u32 fwaddr, u32 value, u32 required), \
		INNO_ARGS(timestamp, gpu_id, ctx_id, int_id, ext_id, fwaddr, value, required)) \
\
	INNO_DECLARE_TRACE(rogue_ufo_check_success, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, \
			 u32 fwaddr, u32 value), \
		INNO_ARGS(timestamp, gpu_id, ctx_id, int_id, ext_id, fwaddr, value)) \
\
	INNO_DECLARE_TRACE(rogue_ufo_pr_check_success, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, u32 ctx_id, u32 int_id, u32 ext_id, \
			 u32 fwaddr, u32 value), \
		INNO_ARGS(timestamp, gpu_id, ctx_id, int_id, ext_id, fwaddr, value)) \
\
	INNO_DECLARE_TRACE(rogue_events_lost, \
		INNO_PROTO(u32 event_source, u32 gpu_id, u32 last_ordinal, u32 curr_ordinal), \
		INNO_ARGS(event_source, gpu_id, last_ordinal, curr_ordinal)) \
\
	INNO_DECLARE_TRACE(rogue_firmware_activity, \
		INNO_PROTO(u64 timestamp, u32 gpu_id, const char *task, u32 fw_event), \
		INNO_ARGS(timestamp, gpu_id, task, fw_event))

INNO_TRACE_DECLARE_ENTRY();

#endif
