#!/usr/bin/env python3
"""Offline R34 trace configuration and evidence checks; never accesses tracefs."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOTS = (
    'pm_prepare_console', 'suspend_console', 'notifier_call_chain',
    'wait_for_device_probe', 'device_block_probing', '__driver_probe_device',
    'pm_runtime_barrier', '__pm_runtime_barrier', 'rpm_resume', 'rpm_suspend',
    'dpm_prepare', 'dpm_wait_for_subordinate', 'dpm_wait_for_superior',
    'device_suspend', 'device_resume',
    'timer_delete_sync',
)
EVENTS = (
    'notifier/notifier_boundary', 'power/r5_prepare_progress',
    'power/suspend_resume', 'power/device_pm_callback_start',
    'power/device_pm_callback_end', 'rpm/rpm_suspend', 'rpm/rpm_resume',
    'rpm/rpm_idle', 'rpm/rpm_return_int', 'timer/timer_expire_entry',
    'timer/timer_expire_exit', 'sched/sched_switch', 'sched/sched_wakeup',
)
DRIVER_ROOTS = (
    'vpu_notifier', 'workload_timer_callback', 'fantvpu_workload_update',
    'fantgpu_pci_probe', 'fh2m_hal_mcufw_comm_msg_xfer',
    'g0m_soc_check_pcie_irq_response', 'mailbox_interrupt_handler',
)

def plan(system_map):
    symbols = {x[2] for line in system_map.read_text().splitlines()
               if len(x := line.split()) == 3 and x[1] in ('t', 'T')}
    selected = {}
    for name in ROOTS:
        # Require one real text symbol; compiler cloning is explicit, not guessed.
        matches = [x for x in symbols if x == name or x.startswith(name+'.')]
        matches = [x for x in matches if '.cold' not in x]
        if len(matches) != 1:
            raise ValueError(f'{name}: missing or ambiguous symbol: {sorted(matches)}')
        selected[name] = matches[0]
    return {
        'generation': 'r5obs1', 'kernel_release': '6.12.101-r5obs1',
        'system_map_sha256': hashlib.sha256(system_map.read_bytes()).hexdigest(),
        'scope': 'offline configuration; not applied; does not authorize PM',
        'instance': 'r5obs1-<approved-batch>', 'buffer_size_kb_per_cpu': 1024,
        'max_total_buffer_kb': 65536, 'overwrite': False, 'trace_clock': 'mono',
        'current_tracer': 'function_graph', 'set_graph_function': list(selected.values()),
        'driver_roots_require_loaded_symbol_and_ftrace_membership': DRIVER_ROOTS,
        'options': ['funcgraph-abstime', 'funcgraph-proc', 'funcgraph-cpu',
                    'funcgraph-tail', 'funcgraph-retval', 'funcgraph-overrun',
                    'funcgraph-irqs'],
        'events': EVENTS,
        'additional_argument_events': [
            'p:r5obs/cwait wait_for_completion completion=$arg1:x64',
            'p:r5obs/cdone complete_all completion=$arg1:x64',
            'p:r5obs/runtime_barrier pm_runtime_barrier device=$arg1:x64',
            'p:r5obs/probe_wait wait_for_device_probe count=@probe_count:s32',
            'p:r5obs/probe_entry __driver_probe_device driver=$arg1:x64 device=$arg2:x64 count=@probe_count:s32',
            'r:r5obs/probe_exit __driver_probe_device ret=$retval:s32 count=@probe_count:s32',
            'p:r5obs/rpm_resume rpm_resume device=$arg1:x64 flags=$arg2:x32',
            'p:r5obs/cancel_work cancel_work_sync work=$arg1:x64',
        ],
        'admission_requirements': [
            'separate kernel/module installation and experiment authorization',
            'driver rebuilt for exact observation release; current 101 module is not compatible proof',
            'all selected roots are in actual available_filter_functions; no silent omissions',
            'all events/options/argument-probes supported and own instance/group unused',
            'probe_count resolves to exactly one object symbol; count snapshots are not atomic with event ordering',
            'CPU count times buffer size <= memory budget; allocation succeeds before tracing_on',
            'external receiver path reviewed and positive controls independently retained',
            'boot/window identity plus all per-CPU ring/graph/probe loss counters retained',
            'explicit begin/end marker and stop policy; no automatic PM or retry',
        ],
        'channel': 'UNVERIFIED', 'PM': 'NOT_RUN', 'R5': 'FAIL',
    }

def check_records(records, losses):
    """Check normalized events; incomplete pairs are evidence, never deleted."""
    required_loss = {'overrun', 'commit_overrun', 'dropped_events', 'graph_overrun', 'probe_nmissed'}
    if set(losses) != required_loss or any(type(x) is not int or x < 0 for x in losses.values()):
        raise ValueError('all aggregated per-CPU/graph/probe loss counters required')
    if not records or any(losses.values()):
        raise ValueError('empty or lossy window cannot establish coverage')
    pending = {}; pairs = 0; retries = Counter(); last_progress = {}; previous_seq = -1
    for event in records:
        # sequence is assigned when merging the same-boot raw trace, never across boots.
        if type(event.get('seq')) is not int or event['seq'] <= previous_seq:
            raise ValueError('missing/non-increasing event sequence')
        previous_seq = event['seq']
        if type(event.get('pid')) is not int or event['pid'] < 0:
            raise ValueError('task identity required (including IRQ context)')
        if type(event.get('cpu')) is not int or event['cpu'] < 0:
            raise ValueError('CPU identity required (idle PID 0 is per CPU)')
        if event.get('kind') == 'notifier':
            if event.get('phase') not in ('entry', 'exit'):
                raise ValueError('invalid notifier phase')
            # Tasks may migrate; idle tasks share PID 0 but cannot share a stack.
            key = (event['pid'], event['cpu'] if event['pid'] == 0 else None)
            call = (event['nb'], event['callback'], event['action'])
            stack = pending.setdefault(key, [])
            if event['phase'] == 'entry':
                stack.append((call, event['seq']))
            elif not stack:
                raise ValueError('exit without matching entry')
            else:
                if stack[-1][0] != call:
                    raise ValueError('notifier exit violates task call-stack order')
                if type(event.get('ret')) is not int:
                    raise ValueError('exit return required')
                stack.pop(); pairs += 1
        elif event.get('kind') == 'prepare':
            required = ('attempt', 'moved', 'did_move', 'ret')
            if any(type(event.get(k)) is not int for k in required):
                raise ValueError('prepare progress fields required')
            attempt, moved = event['attempt'], event['moved']
            previous = last_progress.get(event['pid'], (0, 0))
            if attempt != previous[0]+1 or event['did_move'] not in (0, 1) or moved != previous[1]+event['did_move']:
                raise ValueError('prepare sequence/list movement discontinuity')
            if event['ret'] != 0 and event['did_move']:
                raise ValueError('failed prepare cannot move to prepared list')
            last_progress[event['pid']] = (attempt, moved)
            if event['ret'] == -11 and not event['did_move']:
                retries[(event['pid'], event['device'], moved)] += 1
        else:
            raise ValueError('unknown normalized record type')
    unmatched = [{'pid':key[0], 'idle_cpu':key[1], 'nb':call[0],
                  'callback':call[1], 'action':call[2], 'entry_seq':seq}
                 for key,stack in pending.items() for call,seq in stack]
    return {'notifier_pairs': pairs, 'unmatched_entries': unmatched,
            'prepare_retry_candidates': [dict(pid=k[0], device=k[1], moved=k[2], count=v)
                                         for k,v in retries.items() if v >= 3],
            'coverage': 'INCOMPLETE' if unmatched else 'SELECTED_RECORDS_PAIRED',
            'root_cause': 'unresolved', 'R5': 'FAIL',
            'boundary': 'normalized subset only; raw graph, identities and channel require review'}

def check_evidence(evidence):
    identity = evidence.get('identity', {})
    if (not re.fullmatch(r'[A-Za-z0-9_.-]+', identity.get('batch', ''))
            or not re.fullmatch(r'[0-9a-f]{32}', identity.get('boot_id', ''))
            or not re.fullmatch(r'[0-9a-f]{64}', identity.get('raw_sha256', ''))
            or identity.get('kernel') != '6.12.101-r5obs1'):
        raise ValueError('exact batch/boot/raw SHA/kernel identity required')
    result = check_records(evidence['records'], evidence['losses'])
    result['identity'] = identity
    result['identity_boundary'] = 'declared identity; reviewer must match preserved raw bytes'
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    p = sub.add_parser('plan'); p.add_argument('system_map', type=Path)
    p = sub.add_parser('check'); p.add_argument('evidence', type=Path)
    args = parser.parse_args()
    if args.mode == 'plan':
        result = plan(args.system_map)
    else:
        evidence = json.loads(args.evidence.read_text())
        result = check_evidence(evidence)
    print(json.dumps(result, indent=2))
