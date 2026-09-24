#!/usr/bin/env python3
"""R34 offline-only source overlay; no build, mount, install, probe or PM."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

LOCK = {
    'kernel/notifier.c': '1f4a1b2384a8751d780d39a748ef5beda21149f6e715af54df131ff9add2c44e',
    'include/trace/events/notifier.h': 'f783594df1c564fd8fa361a3289639963b663122729af6c902d1de080bf587c0',
    'drivers/base/power/main.c': 'cff97ce795d5f2fd27e2fe9e19c2c04bbf695932aaf382587c46559c5bf12464',
    'include/trace/events/power.h': '1b77bcc77dc67445fd29a676f9c2a62b63b2d69813bd4e35f8a11763c0d46756',
    'Makefile': '7eab4d9db58c3920f25067c2f0478878289eb70e511005cf0b1ae023a91a4dfe',
}

NOTIFIER_EVENT = r'''
/* R34: both sides of the actual indirect call, including robust rollback. */
TRACE_EVENT(notifier_boundary,
    TP_PROTO(void *cb, void *nb, unsigned long action, bool exit, int ret),
    TP_ARGS(cb, nb, action, exit, ret),
    TP_STRUCT__entry(
        __field(void *, cb)
        __field(void *, nb)
        __field(unsigned long, action)
        __field(bool, exit)
        __field(int, ret)
    ),
    TP_fast_assign(
        __entry->cb = cb;
        __entry->nb = nb;
        __entry->action = action;
        __entry->exit = exit;
        __entry->ret = ret;
    ),
    TP_printk("cb=%ps nb=%p action=%lu exit=%u ret=%d",
        __entry->cb, __entry->nb, __entry->action, __entry->exit, __entry->ret)
);
'''

PROGRESS_EVENT = r'''
/* R34: actual list movement, not an inference from a successful callback. */
TRACE_EVENT(r5_prepare_progress,
    TP_PROTO(struct device *dev, unsigned long attempt,
             unsigned long moved, bool did_move, int ret),
    TP_ARGS(dev, attempt, moved, did_move, ret),
    TP_STRUCT__entry(
        __string(device, dev_name(dev))
        __field(unsigned long, attempt)
        __field(unsigned long, moved)
        __field(bool, did_move)
        __field(int, ret)
    ),
    TP_fast_assign(
        __assign_str(device);
        __entry->attempt = attempt;
        __entry->moved = moved;
        __entry->did_move = did_move;
        __entry->ret = ret;
    ),
    TP_printk("device=%s attempt=%lu moved=%lu did_move=%u ret=%d",
        __get_str(device), __entry->attempt, __entry->moved,
        __entry->did_move, __entry->ret)
);
'''

def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('non-unique or missing source anchor')
    return text.replace(before, after)

def overlay(files):
    """Pure transformation; the production path and tests share this function."""
    result = dict(files)
    p = 'include/trace/events/notifier.h'
    result[p] = replace_once(files[p], '#endif /* _TRACE_NOTIFIERS_H */',
                             NOTIFIER_EVENT + '\n#endif /* _TRACE_NOTIFIERS_H */')
    p = 'kernel/notifier.c'
    t = replace_once(files[p], '\tstruct notifier_block *nb, *next_nb;',
                     '\tstruct notifier_block *nb, *next_nb;\n\tvoid *r5_cb;')
    t = replace_once(t, '\t\tret = nb->notifier_call(nb, val, v);',
                     '\t\tr5_cb = (void *)nb->notifier_call;\n'
                     '\t\ttrace_notifier_boundary(r5_cb, nb, val, false, 0);\n'
                     '\t\tret = nb->notifier_call(nb, val, v);\n'
                     '\t\ttrace_notifier_boundary(r5_cb, nb, val, true, ret);')
    result[p] = t
    p = 'include/trace/events/power.h'
    result[p] = replace_once(files[p], 'TRACE_EVENT(device_pm_callback_start,',
                             PROGRESS_EVENT + '\nTRACE_EVENT(device_pm_callback_start,')
    p = 'drivers/base/power/main.c'
    t = replace_once(files[p], 'int dpm_prepare(pm_message_t state)\n{\n\tint error = 0;',
                     'int dpm_prepare(pm_message_t state)\n{\n\tint error = 0;\n'
                     '\tunsigned long r5_attempt = 0, r5_moved = 0;')
    t = replace_once(t, '\t\tstruct device *dev = to_device(dpm_list.next);',
                     '\t\tstruct device *dev = to_device(dpm_list.next);\n'
                     '\t\tint r5_ret;\n\t\tbool r5_did_move = false;')
    t = replace_once(t, '\t\terror = device_prepare(dev, state);',
                     '\t\terror = device_prepare(dev, state);\n\t\tr5_ret = error;')
    t = replace_once(t, '\t\t\tif (!list_empty(&dev->power.entry))\n'
                        '\t\t\t\tlist_move_tail(&dev->power.entry, &dpm_prepared_list);',
                     '\t\t\tif (!list_empty(&dev->power.entry)) {\n'
                     '\t\t\t\tlist_move_tail(&dev->power.entry, &dpm_prepared_list);\n'
                     '\t\t\t\tr5_moved++;\n\t\t\t\tr5_did_move = true;\n\t\t\t}')
    t = replace_once(t, '\t\t\t\t error);\n\t\t}\n\n\t\tmutex_unlock(&dpm_list_mtx);',
                     '\t\t\t\t error);\n\t\t}\n'
                     '\t\ttrace_r5_prepare_progress(dev, ++r5_attempt, r5_moved,\n'
                     '\t\t\t\t\t  r5_did_move, r5_ret);\n\n'
                     '\t\tmutex_unlock(&dpm_list_mtx);')
    result[p] = t
    return result

def prepare(source, output):
    source = source.resolve(strict=True)
    output = output.absolute()
    if output.exists() or output.is_symlink() or output.parent.resolve() != output.parent:
        raise ValueError('output must be new and its parent must not traverse symlinks')
    if source == output or source in output.parents or output in source.parents:
        raise ValueError('source/output must be disjoint')
    files = {}
    for name, expected in LOCK.items():
        path = source/name
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('source identity mismatch: '+name)
        files[name] = path.read_text()
    changed = overlay(files)  # Validate all anchors before making any copy.
    # Never copy existing signing secrets, build products or a source-tree config.
    shutil.copytree(source, output, symlinks=True,
                    ignore=shutil.ignore_patterns('*.pem', '*.key', '*.p12', '*.pfx',
                                                 '*.o', '*.ko', '.*.cmd', '.config*', '.git'))
    rows = []
    for name, text in changed.items():
        if text == files[name]:
            continue
        (output/name).write_text(text)
        rows.append({'path': name, 'before': LOCK[name],
                     'after': hashlib.sha256((output/name).read_bytes()).hexdigest()})
    return {'generation': 'r5obs1', 'changed': rows, 'input': LOCK,
            'install': 'NOT_RUN', 'PM': 'NOT_RUN', 'R5': 'FAIL'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.output), indent=2))
