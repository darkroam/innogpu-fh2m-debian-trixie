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

obs = load('obs', ROOT/'tools/internal/r5-observation.py')
prepare = load('prepare', ROOT/'tools/internal/prepare-r5-observation.py')
upload = load('upload', ROOT/'tools/internal/r5-observation-upload.py')
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
assert obs.dictionary_limits(obs.CAPACITY_PROFILE)==dict(objects=4096,edges=8192,functions=256,notifiers=64)
assert obs.semantic_contract()['max_records']==39936;count+=2
rejects(lambda: obs.dictionary_limits('unknown'))
large=dict(semantic,object_id=513,peer_id=obs.MAX_OBJECTS)
large_exit=dict(large,seq=2,phase='exit',ret=0)
assert obs.check_semantic_records([large,large_exit])['coverage']=='UNKNOWN';count+=1
assert obs.check_semantic_records([large,large_exit],obs.CAPACITY_PROFILE)['semantic_pairs']==1;count+=1
for field in ('object_id','peer_id'):
    a=dict(large,**{field:obs.MAX_OBJECTS+1});b=dict(a,seq=2,phase='exit',ret=0)
    assert obs.check_semantic_records([a,b],obs.CAPACITY_PROFILE)['coverage']=='UNKNOWN';count+=1

with tempfile.TemporaryDirectory(prefix='r5obs-unit-') as tmp:
    tmp = Path(tmp)
    # The actual CLI must not turn an UNKNOWN result into exit 0.
    evidence_file=tmp/'semantic.json'
    for records,expected_rc in (([semantic,finished],0),([semantic],1),
                               ([semantic,dict(finished,task_generation=4)],1)):
        evidence_file.write_text(json.dumps(dict(envelope,records=records)))
        checked=subprocess.run(['python3','-B',str(ROOT/'tools/internal/r5-observation.py'),
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
    for rows in (frames[1:], frames+[frames[-1]],
                 [frames[0],frames[2],frames[1],frames[3]]):
        capture_frames(rows); rejects(lambda: obs.check_stream(capture,'a'*32))
    capture_frames([frames[0], *frames[2:]])
    gap = obs.check_stream(capture,'a'*32)
    assert gap['sequence_gaps']==1 and not gap['transport_complete']; count+=1
    capture_frames(frames[:-1])
    assert not obs.check_stream(capture,'a'*32)['transport_complete']; count+=1
    capture_frames(frames[:2]+frames[3:])
    gap = obs.check_stream(capture,'a'*32)
    assert gap['sequence_gaps']==1 and not gap['transport_complete']; count+=1
    for index,offset,value in ((1,48,1),(1,56,1),(2,64,1),(2,88,1),(3,64,1)):
        rows=list(frames); changed=bytearray(rows[index]); struct.pack_into('!Q',changed,offset,value)
        rows[index]=bytes(changed); capture_frames(rows)
        assert not obs.check_stream(capture,'a'*32)['transport_complete']; count+=1

    # New transport bounds; old bytes above still use their original bounds.
    bounded = list(frames)
    changed = bytearray(bounded[0]); struct.pack_into('!4I',changed,64,1,1,2048,256)
    bounded[0] = bytes(changed); capture_frames(bounded)
    assert obs.check_stream(capture,'a'*32)['transport_complete']; count+=1
    for offset,value in ((64,17),(68,20),(72,256),(76,129)):
        bad=list(bounded); changed=bytearray(bad[0]);struct.pack_into('!I',changed,offset,value)
        bad[0]=bytes(changed);capture_frames(bad)
        rejects(lambda: obs.check_stream(capture,'a'*32))

    # Run the production drain, mocking only kernel I/O and time. This tests
    # fairness/deadline/overflow, NOT kernel/netpoll/Windows throughput.
    drain = c[c.index('static bool semantic_profile_ok('):c.index('static int __init export_init(')]
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
struct ring_buffer_event { unsigned char data[256]; } record;
struct tracer { const char *name; } tracer={"nop"};
struct trace_array { int pipe_cpumask[1]; int trace_flags; struct tracer *current_trace; int buffer_disabled,clock_id; int *tracing_cpumask; } a={{0},0,&tracer,0,5,0},*array=&a;
struct { unsigned char payload[256]; } packet;
static int buffer,drain_work,system_unbound_wq,scheduled;
static unsigned cursor,transport_error,interval_ms=1,nr_cpu_ids=16;
static u64 lost,oversized,last_stats,now,cost;
static bool stopping,room=true;
static int semantic_error;
static int r5obs2_status(void) { return semantic_error; }
static int r5obs2_emit_dictionary(void) { return 0; }
static unsigned pending[16],sent[16],width=64;
static unsigned previous=99,streak,max_streak;
static int cpumask_empty(int *p) { return !*p; }
#define EVENT_FILE_FL_ENABLED 1
#define EVENT_FILE_FL_WAS_ENABLED 2
static int *cpu_possible_mask;
static bool cpumask_equal(int *a,int *b) { return a==b; }
static struct trace_event_file { unsigned long flags; } files[3]={{1},{1},{1}},*semantic_files[]={&files[0],&files[1],&files[2]};
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
    cursor=transport_error=scheduled=semantic_error=0;files[0].flags=files[1].flags=files[2].flags=1;now=cost=lost=oversized=last_stats=0;
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
    reset();pending[0]=1;width=257;drain(0);assert(oversized==1 && transport_error && !scheduled);
    reset();files[0].flags=0;pending[0]=1;drain(0);assert(transport_error && !scheduled && pending[0]);
    reset();files[1].flags=5;drain(0);assert(transport_error && !scheduled);
    reset();semantic_error=1;pending[0]=1;drain(0);assert(transport_error && !scheduled && pending[0]);
    reset();tracer.name="function_graph";drain(0);assert(transport_error && !scheduled);
    return 0;
}
'''
    source=tmp/'drain.c';exe=tmp/'drain';source.write_text(constants+'\n'+adapter+drain+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=10

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

# Compile the production TRACE_EVENT declarations/assignments with only the
# kernel trace sink replaced by fwrite; test raw C bytes through the real CLI.
with tempfile.TemporaryDirectory(prefix='r5obs-semantic-') as tmp:
    tmp=Path(tmp)
    adapter=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
typedef uint64_t u64; typedef uint32_t u32; typedef int32_t s32;
struct trace_entry { uint16_t type; uint8_t flags,preempt; int32_t pid; };
static void strscpy(char *d,const char *s,size_t n) { assert(strlen(s)<n);strcpy(d,s); }
#define TP_PROTO(...) (__VA_ARGS__)
#define TP_ARGS(...)
#define TP_STRUCT__entry(...) __VA_ARGS__
#define __field(type,name) type name;
#define __field_struct(type,name) type name;
#define __array(type,name,n) type name[n];
#define TP_fast_assign(...) __VA_ARGS__
#define TP_printk(...)
#define TRACE_EVENT(name,proto,args,fields,assign,print) \
struct raw_##name { struct trace_entry ent; fields }; \
static void emit_##name proto { struct raw_##name row={.ent={.type=id_##name,.pid=12}}; \
struct raw_##name *__entry=&row; assign; fwrite(&row,sizeof(row),1,stdout); }
enum { id_r5_pair=101,id_r5_dictionary=102,id_r5_aux=103 };
'''
    header=prepare.SEMANTIC_HEADER.replace('#include <linux/types.h>','')
    main=r'''
int main(void) {
    _Static_assert(sizeof(struct raw_r5_pair)==80,"pair width");
    _Static_assert(sizeof(struct raw_r5_dictionary)==136,"dictionary width");
    emit_r5_dictionary(1,2,0,1,0,"child","driver");
    emit_r5_dictionary(2,0,0,1,0,"parent","bus");
    emit_r5_dictionary(2,0,0,1,2,"END_DICTIONARY","");
    struct r5obs2_token t={.object_id=1,.object_generation=1,.completion_generation=2,
        .task_id=13,.task_generation=500,.call_id=99,.peer_id=2,
        .operation=R5_WAIT,.pm_phase=R5_RESUME_PHASE};
    emit_r5_pair(&t,false,0);emit_r5_pair(&t,true,0);
    t.operation=R5_PREPARE;t.peer_id=0;t.call_id=100;
    emit_r5_pair(&t,false,0);emit_r5_pair(&t,true,0);
    emit_r5_aux(&t,1,3,R5_PROGRESS,0);
    struct r5obs2_token stage={.object_generation=1,.task_id=13,.task_generation=500,
        .call_id=101,.operation=R5_STAGE,.pm_phase=R5_PREPARE_PHASE};
    emit_r5_aux(&stage,1,3,R5_STAGE_BEGIN,0);emit_r5_aux(&stage,1,3,R5_STAGE_END,0);
    t.operation=R5_TASK;t.call_id=102;
    emit_r5_pair(&t,false,0);emit_r5_aux(&t,1,0,R5_ASYNC,0);emit_r5_pair(&t,true,0);
    return 0;
}
'''
    c=tmp/'events.c';exe=tmp/'events'
    c.write_text(adapter+header+prepare.SEMANTIC_EVENTS+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
    raw=subprocess.check_output([str(exe)])
    assert len(raw)==1272;count+=1
    dictionaries=[raw[i*136:(i+1)*136] for i in range(3)]
    pair=[raw[408:488],raw[488:568]]
    formats=[]
    for name,eid in [('r5_pair',101),('r5_dictionary',102)]:
        path=tmp/(name+'.format')
        path.write_text(f'name: {name}\nID: {eid}\n'+''.join(
            f'field:char {field}; offset:{offset}; size:{size}; signed:0;\n'
            for field,offset,size in obs.RAW_LAYOUTS[name]))
        formats.append(path)
    capture=tmp/'semantic.r5o';session='a'*32
    def semantic_capture(payloads, complete=True, loss=0):
        packets=[]
        def packet(kind,cpu,ts,payload):
            seq=len(packets)
            packets.append(obs.WIRE.pack(b'R5O1',1,kind,len(payload),bytes.fromhex(session),
                                        seq,cpu,12,ts,loss,0)+payload)
        packet(3,0xffffffff,1,struct.pack('!4I',2,1,2048,256))
        for cpu,ts,payload in payloads: packet(1,cpu,ts,payload)
        for cpu in range(2): packet(2,cpu,20,struct.pack('!4Q',0,0,0,0))
        if complete: packet(4,0xffffffff,21,struct.pack('!QII',0,0,0))
        capture.write_bytes(b'R5CAP01\n'+b''.join(obs.FRAME.pack(len(p),1)+p for p in packets))
    base=[(0,i+1,d) for i,d in enumerate(dictionaries)]
    # Exit is received first from CPU1; mono timestamps restore pair order.
    rows=base+[(1,10,pair[1]),(0,9,pair[0])]
    def checked(): return obs.semantic_wire_check(capture,session,*formats)
    semantic_capture(rows)
    assert checked()['coverage']=='SEMANTIC_RECORDS_PAIRED';count+=1
    assert checked()['raw_locations'] == [dict(seq=0,wire_seq=5,timestamp=9,cpu=0),
                                         dict(seq=1,wire_seq=4,timestamp=10,cpu=1)];count+=1
    cli=['python3','-B',str(ROOT/'tools/internal/r5-observation.py'),'semantic-wire-check',str(capture),
         '--session',session,'--pair-format',str(formats[0]),'--dictionary-format',str(formats[1])]
    assert subprocess.run(cli,capture_output=True).returncode==0;count+=1
    # Six identity conflicts, bad peer, producer invalidation, duplicate call.
    for field_index in range(6):
        broken=bytearray(pair[1]);offset=8+8*field_index
        struct.pack_into('<Q',broken,offset,struct.unpack_from('<Q',broken,offset)[0]+1)
        semantic_capture(base+[(0,9,pair[0]),(1,10,bytes(broken))])
        rc=subprocess.run(cli,capture_output=True).returncode
        assert rc==1;count+=1
    for offset,value in ((56,511),(68,1)):
        broken=bytearray(pair[1]);struct.pack_into('<I',broken,offset,value)
        semantic_capture(base+[(0,9,pair[0]),(1,10,bytes(broken))])
        assert checked()['coverage']!='SEMANTIC_RECORDS_PAIRED';count+=1
    poisoned=bytearray(pair[1]);struct.pack_into('<I',poisoned,68,1)
    semantic_capture(rows+[(1,11,bytes(poisoned))])
    assert checked()['coverage']=='UNKNOWN' and checked()['semantic_pairs']==0;count+=1
    for bad in (base+[(0,9,pair[0])],rows+[(1,11,pair[1])],rows[1:],rows[:2]+rows[3:]):
        semantic_capture(bad)
        assert checked()['coverage']!='SEMANTIC_RECORDS_PAIRED';count+=1
    for complete,loss in ((False,0),(True,1)):
        semantic_capture(rows,complete,loss)
        assert checked()['coverage']=='INCOMPLETE_WITH_LOSS';count+=1
    semantic_capture(rows)

    aux_format=tmp/'r5_aux.format'
    aux_format.write_text('name: r5_aux\nID: 103\n'+''.join(
        f'field:char {field}; offset:{offset}; size:{size}; signed:0;\n'
        for field,offset,size in obs.RAW_LAYOUTS['r5_aux']))
    chunks=[];at=568
    for size in (80,80,96,96,96,80,96,80):
        chunks.append(raw[at:at+size]);at+=size
    assert at==len(raw)
    aux_rows=base+[(0,9+i,p) for i,p in enumerate(chunks)]
    def aux_checked():return obs.semantic_wire_check(capture,session,*formats,aux_format)
    semantic_capture(aux_rows)
    result=aux_checked()
    assert result['coverage']=='SEMANTIC_RECORDS_PAIRED';count+=1
    assert result['auxiliary']['stages']['semantic_pairs']==1;count+=1
    assert result['auxiliary']['prepare_progress'][0]['did_move']==1;count+=1
    assert result['auxiliary']['async_decisions'][0]['queued']==False;count+=1
    assert len(result['transport']['cpu_counters'])==2;count+=1
    aux_cli=cli+['--aux-format',str(aux_format)]
    assert subprocess.run(aux_cli,capture_output=True).returncode==0;count+=1
    # missing progress, duplicate, wrong return/movement, stage mismatch/exit,
    # async enabled/queued contradictions all go through production raw CLI.
    for index in (2,3,4,6):
        semantic_capture(base+[(0,9+i,p) for i,p in enumerate(chunks) if i!=index])
        assert subprocess.run(aux_cli,capture_output=True).returncode==1;count+=1
    for index,offset,fmt,value in [(2,72,'<Q',2),(2,80,'<Q',5),(2,92,'<i',-11),
                                  (4,72,'<Q',2),(4,80,'<Q',4),(4,68,'<I',1),
                                  (6,72,'<Q',0),(6,80,'<Q',1),(6,92,'<i',-1)]:
        bad=list(chunks);payload=bytearray(bad[index]);struct.pack_into(fmt,payload,offset,value);bad[index]=bytes(payload)
        # enabled=0, queued=0 is valid; pair it with queued=1 for contradiction.
        if index==6 and offset==72:
            struct.pack_into('<Q',payload,80,1);bad[index]=bytes(payload)
        semantic_capture(base+[(0,9+i,p) for i,p in enumerate(bad)])
        assert subprocess.run(aux_cli,capture_output=True).returncode==1;count+=1
    bad=list(chunks)
    for index in (3,4):
        payload=bytearray(bad[index]);struct.pack_into('<Q',payload,48,100);bad[index]=bytes(payload)
    semantic_capture(base+[(0,9+i,p) for i,p in enumerate(bad)])
    assert aux_checked()['coverage']=='UNKNOWN';count+=1
    semantic_capture(aux_rows+[(0,18,chunks[2])])
    assert aux_checked()['coverage']=='UNKNOWN';count+=1
    semantic_capture(aux_rows,complete=False)
    assert aux_checked()['coverage']=='INCOMPLETE_WITH_LOSS';count+=1
    semantic_capture(aux_rows)
    rejects(checked)  # an R46 capture cannot silently use R45 two-event decoder
    # R47 production TRACE_EVENT bytes: function/notifier dictionaries, actual
    # callback metadata, queue->worker cookie, robust-return and wake snapshot.
    details=r'''
    emit_r5_dictionary(1,0,0,1,3,"callback","");
    emit_r5_dictionary(1,1,0,1,4,"PM_NOTIFIER","");
    emit_r5_dictionary(1,1,0,1,5,"DETAIL_COUNTS","");
'''
    extra=r'''
    t.operation=R5_CALLBACK;t.call_id=200;
    emit_r5_pair(&t,false,0);emit_r5_aux(&t,1,5,R5_CALLBACK_META,0);emit_r5_pair(&t,true,-5);
    t.operation=R5_TASK;t.call_id=201;
    emit_r5_pair(&t,false,0);emit_r5_aux(&t,1,1,R5_ASYNC,0);emit_r5_pair(&t,true,1);
    t.operation=R5_WORKER;t.call_id=202;
    emit_r5_pair(&t,false,0);emit_r5_aux(&t,201,27,R5_WORKER_META,0);emit_r5_pair(&t,true,0);
    stage.operation=R5_NOTIFIER;stage.call_id=203;
    emit_r5_aux(&stage,1,4,R5_NOTIFIER_BEGIN,0);emit_r5_aux(&stage,1,4,R5_NOTIFIER_END,0x8001);
    stage.operation=R5_STAGE;stage.call_id=204;
    emit_r5_aux(&stage,16,0,R5_STAGE_BEGIN,0);emit_r5_aux(&stage,19,1,R5_WAKE_META,0);
    emit_r5_aux(&stage,16,0,R5_STAGE_END,-16);
'''
    r47_main=main.replace('    emit_r5_dictionary(2,0,0,1,2,"END_DICTIONARY","");',
                          details+'    emit_r5_dictionary(2,0,0,1,2,"END_DICTIONARY","");')
    r47_main=r47_main.replace('    return 0;',extra+'    return 0;')
    c.write_text(adapter+header+prepare.SEMANTIC_EVENTS+r47_main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
    raw47=subprocess.check_output([str(exe)]);events47=[];at=0
    while at<len(raw47):
        size={101:80,102:136,103:96}[struct.unpack_from('<H',raw47,at)[0]]
        events47.append(raw47[at:at+size]);at+=size
    semantic_capture([(i%2,i+1,p) for i,p in enumerate(events47)])
    strict_cli=aux_cli+['--require-r47']
    checked47=subprocess.run(strict_cli,capture_output=True)
    assert checked47.returncode==0,checked47.stderr.decode()+checked47.stdout.decode();count+=1
    result47=json.loads(checked47.stdout)
    assert result47['auxiliary']['notifiers']['pairs'][0]['exit']['ret']==0x8001
    assert len(result47['auxiliary']['workers'])==1 and result47['detail_dictionary_complete'];count+=2
    for code,offset,fmt,value in [(5,72,'<Q',2),(8,72,'<Q',999),(7,80,'<Q',5),(9,48,'<Q',999)]:
        bad=list(events47)
        index=next(i for i,p in enumerate(bad) if len(p)==96 and struct.unpack_from('<I',p,88)[0]==code)
        row=bytearray(bad[index]);struct.pack_into(fmt,row,offset,value);bad[index]=bytes(row)
        semantic_capture([(i%2,i+1,p) for i,p in enumerate(bad)])
        assert subprocess.run(strict_cli,capture_output=True).returncode==1;count+=1
    for code in (5,8,7,9):
        bad=[p for p in events47 if not (len(p)==96 and struct.unpack_from('<I',p,88)[0]==code)]
        semantic_capture([(i%2,i+1,p) for i,p in enumerate(bad)])
        assert subprocess.run(strict_cli,capture_output=True).returncode==1;count+=1
    semantic_capture(aux_rows)
    assert subprocess.run(strict_cli,capture_output=True).returncode==1;count+=1
    # New profile has metadata in the real entry; no pair-entry is discarded
    # during analysis. Historical expanded-profile bytes remain unchanged.
    compact_main=r47_main.replace('DETAIL_COUNTS"','DETAIL_COUNTS_R40"')
    compact_main=compact_main.replace('emit_r5_pair(&t,false,0);emit_r5_aux(&t,1,5,R5_CALLBACK_META,0);',
                                    'emit_r5_aux(&t,1,5,R5_CALLBACK_ENTRY,0);')
    compact_main=compact_main.replace('emit_r5_pair(&t,false,0);emit_r5_aux(&t,201,27,R5_WORKER_META,0);',
                                    'emit_r5_aux(&t,201,27,R5_WORKER_ENTRY,0);')
    c.write_text(adapter+header+prepare.SEMANTIC_EVENTS+compact_main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
    raw_compact=subprocess.check_output([str(exe)]);compact=[];at=0
    while at<len(raw_compact):
        size={101:80,102:136,103:96}[struct.unpack_from('<H',raw_compact,at)[0]]
        compact.append(raw_compact[at:at+size]);at+=size
    semantic_capture([(i%2,i+1,p) for i,p in enumerate(compact)])
    checked_compact=subprocess.run(strict_cli,capture_output=True)
    assert checked_compact.returncode==0,checked_compact.stderr.decode();count+=1
    result_compact=json.loads(checked_compact.stdout)
    assert result_compact['semantic_pairs']==result47['semantic_pairs'] and len(result_compact['fused_entry_wire_sequences'])==2
    assert len(events47)-len(compact)==2;count+=2
    for code in (10,11):
        bad=[p for p in compact if not (len(p)==96 and struct.unpack_from('<I',p,88)[0]==code)]
        semantic_capture([(i%2,i+1,p) for i,p in enumerate(bad)])
        assert subprocess.run(strict_cli,capture_output=True).returncode==1;count+=1
    # New producer marker, genuine TRACE_EVENT layouts, and all boundary IDs
    # traverse the same strict CLI. Legacy evidence never inherits new limits.
    for n in (513,obs.MAX_OBJECTS,obs.MAX_OBJECTS+1):
        expanded=compact_main.replace('DETAIL_COUNTS_R40"','DETAIL_COUNTS_R40_V2"')
        expanded=expanded.replace('    emit_r5_dictionary(1,2,0,1,0,"child","driver");',
            f'    for (unsigned i=1;i<={n};i++) emit_r5_dictionary(i,i==1?{n}:0,0,1,0,"node","driver");')
        expanded=expanded.replace('    emit_r5_dictionary(2,0,0,1,0,"parent","bus");','')
        expanded=expanded.replace('emit_r5_dictionary(2,0,0,1,2,',f'emit_r5_dictionary({n},0,0,1,2,')
        expanded=expanded.replace('.peer_id=2,',f'.peer_id={n},')
        c.write_text(adapter+header+prepare.SEMANTIC_EVENTS+expanded)
        subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(c),'-o',str(exe)],check=True)
        large_raw=subprocess.check_output([str(exe)]);payloads=[];at=0
        while at<len(large_raw):
            size={101:80,102:136,103:96}[struct.unpack_from('<H',large_raw,at)[0]]
            payloads.append(large_raw[at:at+size]);at+=size
        semantic_capture([(i%2,i+1,p) for i,p in enumerate(payloads)])
        verified=subprocess.run(strict_cli,capture_output=True)
        if n>obs.MAX_OBJECTS:
            assert verified.returncode!=0;count+=1
            continue
        assert verified.returncode==0,verified.stderr.decode()
        assert len(json.loads(verified.stdout)['objects'])==n;count+=1
        for marker in ('DETAIL_COUNTS','DETAIL_COUNTS_R40','UNKNOWN_PROFILE'):
            downgraded=[p.replace(b'DETAIL_COUNTS_R40_V2'.ljust(48,b'\0'),marker.encode().ljust(48,b'\0'))
                        for p in payloads]
            semantic_capture([(i%2,i+1,p) for i,p in enumerate(downgraded)])
            assert subprocess.run(strict_cli,capture_output=True).returncode!=0;count+=1
        for complete,loss in ((False,0),(True,1)):
            semantic_capture([(i%2,i+1,p) for i,p in enumerate(payloads)],complete,loss)
            assert subprocess.run(strict_cli,capture_output=True).returncode==1;count+=1
    formats[0].write_text(formats[0].read_text().replace('offset:72','offset:73'))
    rejects(checked)



# Execute the production boundary/generation helpers with primitive counters.
# This proves return/complete semantics in the adapter, not kernel concurrency.
with tempfile.TemporaryDirectory(prefix='r5obs-emitter-') as tmp:
    tmp=Path(tmp)
    adapter=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
typedef uint64_t u64; typedef uint32_t u32;
typedef long atomic_t; typedef long atomic64_t;
#define atomic_read(p) (*(p))
#define atomic_set(p,v) (*(p)=(v))
#define atomic_inc_return(p) (++*(p))
#define atomic64_read atomic_read
#define atomic64_inc_return atomic_inc_return
static long atomic_cmpxchg(long *p,long old,long new) { long v=*p;if(v==old)*p=new;return v; }
#define EXPORT_SYMBOL_GPL(x)
struct device { struct { int completion; } power; } dev;
static struct { u64 start_boottime; int pid; } task={100,12},*current=&task;
#define task_pid_nr(t) ((t)->pid)
static struct { int event; } pm_transition;
enum { PM_EVENT_RESUME,PM_EVENT_RECOVER,PM_EVENT_THAW,PM_EVENT_RESTORE };
static struct { struct device *dev; atomic64_t generation; atomic_t resetting,prepares; } r5_objects[1];
static unsigned r5_n=1;
static atomic_t r5_state=2,r5_exported=1,r5_records;
static bool r5_ordinary=true;
#define READ_ONCE(x) (x)
static atomic64_t r5_lifetime=1,r5_calls;
static int completed,reinitialized;
static void complete_all(int *p) { (void)p;completed++; }
static void reinit_completion(int *p) { (void)p;reinitialized++; }
'''
    header=prepare.SEMANTIC_HEADER.replace('#include <linux/types.h>','')
    trace=r'''
static struct r5obs2_token saved[32];static unsigned emitted;
static unsigned aux_count;
static struct r5obs2_token aux_token;
static void trace_r5_aux(const struct r5obs2_token *t,u64 a,u64 b,u32 code,int ret) {
    (void)a;(void)b;(void)code;(void)ret;aux_count++;aux_token=*t;
}
static void trace_r5_pair(const struct r5obs2_token *t,bool exit,int ret) {
    (void)exit;(void)ret;assert(emitted<32);saved[emitted++]=*t;
}
'''
    emitter=prepare.SEMANTIC_EMITTER
    changes=emitter[emitter.index('void r5obs2_topology_changed('):emitter.index('/* Called only while census')]
    helpers=emitter[emitter.index('static struct r5obs2_token r5_begin('):]
    main=r'''
int main(void) {
    r5_objects[0].dev=&dev;r5_objects[0].generation=1;
    struct r5obs2_token t=r5_begin(&dev,R5_WAIT,R5_RESUME_PHASE,NULL);
    r5_end(&t,0);assert(emitted==2 && saved[0].call_id==saved[1].call_id);
    r5_reinit(&dev);assert(reinitialized==1 && emitted==4);
    assert(saved[2].completion_generation==2 && !saved[3].flags);
    r5_complete(&dev);assert(completed==1 && emitted==6);
    t=r5_begin(&dev,R5_WAIT,R5_RESUME_PHASE,NULL);
    r5_objects[0].generation++;r5_end(&t,0);assert(saved[7].flags==1);
    r5_objects[0].resetting=1;r5_reinit(&dev);
    assert(reinitialized==2 && r5_state==3); /* no PM behavior hidden */
    r5_complete(&dev);assert(completed==2);
    r5_state=2;r5_objects[0].resetting=0;r5_objects[0].generation=7;
    r5_reinit(&dev);assert(r5_state==3 && reinitialized==3);
    r5_state=2;r5_objects[0].prepares=4;
    t=r5_begin(&dev,R5_PREPARE,R5_PREPARE_PHASE,NULL);assert(!t.object_id && r5_state==3);
    r5_state=2;r5_records=39936;
    t=r5_begin(&dev,R5_WAIT,R5_RESUME_PHASE,NULL);assert(!t.object_id && r5_state==3);
    r5_state=2;r5_records=0;struct device unknown={0};
    t=r5_begin(&unknown,R5_WAIT,R5_RESUME_PHASE,NULL);assert(!t.object_id && r5_state==3);
    t=r5obs2_stage_begin(1,3);assert(!t.call_id && !aux_count);
    r5_state=2;r5_records=0;t=r5obs2_stage_begin(1,3);
    assert(t.call_id && !t.object_id && aux_count==1 && aux_token.operation==R5_STAGE);
    r5obs2_stage_end(&t,1,3,-5);assert(aux_count==2 && aux_token.call_id==t.call_id);
    r5_ordinary=false;unsigned before=emitted;
    t=r5_begin(&dev,R5_WAIT,R5_RESUME_PHASE,NULL);assert(!t.call_id);
    t=r5_begin(&dev,R5_CALLBACK,R5_SUSPEND_PHASE,NULL);assert(!t.call_id);
    r5_complete(&dev);assert(completed==3 && emitted==before+2);
    r5_ordinary=true;t=r5_begin(&dev,R5_CALLBACK,R5_RESUME_PHASE,NULL);
    assert(t.call_id && emitted==before+2); /* aux carries entry; pair exit remains */
    return 0;
}
'''
    source=tmp/'emitter.c';exe=tmp/'emitter'
    source.write_text('#define CONFIG_PM_SLEEP 1\n'+adapter+header+trace+changes+helpers+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=10

# R47 admission failures: compile the production guards, not a replacement checker.
with tempfile.TemporaryDirectory() as temporary:
    tmp=Path(temporary); c=prepare.EXPORT_MODULE
    constants='\n'.join(re.findall(r'^#define (?:CPU_BUFFER_BYTES|TOTAL_PAGE_BYTES) .+$',c,re.M))
    guards=c[c.index('static int geometry_reject('):c.index('static bool semantic_profile_ok(')]
    shim=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uint64_t u64;
#define PAGE_SIZE 4096
#define CONFIG_TRACER_MAX_TRACE 1
static char report[256];
#define pr_err(...) snprintf(report,sizeof report,__VA_ARGS__)
struct trace_buffer { int subbuf; unsigned long sizes[17]; } primary, secondary;
static struct trace_buffer *buffer=&primary;
static struct { int allocated_snapshot; struct { struct trace_buffer *buffer; } max_buffer; } a={0,{&secondary}},*array=&a;
static unsigned cpus=16;
#define for_each_possible_cpu(cpu) for((cpu)=0;(unsigned)(cpu)<cpus;(cpu)++)
static int ring_buffer_subbuf_size_get(struct trace_buffer *b) { return b->subbuf; }
static unsigned long ring_buffer_size(struct trace_buffer *b,int cpu) { return b->sizes[cpu]; }
static void reset(void) {
    cpus=16;array->allocated_snapshot=0;report[0]=0;primary.subbuf=secondary.subbuf=4096;
    for(unsigned i=0;i<17;i++) {primary.sizes[i]=CPU_BUFFER_BYTES;secondary.sizes[i]=2*(4096-16);}
}
'''
    main=r'''
int main(void) {
    reset();assert(check_buffer_geometry()==0 && !report[0]);
    primary.subbuf=8192;assert(check_buffer_geometry()==-E2BIG && strstr(report,"primary-subbuf"));
    reset();array->allocated_snapshot=1;assert(check_buffer_geometry()==-E2BIG && strstr(report,"snapshot-active"));
    reset();primary.sizes[15]++;assert(check_buffer_geometry()==-E2BIG && strstr(report,"primary-size cpu=15"));
    reset();secondary.subbuf=8192;assert(check_buffer_geometry()==-E2BIG && strstr(report,"snapshot-subbuf"));
    reset();secondary.sizes[15]++;assert(check_buffer_geometry()==-E2BIG && strstr(report,"snapshot-size cpu=15"));
    reset();cpus=0;assert(check_buffer_geometry()==-E2BIG && strstr(report,"total-pages"));
    reset();cpus=17;assert(check_buffer_geometry()==-E2BIG && strstr(report,"total-pages"));
    return 0;
}
'''
    source=tmp/'admission.c';exe=tmp/'admission';source.write_text(constants+'\n'+shim+guards+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=1
    emitter=prepare.SEMANTIC_EMITTER
    latch=emitter[emitter.index('static const char *r5_reject_gate'):emitter.index('void r5obs2_topology_changed(')]
    functions=prepare.DETAIL_TABLES
    functions=functions[functions.index('static unsigned int r5_function('):functions.index('static int r5_ops_add(')]
    shim=r'''
#include <assert.h>
#include <errno.h>
#include <string.h>
#define KSYM_SYMBOL_LEN 256
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
static struct {void *fn;char name[48];} r5_functions[256];
static unsigned r5_nf;
static char symbol_text[KSYM_SYMBOL_LEN];
static void sprint_symbol_no_offset(char *out,unsigned long fn) {(void)fn;strcpy(out,symbol_text);}
static long strscpy(char *out,const char *in,unsigned long cap) {
    if(strlen(in)>=cap)return -E2BIG;
    strcpy(out,in);return strlen(in);
}
'''
    main=r'''
int main(void) {
    assert(r5_function_add(0)==0);
    memset(symbol_text,'x',47);symbol_text[47]=0;
    assert(r5_function_add((void *)1)==1 && r5_function_add((void *)1)==1);
    symbol_text[47]='x';symbol_text[48]=0;
    assert(r5_function_add((void *)2)==-E2BIG && r5_nf==1);
    assert(!strcmp(r5_reject_gate,"function-name") && r5_reject_value==49 && r5_reject_limit==48);
    r5_nf=256;assert(r5_function_add((void *)2)==-E2BIG);
    assert(!strcmp(r5_reject_gate,"functions") && r5_reject_value==257 && r5_reject_limit==256);
    assert(r5_size_error("objects",513,512)==-E2BIG && r5_reject_value==513);
    assert(r5_size_error("driver-name-copy",-E2BIG,24)==-E2BIG && r5_reject_value==-E2BIG);
    return 0;
}
'''
    source.write_text(shim+latch+functions+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=1

# Actual r5_add/r5_find and table declarations, with primitive device refs.
# The sanitizer covers the queued-call array indexed by the enlarged IDs.
with tempfile.TemporaryDirectory(prefix='r47-capacity-') as temporary:
    tmp=Path(temporary);emitter=prepare.SEMANTIC_EMITTER
    constants='\n'.join(re.findall(r'^#define R5_\w+ .+$',emitter,re.M))
    tables=emitter[emitter.index('struct r5_object {'):emitter.index('static unsigned int r5_n, r5_e;')]
    details=prepare.DETAIL_TABLES
    details=details[details.index('static struct { void *fn;'):details.index('static unsigned int r5_nf, r5_nn;')]
    helpers=emitter[emitter.index('static unsigned int r5_find('):emitter.index('int r5obs2_census(void)')]
    latch=emitter[emitter.index('static const char *r5_reject_gate'):emitter.index('void r5obs2_topology_changed(')]
    shim=r'''
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
typedef uint32_t u32;typedef uint64_t u64;
typedef struct {int counter;} atomic_t;
typedef struct {long counter;} atomic64_t;
#define atomic_set(p,v) ((p)->counter=(v))
#define atomic64_set atomic_set
struct device {unsigned refs;};
static struct device *get_device(struct device *p) {p->refs++;return p;}
static unsigned r5_n;
'''
    main=r'''
int main(void) {
    static struct device devices[R5_OBJECTS+1];
    assert(sizeof(r5_objects[0])==136 && sizeof(r5_edges[0])==8);
    assert(sizeof(r5_functions[0])==56 && sizeof(r5_notifiers[0])==16);
    assert(sizeof(r5_queued_call)==R5_OBJECTS*8);
    assert(sizeof(r5_objects)+sizeof(r5_edges)+sizeof(r5_queued_call)+
           sizeof(r5_functions)+sizeof(r5_notifiers)+R5_DICTIONARY_RESERVE<=R5_DICTIONARY_CAP);
    assert(r5_add(NULL)==0);
    for(unsigned i=0;i<R5_OBJECTS;i++) {
        int id=r5_add(&devices[i]);assert(id==(int)i+1);
        r5_queued_call[id-1]=id;assert(devices[i].refs==1);
        assert(r5_add(&devices[i])==id && devices[i].refs==1);
    }
    assert(r5_find(&devices[512])==513 && r5_find(&devices[R5_OBJECTS-1])==R5_OBJECTS);
    assert(r5_queued_call[R5_OBJECTS-1]==R5_OBJECTS);
    assert(r5_add(&devices[R5_OBJECTS])==-E2BIG && r5_n==R5_OBJECTS);
    assert(!devices[R5_OBJECTS].refs && !strcmp(r5_reject_gate,"objects"));
    assert(r5_reject_value==R5_OBJECTS+1 && r5_reject_limit==R5_OBJECTS);
    return 0;
}
'''
    source=tmp/'capacity.c';exe=tmp/'capacity'
    source.write_text(constants+'\n'+shim+tables+details+latch+helpers+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror','-fsanitize=address,undefined',
                    str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True);count+=1

# R47: production journal replay, shared slab lifetime and PFN reuse. These
# bytes test the interface; they are never recorded as target measurements.
with tempfile.TemporaryDirectory() as temporary:
    tmp=Path(temporary); path=tmp/'meter.bin';rows=[]
    # Compile the actual bounded journal writer, with only kernel I/O supplied
    # by deterministic stand-ins. Never substitute a second writer algorithm.
    c=prepare.METER_SOURCE
    structure=c[c.index('struct r5m_record {'):c.index('static struct r5m_record r5m_log')]
    append=c[c.index('static void r5m_append('):c.index('static void r5m_emit(')]
    shim='''#include <stdint.h>
#include <assert.h>
typedef uint64_t u64; typedef uint32_t u32;
#define R5M_SLOTS 8192
'''+structure+'''
static struct r5m_record r5m_log[R5M_SLOTS];
static u64 r5m_head,r5m_tail,r5m_seq,r5m_lost,clock_ns;
static unsigned r5m_phase;
static struct {u64 start_boottime;} task={7},*current=&task;
#define atomic64_inc(p) (++*(p))
#define task_pid_nr(p) 23
static u64 ktime_get_mono_fast_ns(void) {return ++clock_ns;}
'''+append+'''
int main(void) {
 _Static_assert(sizeof(struct r5m_record)==88,"meter ABI");
 struct r5m_record r={.kind=1};
 for(unsigned i=0;i<R5M_SLOTS;i++)r5m_append(&r);
 r5m_append(&r); assert(r5m_head==8192 && r5m_seq==8193 && r5m_lost==1);
 assert(r5m_log[0].seq==1 && r5m_log[8191].seq==8192);
 r5m_tail++;r5m_phase=4;r5m_append(&r);
 assert(r5m_log[0].seq==8194 && r5m_log[0].phase==4 && r5m_log[0].birth==7);
 return 0;
}
'''
    (tmp/'meter.c').write_text(shim)
    subprocess.run(['cc','-std=gnu11','-Wall','-Werror',str(tmp/'meter.c'),'-o',str(tmp/'meter')],check=True)
    subprocess.run([str(tmp/'meter')],check=True);count+=1
    def meter(kind,phase,**kw):
        row=dict.fromkeys(obs.METER_FIELDS,0)
        row.update(seq=len(rows)+1,ns=len(rows)+1,kind=kind,phase=phase,**kw)
        rows.append(row)
    def mark(phase):
        meter(0,phase,ptr=0x52354d31,site=16,requested=8192*88,allocated=8192*88,order=12)
    def save(data):
        path.write_bytes(b''.join(obs.METER.pack(*(r[k] for k in obs.METER_FIELDS)) for r in data))
    mark(0);mark(1)
    meter(3,1,pfn=100)
    meter(1,1,ptr=409600,pfn=100,requested=64,allocated=64)
    meter(1,1,ptr=409664,pfn=100,requested=64,allocated=64)
    meter(2,1,ptr=409600);mark(2)
    meter(2,2,ptr=409664);mark(3)
    meter(4,3,pfn=100);meter(3,3,pfn=100);mark(4)
    meter(4,4,pfn=100);mark(5);mark(6);mark(7);save(rows)
    checked=obs.meter_check(path)
    assert checked['marks'][2]['backing_bytes']==4096 and checked['marks'][2]['objects_live']==1
    assert checked['marks'][3]['backing_bytes']==4096 and checked['marks'][3]['objects_live']==0
    assert checked['final']['backing_bytes']==0 and checked['final']['physical_peak_bytes']==4096
    assert checked['admission']=='ALLOCATOR_UNVERIFIED';count+=4
    rc=subprocess.run(['python3',str(ROOT/'tools/internal/r5-observation.py'),'meter-check',str(path)],stdout=subprocess.PIPE)
    assert rc.returncode==1 and json.loads(rc.stdout)['admission']=='ALLOCATOR_UNVERIFIED';count+=1
    bad=copy.deepcopy(rows);bad[-1]['extra']=1;save(bad)
    assert obs.meter_check(path)['journal']=='INCOMPLETE_WITH_LOSS';count+=1
    save(rows[:-1]);assert obs.meter_check(path)['issues']['missing_M0_M6_or_END'];count+=1
    bad=copy.deepcopy(rows);bad[4]['ptr']=409600;save(bad);rejects(lambda:obs.meter_check(path))
    bad=copy.deepcopy(rows);bad[9]['kind']=3;save(bad);rejects(lambda:obs.meter_check(path))
    save(rows);path.write_bytes(path.read_bytes()[:-1]);rejects(lambda:obs.meter_check(path))
    bounds=[dict(class_='objects',L=1,R=2,C=2,B=4096,source_sha256='a'*64,unit='objects')]
    bounds[0]['class']=bounds[0].pop('class_')
    assert obs.check_bounds(bounds,4096,0)['status']=='RETURN_TO_DESIGN';count+=1
    bounds[0]['C']=None
    assert obs.check_bounds(bounds,4096,0)['U_total'] is None;count+=1
    assert obs.event_budget({})['total'] is None;count+=1
    envelope=dict(N=512,E=1024,G=6,R=4,C=16,functions=256,notifiers=64,stage_calls=56,wake_calls=1,external_wait_calls=0)
    b=obs.event_budget(envelope)
    assert b['status']=='RETURN_TO_DESIGN' and b['total']>39936 and b['remaining']<0;count+=1
    compact_budget=obs.event_budget(dict(envelope,profile='R40_ORDINARY_RESUME'))
    assert compact_budget['total']==38707 and compact_budget['remaining']==1229
    assert compact_budget['status']=='CENSUS_UNVERIFIED';count+=2
    expanded=dict(envelope,N=513,profile=obs.CAPACITY_PROFILE)
    assert obs.event_budget(expanded)['status']=='CENSUS_UNVERIFIED';count+=1
    rejects(lambda: obs.event_budget(dict(expanded,profile='R40_ORDINARY_RESUME')))
    rejects(lambda: obs.event_budget(dict(expanded,N=obs.MAX_OBJECTS+1)))
    rejects(lambda: obs.event_budget(dict(expanded,E=obs.MAX_EDGES+1)))
    enlarged=obs.event_budget(dict(expanded,N=obs.MAX_OBJECTS,E=obs.MAX_EDGES))
    assert enlarged['status']=='RETURN_TO_DESIGN' and enlarged['remaining']<0;count+=1
    envelope['N']=True;rejects(lambda:obs.event_budget(envelope))

# Run the exact production bulk hooks: trace before pointer permutation,
# skip disabled tracing, and emit only the returned successful allocation count.
with tempfile.TemporaryDirectory(prefix='r47-bulk-') as temporary:
    tmp=Path(temporary)
    code='''#include <stddef.h>
#include <assert.h>
struct kmem_cache { int unused; };
static int enabled=1,n; static void *seen[4];
#define _RET_IP_ 0
#define NUMA_NO_NODE -1
#define trace_kfree_enabled() enabled
#define trace_kmem_cache_alloc_enabled() enabled
#define trace_kfree(site,p) (seen[n++]=(p))
#define trace_kmem_cache_alloc(site,p,s,f,node) (seen[n++]=(p))
static void release(size_t size,void **p) {
'''+prepare.BULK_FREE_TRACE+'''
 if(size)p[0]=NULL;
}
static void allocated(int i,void **p) {
'''+prepare.BULK_ALLOC_TRACE+'''
}
int main(void) {
 int a,b; void *p[]={&a,&b};
 release(2,p); assert(n==2 && seen[0]==&a && seen[1]==&b && !p[0]);
 n=0;enabled=0;release(2,p);assert(!n);
 enabled=1;allocated(1,p);assert(n==1 && seen[0]==NULL);
 n=0;allocated(0,p);release(0,p);assert(!n);return 0;
}
'''
    (tmp/'bulk.c').write_text(code)
    subprocess.run(['cc','-Wall','-Werror',str(tmp/'bulk.c'),'-o',str(tmp/'bulk')],check=True)
    subprocess.run([str(tmp/'bulk')],check=True);count+=1

# Control EOF after END must not discard buffered kernel journal bytes.
from unittest.mock import patch
import os
import types
with patch('os.uname',return_value=types.SimpleNamespace(release='6.12.101-r5obs2-r47e')), \
     patch('os.open') as device_open:
    rejects(lambda: obs.meter_session(Path('/unused-old-boot-journal')))
    device_open.assert_not_called()
with tempfile.TemporaryDirectory(prefix='r47-reader-') as temporary:
    target=Path(temporary)/'journal';fakefd=-987;real_open=os.open;real_close=os.close
    payload=b'tail after END';chunks=iter([None]*7+[payload,b''])
    def device_read(fd,size):
        assert fd==fakefd
        data=next(chunks)
        if data is None:raise BlockingIOError()
        return data
    def opened(path,flags,*args):
        return fakefd if path=='/dev/r5_meter' else real_open(path,flags,*args)
    with patch('os.uname',return_value=types.SimpleNamespace(release=obs.KERNEL_RELEASE)), \
         patch('os.open',side_effect=opened), patch('os.read',side_effect=device_read), \
         patch('os.close',side_effect=lambda fd:None if fd==fakefd else real_close(fd)), \
         patch('fcntl.ioctl') as control, patch('sys.stdin',io.StringIO('M1\nM2\nM3\nM4\nM5\nM6\nEND\n')), \
         patch('sys.stdout',io.StringIO()), patch('select.select',side_effect=lambda readers,*args:(readers,[],[])), \
         patch.object(obs,'meter_check',return_value={'admission':'ALLOCATOR_UNVERIFIED'}):
        assert obs.meter_session(target)['admission']=='ALLOCATOR_UNVERIFIED'
        assert [c.args[1] for c in control.call_args_list]==list(range(0x523500,0x523508))
    assert target.read_bytes()==payload;count+=1

# A blocked disk sync must not block production device reads. No real meter opens.
with tempfile.TemporaryDirectory(prefix='r47-reader-sync-') as temporary:
    target=Path(temporary)/'journal';fakefd=-987;real_open=os.open;real_close=os.close
    syncing=threading.Event();released=threading.Event();reads=0;controls=0
    payload=b'x'*(obs.METER.size*16)
    def opened(path,flags,*args):
        return fakefd if path=='/dev/r5_meter' else real_open(path,flags,*args)
    def ioctl(fd,cmd,arg):
        global controls
        assert fd==fakefd and cmd==0x523500+controls and not arg
        controls+=1
    def device_read(fd,size):
        global reads
        assert fd==fakefd and size==len(payload)
        if controls<8:raise BlockingIOError()
        if reads==64:assert syncing.wait(2),'sync and device reads still serialized'
        if reads==128:released.set();return b''
        reads+=1
        return payload
    def slow_sync(fd):
        syncing.set()
        assert released.wait(2),'reader did not advance during disk sync'
    with patch('os.uname',return_value=types.SimpleNamespace(release=obs.KERNEL_RELEASE)), \
         patch('os.open',side_effect=opened),patch('os.read',side_effect=device_read), \
         patch('os.close',side_effect=lambda fd:None if fd==fakefd else real_close(fd)), \
         patch('fcntl.ioctl',side_effect=ioctl),patch('os.fsync',side_effect=slow_sync), \
         patch('sys.stdin',io.StringIO('M1\nM2\nM3\nM4\nM5\nM6\nEND\n')), \
         patch('sys.stdout',io.StringIO()),patch('select.select',side_effect=lambda readers,*args:(readers,[],[])), \
         patch.object(obs,'meter_check',return_value={'admission':'ALLOCATOR_UNVERIFIED'}):
        assert obs.meter_session(target)['admission']=='ALLOCATOR_UNVERIFIED'
    assert reads==128 and controls==8 and target.read_bytes()==payload*128;count+=1

# A writer error must propagate and keep the partial file, not sign a replay.
with tempfile.TemporaryDirectory(prefix='r47-reader-error-') as temporary:
    target=Path(temporary)/'journal';fakefd=-987;real_open=os.open;real_close=os.close
    chunks=iter([None]*7+[b'partial',b''])
    def device_read(fd,size):
        assert fd==fakefd
        data=next(chunks)
        if data is None:raise BlockingIOError()
        return data
    def opened(path,flags,*args):
        return fakefd if path=='/dev/r5_meter' else real_open(path,flags,*args)
    with patch('os.uname',return_value=types.SimpleNamespace(release=obs.KERNEL_RELEASE)), \
         patch('os.open',side_effect=opened),patch('os.read',side_effect=device_read), \
         patch('os.close',side_effect=lambda fd:None if fd==fakefd else real_close(fd)), \
         patch('fcntl.ioctl'),patch('os.fsync',side_effect=OSError('injected storage failure')), \
         patch('sys.stdin',io.StringIO('M1\nM2\nM3\nM4\nM5\nM6\nEND\n')), \
         patch('sys.stdout',io.StringIO()),patch('select.select',side_effect=lambda readers,*args:(readers,[],[])), \
         patch.object(obs,'meter_check') as replay:
        try:obs.meter_session(target)
        except OSError as exc:assert 'writer failed' in str(exc)
        else:raise AssertionError('writer failure must reject')
        replay.assert_not_called()
    assert target.read_bytes()==b'partial';count+=1

# Bounded backpressure must reject, not silently drop or allocate without limit.
with tempfile.TemporaryDirectory(prefix='r47-reader-full-') as temporary:
    import time
    target=Path(temporary)/'journal';fakefd=-987;real_open=os.open;real_close=os.close
    syncing=threading.Event();reads=0;controls=0;payload=b'x'*(obs.METER.size*16)
    def opened(path,flags,*args):
        return fakefd if path=='/dev/r5_meter' else real_open(path,flags,*args)
    def ioctl(fd,cmd,arg):
        global controls
        assert fd==fakefd and cmd==0x523500+controls and not arg
        controls+=1
    def device_read(fd,size):
        global reads
        assert fd==fakefd and size==len(payload)
        if controls<8:raise BlockingIOError()
        if reads==64:assert syncing.wait(2)
        reads+=1
        assert reads<=(obs.METER_QUEUE_BLOCKS+2)*64,'queue exceeded its bound'
        return payload
    def slow_sync(fd):
        if not syncing.is_set():syncing.set();time.sleep(0.5)
    with patch('os.uname',return_value=types.SimpleNamespace(release=obs.KERNEL_RELEASE)), \
         patch('os.open',side_effect=opened),patch('os.read',side_effect=device_read), \
         patch('os.close',side_effect=lambda fd:None if fd==fakefd else real_close(fd)), \
         patch('fcntl.ioctl',side_effect=ioctl),patch('os.fsync',side_effect=slow_sync), \
         patch('sys.stdin',io.StringIO('M1\nM2\nM3\nM4\nM5\nM6\nEND\n')), \
         patch('sys.stdout',io.StringIO()),patch('select.select',side_effect=lambda readers,*args:(readers,[],[])), \
         patch.object(obs,'meter_check') as replay:
        rejects(lambda:obs.meter_session(target));replay.assert_not_called()
    assert reads<=(obs.METER_QUEUE_BLOCKS+2)*64 and 0<target.stat().st_size<reads*len(payload);count+=1

assert 'trace_notifier_boundary' not in prepare.NOTIFIER_EVENT  # generated tracepoint API, not recursive
assert '__field(int, ret)' in prepare.NOTIFIER_EVENT and '__field(bool, exit)' in prepare.NOTIFIER_EVENT
assert '__field(bool, did_move)' in prepare.PROGRESS_EVENT
count += 1
print(f'r5_observation_offline_checks={count} PASS; PM=NOT_RUN R5=FAIL')
