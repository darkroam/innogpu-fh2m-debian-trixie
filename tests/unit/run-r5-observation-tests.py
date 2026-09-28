#!/usr/bin/env python3
"""Offline unit checks. Never mounts/reads tracefs or loads a module."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import subprocess
import struct
import io
import hashlib
import json
import re
import threading
import http.client

ROOT = Path(__file__).resolve().parents[2]

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

obs = load('obs', ROOT/'tools/r5-observation.py')
prepare = load('prepare', ROOT/'tools/prepare-r5-observation.py')
upload = load('upload', ROOT/'tools/r5-observation-upload.py')
count = 0

def rejects(fn):
    global count
    try:
        fn()
    except (ValueError, FileNotFoundError):
        count += 1
    else:
        raise AssertionError('expected fail-closed rejection')

losses = dict(overrun=0, commit_overrun=0, dropped_events=0, graph_overrun=0, probe_nmissed=0)
entry = dict(seq=1, pid=12, cpu=0, kind='notifier', phase='entry', nb='n1', callback='vpu_notifier', action=2)
exit_event = dict(entry, seq=2, phase='exit', ret=0)
rejects(lambda: obs.check_evidence({'records':[entry, exit_event], 'losses':losses}))
identity = dict(batch='fixture',boot_id='a'*32,raw_sha256='b'*64,kernel='6.12.101-r5obs1')
assert obs.check_evidence(dict(identity=identity,records=[entry,exit_event],losses=losses))['R5']=='FAIL'
count += 1
assert obs.check_records([entry, exit_event], losses)['notifier_pairs'] == 1
count += 1
assert obs.check_records([entry], losses)['coverage'] == 'INCOMPLETE'
count += 1
# Recursive and different-task invocations must not overwrite one another.
events = [entry, dict(entry, seq=2), dict(entry, seq=3, pid=13),
          dict(exit_event, seq=4, pid=13), dict(exit_event, seq=5), dict(exit_event, seq=6)]
assert obs.check_records(events, losses)['notifier_pairs'] == 3
count += 1
assert obs.check_records([entry, dict(exit_event, cpu=1)], losses)['coverage'] == 'UNKNOWN'
count += 1
idle = dict(entry, pid=0)
rejects(lambda: obs.check_records([idle, dict(exit_event, pid=0, cpu=1)], losses))
assert obs.check_records([idle, dict(idle, seq=2, cpu=1),
                         dict(exit_event, seq=3, pid=0),
                         dict(exit_event, seq=4, pid=0, cpu=1)], losses)['notifier_pairs'] == 2
count += 1
rejects(lambda: obs.check_records([entry, dict(entry, seq=2, nb='n2'),
                                  dict(exit_event, seq=3)], losses))
rejects(lambda: obs.check_records([dict(entry, cpu=None)], losses))
retries = [dict(seq=n, pid=12, cpu=0, kind='prepare', device='gpu', attempt=n, moved=0, did_move=0, ret=-11) for n in (1,2,3)]
assert obs.check_records(retries, losses)['prepare_retry_candidates'][0]['count'] == 3
count += 1
retries.append(dict(seq=4,pid=12,cpu=0,kind='prepare',device='gpu',attempt=4,moved=1,did_move=1,ret=0))
assert obs.check_records(retries, losses)['R5'] == 'FAIL'
count += 1
for bad in ([], [exit_event], [entry, entry], [dict(entry, seq=3), exit_event],
            [dict(entry, phase='bogus')], [dict(entry, pid=-1)]):
    rejects(lambda: obs.check_records(bad, losses))
rejects(lambda: obs.check_records([entry], dict(losses, overrun=1)))
rejects(lambda: obs.check_records([entry], {}))
bad = copy.deepcopy(retries); bad[-1]['moved'] = 9
rejects(lambda: obs.check_records(bad, losses))
bad = copy.deepcopy(retries); bad[-1]['ret'] = -11
rejects(lambda: obs.check_records(bad, losses))

# R44: exercise the SAME production entry point, not a fixture-only checker.
rejects(lambda: obs.check_records([dict(entry, task_generation=1),
                                  dict(exit_event, task_generation=2)], losses))
semantic = dict(seq=1, pid=12, cpu=0, kind='pm_boundary', schema=obs.SEMANTIC_SCHEMA,
                phase='entry', operation='wait', pm_phase='resume', object_id=5,
                object_generation=1, completion_generation=2, task_id=7,
                task_generation=3, call_id=9)
finished = dict(semantic, seq=2, cpu=1, phase='exit', ret=0)
assert obs.check_records([semantic, finished], losses)['semantic_pairs'] == 1; count+=1
# Every identity dimension must match; retain the entry and conflicting exit.
for field in obs.PAIR_FIELDS:
    result = obs.check_records([semantic, dict(finished, **{field:finished[field]+1})], losses)
    assert result['semantic_pairs']==0 and result['coverage'] in ('UNKNOWN','UNPAIRED')
    assert result['unmatched_entries'] and result['issues']; count+=1
for field in obs.PAIR_FIELDS:
    bad = dict(finished); del bad[field]
    assert obs.check_records([semantic,bad], losses)['coverage']=='UNKNOWN'; count+=1
assert obs.check_records([semantic], losses)['coverage']=='UNPAIRED'; count+=1
for rows in ([semantic,dict(semantic,seq=2),dict(finished,seq=3)],
             [semantic,finished,dict(finished,seq=3)],
             [semantic,finished,dict(semantic,seq=3),dict(finished,seq=4)],
             [semantic,dict(finished,pm_phase='suspend')],
             [semantic,dict(finished,operation='complete')],
             [dict(semantic,pid=0),dict(finished,pid=0)]):
    result=obs.check_records(rows,losses)
    assert result['semantic_pairs']==0 and result['coverage']=='UNKNOWN'; count+=1
# Distinct lifetimes/calls, same PID, are legitimate when each pair is complete.
second=dict(semantic,seq=3,task_generation=4,object_generation=2)
result=obs.check_records([semantic,finished,second,dict(second,seq=4,phase='exit',ret=-5)],losses)
assert result['semantic_pairs']==2 and result['coverage']=='SEMANTIC_RECORDS_PAIRED'; count+=1
# Negative callback return still pairs; it does not prove complete or wait exit.
assert obs.check_records([dict(semantic,operation='callback'),
                          dict(finished,operation='callback',ret=-5)],losses)['semantic_pairs']==1; count+=1
for bad in (dict(finished,gen=99),dict(finished,task_id=True),dict(finished,schema='other'),
            dict(finished,cpu=16),dict(finished,phase='skip')):
    if bad.get('task_id') is True:
        assert obs.check_records([semantic,bad],losses)['coverage']=='UNKNOWN';count+=1
    else:rejects(lambda: obs.check_records([semantic,bad],losses))
rejects(lambda: obs.check_records([semantic,exit_event],losses))
rejects(lambda: obs.check_records([semantic],dict(losses,dropped_events=1)))
rejects(lambda: obs.check_records([dict(semantic,seq=i) for i in range(39937)],losses))
new_identity=dict(identity,kernel='6.12.101-r5obs2')
envelope=dict(schema=obs.SEMANTIC_SCHEMA,identity=new_identity,records=[semantic,finished],losses=losses)
assert obs.check_evidence(envelope)['coverage']=='SEMANTIC_RECORDS_PAIRED';count+=1
rejects(lambda: obs.check_evidence(dict(envelope,identity=identity)))
rejects(lambda: obs.check_evidence(dict(envelope,schema='unknown')))
rejects(lambda: obs.check_evidence(dict(envelope,records=[entry,exit_event])))
budget=obs.memory_budget()
assert budget['admission']=='UNVERIFIED' and budget['remaining_bytes']>0
assert budget['cap_bytes']==133*1024**2 and budget['buffer_size_kb_per_cpu']==7650
assert budget['components']['minimal_snapshot']>0 and budget['unknown_allocations'];count+=1
assert prepare._analysis.semantic_contract()==obs.semantic_contract();count+=1
assert prepare._analysis.memory_budget()==budget;count+=1

with tempfile.TemporaryDirectory(prefix='r5obs-unit-') as tmp:
    tmp = Path(tmp)
    # The actual CLI must not turn an UNKNOWN result into exit 0.
    evidence_file=tmp/'semantic.json'
    for records,expected_rc in (([semantic,finished],0),([semantic],1),
                               ([semantic,dict(finished,task_generation=4)],1)):
        evidence_file.write_text(json.dumps(dict(envelope,records=records)))
        checked=subprocess.run(['python3','-B',str(ROOT/'tools/r5-observation.py'),
                                'check',str(evidence_file)],capture_output=True,text=True)
        assert checked.returncode==expected_rc,checked.stdout+checked.stderr
        assert json.loads(checked.stdout)['R5']=='FAIL';count+=1
    symbols = tmp/'System.map'
    symbols.write_text('\n'.join(f'000000000000{n:04x} t {name}' for n,name in enumerate(obs.ROOTS)))
    config = tmp/'config'
    valid_config = '\n'.join(name+'=y' for name in obs.REQUIRED_CONFIG) + '\nCONFIG_LOCALVERSION="-r5obs1"\n# CONFIG_LOCALVERSION_AUTO is not set\n'
    config.write_text(valid_config)
    p = obs.plan(symbols, config)
    assert p['PM']=='NOT_RUN' and p['overwrite'] is False and p['buffer_size_kb_per_cpu']==1024
    count += 1
    for name in ('CONFIG_FUNCTION_GRAPH_RETVAL', 'CONFIG_KPROBE_EVENTS'):
        config.write_text(valid_config.replace(name+'=y', '# '+name+' is not set'))
        rejects(lambda: obs.plan(symbols, config))
    config.write_text(valid_config.replace('"-r5obs1"', '"-other"'))
    rejects(lambda: obs.plan(symbols, config))
    config.write_text(valid_config)
    original_symbols = symbols.read_text()
    symbols.write_text(original_symbols+'\n0000000000020000 t rpm_resume\n')
    rejects(lambda: obs.plan(symbols, config))
    symbols.write_text(original_symbols)
    symbols.write_text(symbols.read_text()+'\n0000000000010000 t rpm_resume.constprop.0\n')
    rejects(lambda: obs.plan(symbols, config))
    symbols.write_text('0000000000000000 t suspend_console\n')
    rejects(lambda: obs.plan(symbols, config))
    source = tmp/'source'; source.mkdir()
    rejects(lambda: prepare.prepare(source, source/'inside'))
    existing = tmp/'existing'; existing.mkdir()
    rejects(lambda: prepare.prepare(source, existing))
    rejects(lambda: prepare.prepare(source, tmp/'new'))
    assert not (tmp/'new').exists()
    count += 1

    available = '1000 kernel_root\n1004 control_read\n' + '\n'.join(
        f'{0x2000+n*16:x} {name} [fantgpu]' for n,name in enumerate(obs.DRIVER_ROOTS))
    available += '\n3000 mailbox_interrupt_handler [fantgpu]\n'
    resolved = obs.resolve_graph_roots(['kernel_root'],available,'control_read')
    assert len(resolved['mailbox_interrupt_handler'])==2
    assert sum(map(len,resolved.values()))==len(obs.DRIVER_ROOTS)+3
    count += 1
    rejects(lambda: obs.resolve_graph_roots(['kernel_root'],available,'missing_control'))
    rejects(lambda: obs.resolve_graph_roots(['kernel_root'],available+'4000 kernel_root\n'))
    rejects(lambda: obs.resolve_graph_roots(['kernel_root'],available.replace('3000 mailbox_interrupt_handler [fantgpu]','3000 mailbox_interrupt_handler [other]')))
    rejects(lambda: obs.resolve_graph_roots(['kernel_root'],available+'4000 mailbox_interrupt_handler.constprop.0 [fantgpu]\n'))
    rejects(lambda: obs.resolve_graph_roots(['kernel_root'],available+'3000 mailbox_interrupt_handler [fantgpu]\n'))

    # Execute the production C serializer with only its kernel I/O stubbed.
    # This catches C/Python layout and byte-order mismatches, not just Python roundtrips.
    c = prepare.EXPORT_MODULE
    header = c[c.index('struct wire_header {'):c.index('static struct trace_array *array;')]
    send = c[c.index('static void send_packet('):c.index('static void stats(')]
    adapter = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
typedef uint8_t u8; typedef uint32_t u32; typedef uint64_t u64;
typedef uint16_t __be16; typedef uint32_t __be32; typedef uint64_t __be64;
#define __packed __attribute__((packed))
#define PAYLOAD 1280
#define cpu_to_be16 htobe16
#define cpu_to_be32 htobe32
#define cpu_to_be64 htobe64
#define task_pid_nr(x) 11
#define local_irq_save(x) ((x)=0)
#define local_irq_restore(x) ((void)(x))
static u64 sequence, lost, oversized;
static int np;
static void netpoll_send_udp(void *np, const char *data, int length) {
    uint32_t n=htole32(length); uint64_t ticks=htole64(1);
    fwrite(&n,4,1,stdout); fwrite(&ticks,8,1,stdout); fwrite(data,length,1,stdout);
}
'''
    main = r'''
int main(void) {
    __be32 begin[4]={htobe32(1),htobe32(20),htobe32(256),htobe32(1280)};
    fwrite("R5CAP01\n",8,1,stdout);
    memcpy(packet.header.magic,"R5O1",4); packet.header.version=1;
    memset(packet.header.session,0xaa,16);
    memcpy(packet.payload,begin,16); send_packet(3,~0U,1,16);
    memset(packet.payload,0,8); packet.payload[0]=10; send_packet(1,0,2,8);
    memset(packet.payload,0,32); send_packet(2,0,3,32);
    send_packet(4,~0U,4,16); return 0;
}
'''
    source = tmp/'wire.c'; executable = tmp/'wire'; capture = tmp/'capture.r5o'
    source.write_text(adapter+header+send+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(executable)],check=True)
    raw = subprocess.check_output([str(executable)])
    capture.write_bytes(raw)
    assert obs.check_stream(capture,'a'*32)['transport_complete']
    assert obs.check_stream(capture,'a'*32)['R5']=='FAIL'
    count += 1
    rejects(lambda: obs.check_stream(capture,'b'*32))
    rejects(lambda: obs.check_stream(capture,'invalid'))
    frames=[]; at=8
    while at<len(raw):
        n,ticks=obs.FRAME.unpack_from(raw,at)
        frames.append(raw[at+obs.FRAME.size:at+obs.FRAME.size+n]); at+=obs.FRAME.size+n
    def capture_frames(rows):
        capture.write_bytes(b'R5CAP01\n'+b''.join(obs.FRAME.pack(len(r),1)+r for r in rows))
    for bad in (raw[:-1], b'badmagic'+raw[8:]):
        capture.write_bytes(bad); rejects(lambda: obs.check_stream(capture,'a'*32))
    for rows in (frames[1:], [frames[0],*frames[2:]], frames+[frames[-1]],
                 [frames[0],frames[2],frames[1],frames[3]]):
        capture_frames(rows); rejects(lambda: obs.check_stream(capture,'a'*32))
    capture_frames(frames[:-1])
    assert not obs.check_stream(capture,'a'*32)['transport_complete']; count+=1
    for index,offset,value in ((1,48,1),(1,56,1),(2,64,1),(2,88,1),(3,64,1)):
        rows=list(frames); changed=bytearray(rows[index]); struct.pack_into('!Q',changed,offset,value)
        rows[index]=bytes(changed); capture_frames(rows)
        assert not obs.check_stream(capture,'a'*32)['transport_complete']; count+=1

    # New transport bounds; old bytes above still use their original bounds.
    bounded = list(frames)
    changed = bytearray(bounded[0]); struct.pack_into('!4I',changed,64,1,1,2048,128)
    bounded[0] = bytes(changed); capture_frames(bounded)
    assert obs.check_stream(capture,'a'*32)['transport_complete']; count+=1
    for offset,value in ((64,17),(68,20),(72,256),(76,129)):
        bad=list(bounded); changed=bytearray(bad[0]);struct.pack_into('!I',changed,offset,value)
        bad[0]=bytes(changed);capture_frames(bad)
        rejects(lambda: obs.check_stream(capture,'a'*32))

    # Run the production drain, mocking only kernel I/O and time. This tests
    # fairness/deadline/overflow, NOT kernel/netpoll/Windows throughput.
    drain = c[c.index('static void drain('):c.index('static int __init export_init(')]
    constants = '\n'.join(re.findall(r'^#define (?:PAYLOAD|BURST|CPU_BURST|CPU_BUFFER_BYTES|TOTAL_PAGE_BYTES) .+$',c,re.M))
    adapter = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
typedef uint64_t u64;
#define NSEC_PER_MSEC 1000000ULL
#define NSEC_PER_SEC 1000000000ULL
#define TRACE_ITER_OVERWRITE 1
#define READ_ONCE(x) (x)
struct work_struct { int dummy; };
struct ring_buffer_event { unsigned char data[128]; } record;
struct tracer { const char *name; } tracer={"nop"};
struct trace_array { int pipe_cpumask[1]; int trace_flags; struct tracer *current_trace; } a={{0},0,&tracer},*array=&a;
struct { unsigned char payload[128]; } packet;
static int buffer,drain_work,system_unbound_wq,scheduled;
static unsigned cursor,transport_error,interval_ms=1,nr_cpu_ids=16;
static u64 lost,oversized,last_stats,now,cost;
static bool stopping,room=true;
static unsigned pending[16],sent[16],width=64;
static unsigned previous=99,streak,max_streak;
static int cpumask_empty(int *p) { return !*p; }
static u64 ktime_get_ns(void) { return now+=100; }
static int queue_room(void) { return room; }
static struct ring_buffer_event *ring_buffer_consume(int b,unsigned cpu,u64 *ts,unsigned long *missed) {
    (void)b;*ts=now;*missed=0;
    if (!pending[cpu]) return 0;
    pending[cpu]--;return &record;
}
static unsigned ring_buffer_event_length(struct ring_buffer_event *p) { (void)p;return width; }
static void *ring_buffer_event_data(struct ring_buffer_event *p) { return p->data; }
static void send_packet(unsigned kind,unsigned cpu,u64 ts,unsigned size) {
    (void)kind;(void)ts;(void)size;sent[cpu]++;now+=cost;
    streak=previous==cpu ? streak+1:1;previous=cpu;
    if(streak>max_streak)max_streak=streak;
}
static void stats(void) { }
static unsigned msecs_to_jiffies(unsigned n) { return n; }
static void queue_delayed_work(int q,int *w,unsigned t) { (void)q;(void)w;(void)t;scheduled++; }
static void reset(void) {
    memset(pending,0,sizeof pending);memset(sent,0,sizeof sent);
    cursor=transport_error=scheduled=0;now=cost=lost=oversized=last_stats=0;
    stopping=false;room=true;width=64;previous=99;streak=max_streak=0;
}
'''
    main = r'''
int main(void) {
    _Static_assert(CPU_BUFFER_BYTES==1920ULL*4080, "data capacity must account for page headers");
    _Static_assert(TOTAL_PAGE_BYTES==16ULL*1920*4096, "physical data pages must fit 120MiB");
    reset();pending[14]=39936;
    for(unsigned i=0;i<100 && pending[14];i++)drain(0);
    assert(!pending[14] && sent[14]==39936 && !lost && !oversized);
    reset();for(unsigned i=0;i<16;i++)pending[i]=2496;
    drain(0);for(unsigned i=0;i<16;i++)assert(sent[i]==128);
    assert(max_streak<=128);
    for(unsigned j=0;j<100;j++)drain(0);
    for(unsigned i=0;i<16;i++)assert(!pending[i] && sent[i]==2496);
    reset();pending[0]=pending[1]=1000;cost=100000;
    drain(0);assert(now<=5200000 && sent[0]<128 && !sent[1]);
    drain(0);assert(sent[1]>0); /* next invocation starts with next CPU */
    reset();pending[0]=1;room=false;drain(0);assert(pending[0]==1 && scheduled==1);
    reset();pending[0]=1;width=129;drain(0);assert(oversized==1 && transport_error && !scheduled);
    reset();tracer.name="function_graph";drain(0);assert(transport_error && !scheduled);
    return 0;
}
'''
    source=tmp/'drain.c';exe=tmp/'drain';source.write_text(constants+'\n'+adapter+drain+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=7

    # Production upload persistence, no socket or privileged service needed.
    root=tmp/'uploads';root.mkdir()
    raw=b'R5CAP01\n';sha=hashlib.sha256(raw).hexdigest()
    got=upload.save_upload(io.BytesIO(raw),root,len(raw),sha,'a'*32)
    assert got['upload']=='SHA_MATCH' and got['coverage']=='UNVERIFIED';count+=1
    try: upload.save_upload(io.BytesIO(raw),root,len(raw),sha,'a'*32)
    except FileExistsError: count+=1
    else: raise AssertionError('prior upload overwritten')
    for nonce,body,size,expected in [('b'*32,raw,9,sha),('c'*32,raw,8,'0'*64)]:
        got=upload.save_upload(io.BytesIO(body),root,size,expected,nonce)
        assert got['upload']=='FAILED_OR_UNVERIFIED' and (root/nonce/'capture.r5o').read_bytes()==raw
        assert json.loads((root/nonce/'receipt.json').read_text())==got;count+=1
    for size in (0,upload.MAX_CAPTURE+1):
        rejects(lambda: upload.save_upload(io.BytesIO(raw),root,size,sha,'d'*32))
    config=tmp/'config.json'
    config.write_text(json.dumps(dict(listen='127.0.0.1',peer='127.0.0.2',port=8766,session='e'*32,output=str(root))))
    unit=tmp/'r41-upload.service';unit.write_text(upload.service_unit(config))
    checked=subprocess.run(['systemd-analyze','verify',str(unit)],capture_output=True,text=True)
    assert checked.returncode==0, checked.stdout+checked.stderr
    assert 'WantedBy=multi-user.target' in unit.read_text() and 'BindsTo=' not in unit.read_text();count+=1

    # Local HTTP production handler, not Windows or PM-channel throughput.
    config_data=upload.load_config(config);config_data['peer']='127.0.0.1'
    with upload.HTTPServer(('127.0.0.1',0),upload.Handler) as server:
        server.config=config_data
        thread=threading.Thread(target=server.serve_forever);thread.start()
        try:
            conn=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=5)
            conn.request('GET','/health');response=conn.getresponse()
            assert response.status==200 and json.loads(response.read())['PM']=='NOT_RUN';count+=1
            headers={'X-Content-SHA256':sha,'X-R5-Session':'e'*32}
            conn.request('POST','/observation',raw,dict(headers,**{'X-R5-Session':'f'*32}))
            response=conn.getresponse();assert response.status==400;response.read();count+=1
            conn.request('POST','/observation',raw,headers);response=conn.getresponse()
            assert response.status==200 and json.loads(response.read())['upload']=='SHA_MATCH';count+=1
            conn.request('POST','/observation',raw,headers);response=conn.getresponse()
            assert response.status==409;response.read();conn.close();count+=1
        finally:
            server.shutdown();thread.join()

assert 'trace_notifier_boundary' not in prepare.NOTIFIER_EVENT  # generated tracepoint API, not recursive
assert '__field(int, ret)' in prepare.NOTIFIER_EVENT and '__field(bool, exit)' in prepare.NOTIFIER_EVENT
assert '__field(bool, did_move)' in prepare.PROGRESS_EVENT
count += 1
print(f'r5_observation_offline_checks={count} PASS; PM=NOT_RUN R5=FAIL')
