#!/usr/bin/env python3
"""Offline R34 trace configuration and evidence checks; never accesses tracefs."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct

WIRE = struct.Struct('!4sBBH16sQIIQQQ')
FRAME = struct.Struct('<Iq')

def check_stream(path, session):
    """Validate preserved datagrams; this is transport, not PM coverage."""
    if not re.fullmatch('[0-9a-f]{32}', session):
        raise ValueError('exact lowercase session nonce required')
    sequence = 0; begin = None; end = None; stats = {}; types = Counter()
    last_event = {}; last_stats = {}; losses = 0
    with path.open('rb') as stream:
        if stream.read(8) != b'R5CAP01\n':
            raise ValueError('capture magic mismatch')
        while frame := stream.read(FRAME.size):
            if len(frame) != FRAME.size:
                raise ValueError('truncated frame header')
            length, receiver_ticks = FRAME.unpack(frame)
            if not WIRE.size <= length <= WIRE.size + 1280 or receiver_ticks <= 0:
                raise ValueError('invalid frame length/time')
            data = stream.read(length)
            if len(data) != length:
                raise ValueError('truncated datagram')
            magic, version, kind, size, nonce, seq, cpu, pid, ts, lost, oversized = WIRE.unpack_from(data)
            if (magic != b'R5O1' or version != 1 or nonce.hex() != session or
                    seq != sequence or size != length-WIRE.size or end is not None):
                raise ValueError('identity/sequence/size/end boundary mismatch')
            payload = data[WIRE.size:]; sequence += 1
            losses |= lost | oversized
            if kind == 3:
                if seq != 0 or size != 16 or cpu != 0xffffffff:
                    raise ValueError('invalid BEGIN')
                begin = struct.unpack('!4I', payload)
                legacy = (1 <= begin[0] <= 64 and 1 <= begin[1] <= 100 and
                          begin[2:] == (256, 1280))
                bounded = (1 <= begin[0] <= 16 and 1 <= begin[1] <= 5 and
                           begin[2:] == (2048, 128))
                if not (legacy or bounded):
                    raise ValueError('unexpected transport bounds')
            elif begin is None:
                raise ValueError('missing BEGIN')
            elif kind == 1:
                if cpu >= (begin[0] if begin[2:] == (2048, 128) else 64) or not 8 <= size <= begin[3]:
                    raise ValueError('invalid trace entry')
                event_type = int.from_bytes(payload[:2], 'little')
                types[event_type] += 1; last_event[cpu] = seq
            elif kind == 2:
                if cpu >= (begin[0] if begin[2:] == (2048, 128) else 64) or size != 32:
                    raise ValueError('invalid CPU counters')
                values = struct.unpack('!4Q', payload)
                if cpu in stats and any(a < b for a,b in zip(values[:3],stats[cpu][:3])):
                    raise ValueError('loss counters went backwards')
                stats[cpu] = values; last_stats[cpu] = seq
                losses |= values[0] | values[1] | values[2]
            elif kind == 4:
                if size != 16 or cpu != 0xffffffff:
                    raise ValueError('invalid END')
                end = struct.unpack('!QII',payload)
            else:
                raise ValueError('unknown packet kind')
    complete = bool(begin and end == (0,0,0) and not losses and types and
                    len(stats) == begin[0] and all(v[3] == 0 for v in stats.values()) and
                    all(last_stats.get(cpu, -1) > seq for cpu,seq in last_event.items()))
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream,'sha256').hexdigest()
    return {'session':session,'packets':sequence,'event_types':dict(types),
            'transport_complete':complete,'begin':begin,'end':end,'loss_detected':bool(losses),
            'raw_sha256':digest,
            'boundary':'raw transport only; event formats, graph/probe counters, exact boot and PM coverage require separate evidence',
            'PM_coverage':'NOT_ASSESSED','R5':'FAIL'}

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
REQUIRED_CONFIG = (
    'CONFIG_TRACING', 'CONFIG_EVENT_TRACING', 'CONFIG_FUNCTION_GRAPH_TRACER',
    'CONFIG_FUNCTION_GRAPH_RETVAL', 'CONFIG_KPROBE_EVENTS',
)

def resolve_graph_roots(kernel_roots, available, control=None):
    """Resolve actual ftrace sites; never collapse same-name physical functions."""
    sites = {}
    for line in available.splitlines():
        row = line.split()
        if len(row) in (2, 3) and re.fullmatch('[0-9a-fA-F]+', row[0]):
            sites.setdefault(row[1], []).append({'address': row[0], 'module': row[2] if len(row)==3 else None})
    selected = {}
    for name in list(kernel_roots) + ([control] if control else []):
        found = sites.get(name, [])
        if len(found)!=1 or found[0]['module'] is not None:
            raise ValueError('kernel/control root missing or ambiguous: '+name)
        selected[name] = found
    for name in DRIVER_ROOTS:
        matches = [n for n in sites if (n==name or n.startswith(name+'.')) and '.cold' not in n]
        if len(matches)!=1:
            raise ValueError('driver root missing or ambiguous spelling: '+name)
        found = sites[matches[0]]
        if any(r['module']!='[fantgpu]' for r in found) or len({r['address'] for r in found})!=len(found):
            raise ValueError('driver root module/address collision: '+name)
        selected[matches[0]] = found
    return selected

def plan(system_map, config):
    config_lines = config.read_text().splitlines()
    for name in REQUIRED_CONFIG:
        settings = [line for line in config_lines if line.startswith(name+'=')]
        if settings != [name+'=y']:
            raise ValueError('required observation configuration missing: '+name)
    if ('CONFIG_LOCALVERSION="-r5obs1"' not in config_lines
            or '# CONFIG_LOCALVERSION_AUTO is not set' not in config_lines):
        raise ValueError('observation release configuration mismatch')
    symbols = [x[2] for line in system_map.read_text().splitlines()
               if len(x := line.split()) == 3 and x[1] in ('t', 'T')]
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
        'config_sha256': hashlib.sha256(config.read_bytes()).hexdigest(),
        'required_config': {name: 'y' for name in REQUIRED_CONFIG},
        'scope': 'offline configuration; not applied; does not authorize PM',
        'instance': 'r5obs1-<approved-batch>', 'buffer_size_kb_per_cpu': 1024,
        'max_total_buffer_kb': 65536, 'overwrite': False, 'trace_clock': 'mono',
        'current_tracer': 'function_graph', 'set_graph_function': list(selected.values()),
        'graph_root_filter_scope': 'global on Linux 6.12; require exclusive ownership and restore on exit',
        'same_name_driver_functions': 'include every physical ftrace site; retain addresses in evidence',
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
    p.add_argument('--config', type=Path, required=True)
    p = sub.add_parser('check'); p.add_argument('evidence', type=Path)
    p = sub.add_parser('wire-check'); p.add_argument('capture', type=Path)
    p.add_argument('--session', required=True)
    args = parser.parse_args()
    if args.mode == 'plan':
        result = plan(args.system_map, args.config)
    elif args.mode == 'check':
        evidence = json.loads(args.evidence.read_text())
        result = check_evidence(evidence)
    else:
        result = check_stream(args.capture, args.session)
    print(json.dumps(result, indent=2))
    if args.mode == 'wire-check' and not result['transport_complete']:
        raise SystemExit(1)
