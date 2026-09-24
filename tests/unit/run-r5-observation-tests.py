#!/usr/bin/env python3
"""Offline unit checks. Never mounts/reads tracefs or loads a module."""
import copy
import importlib.util
from pathlib import Path
import tempfile

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
    p = obs.plan(symbols)
    assert p['PM']=='NOT_RUN' and p['overwrite'] is False and p['buffer_size_kb_per_cpu']==1024
    count += 1
    symbols.write_text(symbols.read_text()+'\n0000000000010000 t rpm_resume.constprop.0\n')
    rejects(lambda: obs.plan(symbols))
    symbols.write_text('0000000000000000 t suspend_console\n')
    rejects(lambda: obs.plan(symbols))
    source = tmp/'source'; source.mkdir()
    rejects(lambda: prepare.prepare(source, source/'inside'))
    existing = tmp/'existing'; existing.mkdir()
    rejects(lambda: prepare.prepare(source, existing))
    rejects(lambda: prepare.prepare(source, tmp/'new'))
    assert not (tmp/'new').exists()
    count += 1

assert 'trace_notifier_boundary' not in prepare.NOTIFIER_EVENT  # generated tracepoint API, not recursive
assert '__field(int, ret)' in prepare.NOTIFIER_EVENT and '__field(bool, exit)' in prepare.NOTIFIER_EVENT
assert '__field(bool, did_move)' in prepare.PROGRESS_EVENT
count += 1
print(f'r5_observation_offline_checks={count} PASS; PM=NOT_RUN R5=FAIL')
