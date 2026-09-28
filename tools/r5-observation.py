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

# Shared with prepare-r5-observation.py; a normalized record contract, not an
# assertion that the target kernel already emits these fields.
SEMANTIC_SCHEMA = 'r5obs2-pairs-v1'
PAIR_FIELDS = ('object_id', 'object_generation', 'completion_generation',
               'task_id', 'task_generation', 'call_id')
PAIR_OPERATIONS = ('wait', 'complete', 'callback', 'resume', 'reinit', 'lock',
                   'notifier', 'prepare', 'task', 'skip', 'superior', 'unlock', 'stage')
SEMANTIC_HINTS = set(PAIR_FIELDS) | {'gen', 'generation', 'schema', 'operation', 'pm_phase'}
BUFFER_PAGES = 1920
BUFFER_KIB = BUFFER_PAGES * (4096 - 16) // 1024
MEMORY_CAP = 133 * 1024**2


STAGES = {1:'prepare_console',2:'freeze_processes',3:'suspend_console',4:'resume_console',
          5:'restore_console',6:'thaw_processes',7:'platform_enter',8:'platform_recover',
          9:'enter_state',10:'devices_and_enter',11:'pm_notifier_robust',12:'pm_notifier_chain'}
SKIP_REASONS = {1:'no_pm',2:'async_condition_false',3:'syscore',4:'not_suspended',
                5:'direct_complete',6:'superior_unavailable'}


def semantic_contract():
    return dict(schema=SEMANTIC_SCHEMA, kind='pm_boundary', identity_fields=PAIR_FIELDS,
                operations=PAIR_OPERATIONS, phases=('entry', 'exit'),
                pm_phases=('prepare', 'suspend', 'resume', 'complete'),
                max_records=39936, max_cpus=16,
                call_id_scope='unique within a stable task lifetime; never reused',
                missing_or_conflicting_identity='UNKNOWN_OR_UNPAIRED',
                auxiliary_codes={1:'prepare_progress',2:'stage_entry',3:'stage_exit',4:'async_decision'},
                stages=STAGES, skip_reasons=SKIP_REASONS,
                auxiliary_namespace='stage: object_id=completion_generation=0; not a device ID',
                boundary='normalized evidence only; producer, dictionary and raw binding require review')


def memory_budget():
    """Locked-kernel allocation model, NOT target allocator measurements."""
    cpus = 16
    # BTF sizes 64/552/264; 64-byte cache alignment + kmalloc buckets gives
    # 64/1024/512. Allocator page slack/debug metadata is NOT included here.
    components = dict(data_pages=cpus * BUFFER_PAGES * 4096,
                      reader_pages=cpus * 4096,
                      page_descriptors=cpus * (BUFFER_PAGES + 1) * 64,
                      per_cpu_bucket=cpus * 1024, buffer_bucket=512,
                      cpu_pointer_bucket=128, cpumask_bucket=8,
                      # CONFIG_TRACER_MAX_TRACE=y creates a minimal second
                      # ring even without an allocated full-size snapshot.
                      minimal_snapshot=cpus*(3*4096 + 3*64 + 1024)+512+128+8,
                      trace_array_bucket=8192, trace_name_bucket=64,
                      trace_cpumasks=2*8, array_cpu_payload=2*cpus*128,
                      netpoll_info_bucket=192,
                      dictionary_cap=1024**2, pair_table_cap=4 * 1024**2)
    total = sum(components.values())
    return dict(data_pages_per_cpu=BUFFER_PAGES, buffer_size_kb_per_cpu=BUFFER_KIB,
                cap_bytes=MEMORY_CAP, components=components, accounted_bytes=total,
                remaining_bytes=MEMORY_CAP-total,
                burst_bytes=39936*128, burst_data_page_fraction=39936*128/(BUFFER_PAGES*4096),
                unknown_allocations=['slab backing-page slack/debug metadata',
                    'percpu allocator backing, event files/filters, tracefs and ftrace bookkeeping',
                    'netpoll/skb pools/queue, module and control state, transient peak allocations',
                    'actual dictionary/pair layout and allocator charge'],
                basis='saved R36 BTF and locked SLUB allocation model; no target measurements',
                admission='UNVERIFIED', PM='NOT_RUN')


def check_semantic_records(records):
    """Keep both ends of every conflict; CPU is location, never task identity."""
    if len(records) > semantic_contract()['max_records']:
        raise ValueError('semantic record envelope exceeded')
    pending = {}; seen = set(); tainted = set(); issues = []; pairs = []
    task_pids = {}; previous_seq = -1
    for event in records:
        if (type(event.get('seq')) is not int or event['seq'] <= previous_seq or
                type(event.get('cpu')) is not int or not 0 <= event['cpu'] < 16 or
                type(event.get('pid')) is not int or event['pid'] < 0):
            raise ValueError('semantic sequence/CPU/PID invalid')
        previous_seq = event['seq']
        if (event.get('schema') != SEMANTIC_SCHEMA or event.get('kind') != 'pm_boundary' or
                event.get('phase') not in ('entry', 'exit') or
                event.get('operation') not in PAIR_OPERATIONS or
                event.get('pm_phase') not in semantic_contract()['pm_phases']):
            raise ValueError('semantic schema/operation/phase mismatch')
        allowed = set(PAIR_FIELDS) | {'seq', 'cpu', 'pid', 'schema', 'kind', 'phase',
                                     'operation', 'pm_phase', 'ret', 'peer_id', 'flags'}
        if set(event) - allowed:
            raise ValueError('unknown semantic fields; identity aliases are not silently ignored')
        if any(type(event.get(k)) is not int or not 1 <= event[k] < 2**64 for k in PAIR_FIELDS):
            issues.append(dict(status='UNKNOWN', reason='missing stable identity', record=event))
            continue
        if event['phase'] == 'exit' and type(event.get('ret')) is not int:
            raise ValueError('semantic exit return required')
        if (type(event.get('peer_id', 0)) is not int or not 0 <= event.get('peer_id', 0) <= 512
                or type(event.get('flags', 0)) is not int or event.get('flags', 0) != 0):
            tainted.add((event['task_id'], event['task_generation'], event['call_id']))
            issues.append(dict(status='UNKNOWN', reason='invalid producer identity/census', record=event))
            continue
        task = (event['task_id'], event['task_generation'])
        if task in task_pids and task_pids[task] != event['pid']:
            issues.append(dict(status='UNKNOWN', reason='stable task has conflicting PID', record=event))
            continue
        task_pids[task] = event['pid']
        call = (*task, event['call_id'])
        identity = tuple(event[k] for k in PAIR_FIELDS) + (event['operation'], event['pm_phase'], event.get('peer_id', 0))
        if event['phase'] == 'entry':
            if call in seen:
                tainted.add(call)
                issues.append(dict(status='UNPAIRED', reason='duplicate/reused call ID', record=event))
            else:
                seen.add(call)
                pending[call] = (identity, event)
        elif call not in pending:
            # Also invalidate a prior positive if a second exit later appears.
            tainted.add(call)
            issues.append(dict(status='UNPAIRED', reason='exit without entry', record=event))
        else:
            before, entry = pending[call]
            if (call in tainted or before != identity or
                    (event['pid'] == 0 and entry['cpu'] != event['cpu'])):
                tainted.add(call)
                issues.append(dict(status='UNPAIRED', reason='identity/phase/idle CPU conflict', record=event))
            else:
                pairs.append(dict(call=call, entry=entry, exit=event))
                del pending[call]
    valid = []
    for pair in pairs:
        if pair['call'] in tainted:
            issues.append(dict(status='UNPAIRED', reason='later duplicate invalidates pair', pair=pair))
        else:
            valid.append(pair)
    unmatched = [entry for _, entry in pending.values()]
    return dict(schema=SEMANTIC_SCHEMA, semantic_pairs=len(valid), pairs=valid,
                unmatched_entries=unmatched, issues=issues,
                coverage='UNKNOWN' if issues else ('UNPAIRED' if unmatched else 'SEMANTIC_RECORDS_PAIRED'),
                root_cause='unresolved', R5='FAIL',
                boundary='selected normalized pairs only; not full dictionary, PM coverage or root cause')

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
            'cpu_counters':stats,'last_event_wire_seq':last_event,'last_stats_wire_seq':last_stats,
            'PM_coverage':'NOT_ASSESSED','R5':'FAIL'}


# Exact x86-64 trace layouts emitted by SEMANTIC_EVENTS in the generator.
# IDs are obtained from saved format files, never guessed from another boot.
RAW_PAIR = struct.Struct('<6Q4IIi')
RAW_DICTIONARY = struct.Struct('<Q4I48s24s')
RAW_AUX = struct.Struct('<6Q4I2QIi')
RAW_LAYOUTS = {
    'r5_aux': [('common_type',0,2), ('common_flags',2,1), ('common_preempt_count',3,1),
               ('common_pid',4,4), ('token',8,64), ('a',72,8), ('b',80,8), ('code',88,4), ('ret',92,4)],
    'r5_pair': [('common_type',0,2), ('common_flags',2,1), ('common_preempt_count',3,1),
                ('common_pid',4,4), ('token',8,64), ('exit',72,4), ('ret',76,4)],
    'r5_dictionary': [('common_type',0,2), ('common_flags',2,1), ('common_preempt_count',3,1),
                      ('common_pid',4,4), ('life',8,8), ('id',16,4), ('parent',20,4),
                      ('supplier',24,4), ('kind',28,4), ('name',32,48), ('driver',80,24)],
}

def semantic_format(path, name):
    text = path.read_text()
    if re.findall(r'^name: (\w+)$', text, re.M) != [name]:
        raise ValueError('wrong semantic event format name')
    ids = re.findall(r'^ID: (\d+)$', text, re.M)
    if len(ids) != 1 or not 1 <= int(ids[0]) < 65536:
        raise ValueError('invalid semantic event ID')
    fields = []
    for decl, offset, size in re.findall(r'field:([^;]+);\s*offset:(\d+);\s*size:(\d+);',text):
        field = decl.strip().split()[-1].split('[')[0]
        fields.append((field,int(offset),int(size)))
    if fields != RAW_LAYOUTS[name]:
        raise ValueError('semantic event layout drift')
    return int(ids[0])


def check_auxiliary(auxiliary, records, objects, life):
    """Same pair checker for stage calls; progress binds to the real prepare call."""
    issues=[]; stages=[]; progress=[]; decisions=[]; locations=[]
    completed=check_semantic_records(records)['pairs']
    def key(event):
        return tuple(event[k] for k in PAIR_FIELDS)
    pairs={key(pair['entry']):pair for pair in completed}
    device_calls={(event['task_id'],event['task_generation'],event['call_id']) for event in records}
    seen=set(); last={}
    for seq,(timestamp,wire_seq,event) in enumerate(sorted(auxiliary,key=lambda row:row[:2])):
        event=dict(event,seq=seq)
        locations.append(dict(seq=seq,wire_seq=wire_seq,timestamp=timestamp,cpu=event['cpu']))
        code,a,b=event['code'],event['a'],event['b']
        if event['flags'] or event['object_generation'] != life:
            issues.append(dict(reason='auxiliary invalidated identity',record=event))
        if code in (2,3):
            if (event['task_id'],event['task_generation'],event['call_id']) in device_calls:
                issues.append(dict(reason='stage/device call ID collision',record=event))
            if (a not in STAGES or event['object_id'] or event['completion_generation'] or
                    event['peer_id'] or event['operation']!='stage' or event['pm_phase']!='prepare' or
                    (code==2 and event['ret'])):
                raise ValueError('stage namespace/layout mismatch')
            # Separate projection: stage IDs never resolve through device dictionary.
            stage={k:v for k,v in event.items() if k not in ('a','b','code')}
            stage.update(object_id=a,completion_generation=b+1,kind='pm_boundary',schema=SEMANTIC_SCHEMA,
                         phase='entry' if code==2 else 'exit')
            if b >= 2**64-1:
                raise ValueError('stage value overflow')
            stages.append(stage)
            continue
        if event['object_id'] not in objects or not 1 <= event['completion_generation'] <= 7:
            raise ValueError('auxiliary object/generation invalid')
        identity=key(event);pair=pairs.get(identity);unique=(code,identity)
        if unique in seen or pair is None:
            issues.append(dict(reason='duplicate or unpaired auxiliary call',record=event))
            continue
        seen.add(unique)
        if (event['operation']!=pair['entry']['operation'] or event['peer_id']!=pair['entry'].get('peer_id',0) or
                event['pm_phase']!=pair['entry']['pm_phase']):
            issues.append(dict(reason='auxiliary call metadata conflict',record=event))
        if code==1:
            if event['operation']!='prepare':
                raise ValueError('progress must bind prepare')
            moved,did_move=b>>1,b&1
            task=(event['task_id'],event['task_generation'])
            attempt_before,moved_before=last.get(task,(0,0))
            if (a!=attempt_before+1 or moved!=moved_before+did_move or
                    (event['ret'] and did_move) or event['ret']!=pair['exit']['ret']):
                issues.append(dict(reason='prepare progression/return mismatch',record=event))
            last[task]=(a,moved)
            progress.append(dict(record=event,attempt=a,moved=moved,did_move=did_move))
        elif code==4:
            if event['operation']!='task' or a not in (0,1) or b not in (0,1) or (b and not a) or event['ret']:
                raise ValueError('async decision domain mismatch')
            if pair['exit']['ret']!=b:
                issues.append(dict(reason='async decision/return mismatch',record=event))
            decisions.append(dict(record=event,async_enabled=bool(a),queued=bool(b),
                                  interpretation='queued is not task execution; false uses existing synchronous fallback'))
    for identity,pair in pairs.items():
        op=pair['entry']['operation'];code={'prepare':1,'task':4}.get(op)
        if code and (code,identity) not in seen:
            issues.append(dict(reason='missing required auxiliary observation',pair=pair))
        if op=='skip' and pair['exit']['ret'] not in SKIP_REASONS:
            raise ValueError('unknown skip reason')
    stage_result=check_semantic_records(stages)
    coverage='UNKNOWN' if issues else ('UNPAIRED' if stage_result['coverage']!='SEMANTIC_RECORDS_PAIRED'
                                      else 'AUXILIARY_CONSISTENT')
    return dict(coverage=coverage,issues=issues,stages=stage_result,prepare_progress=progress,
                async_decisions=decisions,raw_locations=locations,
                boundary='selected stage calls and actual progression only; not all required stages or PM success')

def semantic_wire_check(path, session, pair_format, dictionary_format, aux_format=None):
    transport = check_stream(path, session)  # strict frame/nonce/order validation
    if sum(transport['event_types'].values()) > semantic_contract()['max_records']:
        raise ValueError('raw semantic record envelope exceeded')
    pair_id = semantic_format(pair_format, 'r5_pair')
    dictionary_id = semantic_format(dictionary_format, 'r5_dictionary')
    aux_id = semantic_format(aux_format, 'r5_aux') if aux_format else None
    if len({pair_id,dictionary_id,aux_id}) != 3:
        raise ValueError('event IDs collide')
    objects = {}; edges = set(); dictionary_end = None; life = None; events = []; auxiliary = []
    def short_name(raw):
        head, sep, tail = raw.partition(b'\0')
        if not sep or any(tail) or any(c < 32 or c == 127 for c in head):
            raise ValueError('dictionary string not bounded/zero padded')
        return head.decode('utf-8')
    with path.open('rb') as stream:
        stream.read(8)
        while frame := stream.read(FRAME.size):
            length, _ = FRAME.unpack(frame)
            data = stream.read(length)
            _,_,kind,_,_,seq,cpu,_,ts,_,_ = WIRE.unpack_from(data)
            if kind != 1:
                continue
            payload = data[WIRE.size:]
            event_id,_,_,pid = struct.unpack_from('<HBBi',payload)
            if event_id == dictionary_id:
                if len(payload) != 104 or dictionary_end is not None:
                    raise ValueError('dictionary layout/end boundary')
                generation,obj,parent,supplier,tag,name,driver = RAW_DICTIONARY.unpack_from(payload,8)
                if generation != 1 or (life is not None and generation != life):
                    raise ValueError('dictionary lifecycle changed')
                life = generation
                name,driver = short_name(name),short_name(driver)
                if tag == 0:
                    if not 1 <= obj <= 512 or obj in objects or supplier or not name or parent > 512:
                        raise ValueError('invalid/duplicate dictionary object')
                    objects[obj] = dict(parent=parent,name=name,driver=driver,completion_id=obj)
                elif tag == 1:
                    if (parent or name or driver or not 1 <= obj <= 512 or
                            not 1 <= supplier <= 512 or (obj,supplier) in edges or len(edges) >= 1024):
                        raise ValueError('invalid/duplicate supplier edge')
                    edges.add((obj,supplier))
                elif tag == 2:
                    if supplier or name != 'END_DICTIONARY' or driver:
                        raise ValueError('invalid census end')
                    dictionary_end = (obj,parent)
                else:
                    raise ValueError('unknown dictionary kind')
            elif event_id == aux_id:
                if len(payload) != 96:
                    raise ValueError('auxiliary record layout')
                values = RAW_AUX.unpack_from(payload,8)
                item = dict(zip(PAIR_FIELDS,values[:6]))
                peer,op,phase,flags,a,b,code,ret = values[6:]
                if (pid < 0 or item['task_id'] != pid+1 or not item['task_generation'] or
                        not item['call_id'] or op >= len(PAIR_OPERATIONS) or phase >= 4 or code not in (1,2,3,4)):
                    raise ValueError('auxiliary identity/code invalid')
                item.update(seq=seq,cpu=cpu,pid=pid,peer_id=peer,flags=flags,a=a,b=b,code=code,ret=ret,
                            operation=PAIR_OPERATIONS[op],pm_phase=semantic_contract()['pm_phases'][phase])
                auxiliary.append((ts,seq,item))
            elif event_id == pair_id:
                if len(payload) != 80 or len(events) >= 39936:
                    raise ValueError('pair layout/envelope')
                values = RAW_PAIR.unpack_from(payload,8)
                event = dict(zip(PAIR_FIELDS,values[:6]))
                peer,op,phase,flags,exit_event,ret = values[6:]
                if op >= len(PAIR_OPERATIONS) or phase >= 4 or exit_event not in (0,1):
                    raise ValueError('unknown raw operation/phase')
                if pid < 0 or event['task_id'] != pid+1 or not 1 <= event['completion_generation'] <= 7:
                    raise ValueError('raw task/generation mismatch')
                event.update(seq=seq,cpu=cpu,pid=pid,peer_id=peer,flags=flags,
                             schema=SEMANTIC_SCHEMA,kind='pm_boundary',
                             phase='exit' if exit_event else 'entry',operation=PAIR_OPERATIONS[op],
                             pm_phase=semantic_contract()['pm_phases'][phase])
                if exit_event:
                    event['ret']=ret
                events.append((ts,seq,event))
            else:
                # Narrow raw profile: another event must have an explicit decoder.
                raise ValueError('unknown event in semantic-only profile')
    dictionary_complete = bool(objects and dictionary_end == (len(objects),len(edges)) and
        set(objects) == set(range(1,len(objects)+1)) and
        all(not o['parent'] or o['parent'] in objects for o in objects.values()) and
        all(a in objects and b in objects for a,b in edges))
    ordered = []; raw_locations = []
    # Transport drains CPUs independently. Restore mono timestamp order before
    # applying the SAME production pairing checker; never pair by receive order.
    for index,(timestamp,wire_seq,event) in enumerate(sorted(events, key=lambda item:item[:2])):
        raw_locations.append(dict(seq=index,wire_seq=wire_seq,timestamp=timestamp,cpu=event['cpu']))
        event['seq'] = index
        if (event['object_id'] not in objects or event['object_generation'] != life or
                (event['peer_id'] and event['peer_id'] not in objects)):
            event['flags'] |= 1
        ordered.append(event)
    result = check_semantic_records(ordered)
    aux_result = check_auxiliary(auxiliary, ordered, objects, life) if aux_format else None
    if aux_result and aux_result['coverage'] != 'AUXILIARY_CONSISTENT':
        result['coverage'] = aux_result['coverage']
    if not transport['transport_complete']:
        result['coverage'] = 'INCOMPLETE_WITH_LOSS'
    elif not dictionary_complete or not ordered:
        result['coverage'] = 'UNKNOWN'
    result.update(transport=transport,dictionary_complete=dictionary_complete,
                  objects=objects,supplier_edges=sorted(edges),
                  raw_locations=raw_locations,auxiliary=aux_result,
                  format_sha256={name:hashlib.sha256(p.read_bytes()).hexdigest()
                                 for name,p in (('r5_pair',pair_format),('r5_dictionary',dictionary_format),
                                                ('r5_aux',aux_format)) if p is not None},
                  boundary='selected semantic events only; no full PM coverage or root cause; boot identity must match saved formats')
    return result

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
    if any(event.get('kind') == 'pm_boundary' for event in records):
        return check_semantic_records(records)
    if any(SEMANTIC_HINTS.intersection(event) for event in records):
        raise ValueError('semantic identity fields require explicit pm_boundary schema; never ignore them')
    pending = {}; pairs = 0; retries = Counter(); last_progress = {}; previous_seq = -1
    unassociated = []
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
            # Legacy PID-only records cannot prove identity across migration.
            key = (event['pid'], event['cpu'] if event['pid'] == 0 else None)
            call = (event['nb'], event['callback'], event['action'])
            stack = pending.setdefault(key, [])
            if event['phase'] == 'entry':
                stack.append((call, event['seq'], event['cpu']))
            elif not stack:
                raise ValueError('exit without matching entry')
            else:
                if stack[-1][0] != call:
                    raise ValueError('notifier exit violates task call-stack order')
                if type(event.get('ret')) is not int:
                    raise ValueError('exit return required')
                if stack[-1][2] != event['cpu']:
                    unassociated.append(dict(reason='legacy migration lacks stable task identity', record=event))
                    continue
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
                 for key,stack in pending.items() for call,seq,cpu in stack]
    return {'notifier_pairs': pairs, 'unmatched_entries': unmatched,
            'unassociated_exits': unassociated,
            'prepare_retry_candidates': [dict(pid=k[0], device=k[1], moved=k[2], count=v)
                                         for k,v in retries.items() if v >= 3],
            'coverage': 'UNKNOWN' if unassociated else ('INCOMPLETE' if unmatched else 'SELECTED_RECORDS_PAIRED'),
            'root_cause': 'unresolved', 'R5': 'FAIL',
            'boundary': 'normalized subset only; raw graph, identities and channel require review'}

def check_evidence(evidence):
    identity = evidence.get('identity', {})
    semantic = evidence.get('schema') == SEMANTIC_SCHEMA
    if 'schema' in evidence and not semantic:
        raise ValueError('unknown evidence schema')
    if (not re.fullmatch(r'[A-Za-z0-9_.-]+', identity.get('batch', ''))
            or not re.fullmatch(r'[0-9a-f]{32}', identity.get('boot_id', ''))
            or not re.fullmatch(r'[0-9a-f]{64}', identity.get('raw_sha256', ''))
            or identity.get('kernel') not in (('6.12.101-r5obs2','6.12.101-r5obs2-r46') if semantic else ('6.12.101-r5obs1',))):
        raise ValueError('exact batch/boot/raw SHA/kernel identity required')
    if any(e.get('kind') == 'pm_boundary' for e in evidence['records']) != semantic:
        raise ValueError('evidence/record schema mismatch')
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
    p = sub.add_parser('semantic-wire-check'); p.add_argument('capture',type=Path)
    p.add_argument('--session',required=True)
    p.add_argument('--pair-format',type=Path,required=True)
    p.add_argument('--dictionary-format',type=Path,required=True)
    p.add_argument('--aux-format',type=Path)
    args = parser.parse_args()
    if args.mode == 'plan':
        result = plan(args.system_map, args.config)
    elif args.mode == 'check':
        evidence = json.loads(args.evidence.read_text())
        result = check_evidence(evidence)
    elif args.mode == 'semantic-wire-check':
        result = semantic_wire_check(args.capture,args.session,args.pair_format,args.dictionary_format,args.aux_format)
    else:
        result = check_stream(args.capture, args.session)
    print(json.dumps(result, indent=2))
    if args.mode == 'wire-check' and not result['transport_complete']:
        raise SystemExit(1)
    if args.mode in ('check','semantic-wire-check') and result['coverage'] not in ('SELECTED_RECORDS_PAIRED', 'SEMANTIC_RECORDS_PAIRED'):
        raise SystemExit(1)
