#!/usr/bin/env python3
"""Offline unit checks. Never mounts/reads tracefs or loads a module."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import subprocess
import struct

ROOT = Path(__file__).resolve().parents[2]

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

obs = load('obs', ROOT/'tools/r5-observation.py')
prepare = load('prepare', ROOT/'tools/prepare-r5-observation.py')
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
assert obs.check_records([entry, dict(exit_event, cpu=1)], losses)['notifier_pairs'] == 1
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

with tempfile.TemporaryDirectory(prefix='r5obs-unit-') as tmp:
    tmp = Path(tmp); symbols = tmp/'System.map'
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

assert 'trace_notifier_boundary' not in prepare.NOTIFIER_EVENT  # generated tracepoint API, not recursive
assert '__field(int, ret)' in prepare.NOTIFIER_EVENT and '__field(bool, exit)' in prepare.NOTIFIER_EVENT
assert '__field(bool, did_move)' in prepare.PROGRESS_EVENT
count += 1
print(f'r5_observation_offline_checks={count} PASS; PM=NOT_RUN R5=FAIL')
