#!/usr/bin/env python3
"""R34 offline-only source overlay; no build, mount, install, probe or PM."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

WINDOWS_RECEIVER = r'''# R36 raw UDP capture. No PM action; a receipt is not a coverage verdict.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LocalAddress,
    [Parameter(Mandatory=$true)][string]$SenderAddress,
    [Parameter(Mandatory=$true)][string]$Sessions
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$local = [Net.IPAddress]::Parse($LocalAddress)
$sender = [Net.IPAddress]::Parse($SenderAddress)
if ($local.AddressFamily -ne 'InterNetwork' -or $sender.AddressFamily -ne 'InterNetwork') {
    throw 'IPv4 required'
}
$allowed = @($Sessions.Split(','))
if ($allowed.Count -lt 1 -or $allowed.Count -gt 2 -or
    @($allowed | Select-Object -Unique).Count -ne $allowed.Count) { throw 'One or two unique sessions required' }
foreach ($id in $allowed) { if ($id -cnotmatch '^[a-f0-9]{32}$') { throw 'Invalid session' } }
$root = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('R36-stream-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$rule = Split-Path $root -Leaf
$udp = $null; $file = $null; $writer = $null; $ruleMade = $false
$active = ''; $finished = @(); $packets = 0L; $ignored = 0L; $bytesStored = 0L
$failure = $null; $exitCode = 0
$scriptHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()
$clock = [Diagnostics.Stopwatch]::StartNew(); $flushedAt = 0L
try {
    New-NetFirewallRule -DisplayName $rule -Direction Inbound -Action Allow `
        -Protocol UDP -LocalPort 6666 -RemotePort 6665 -LocalAddress $LocalAddress `
        -RemoteAddress $SenderAddress -Profile Any | Out-Null
    $ruleMade = $true
    $udp = [Net.Sockets.UdpClient]::new([Net.Sockets.AddressFamily]::InterNetwork)
    $udp.Client.ExclusiveAddressUse = $true
    $udp.Client.ReceiveBufferSize = 4194304
    $udp.Client.ReceiveTimeout = 100
    $udp.Client.Bind([Net.IPEndPoint]::new($local,6666))
    $peer = [Net.IPEndPoint]::new([Net.IPAddress]::Any,0)
    Write-Host ('SCRIPT_SHA256=' + $scriptHash)
    Write-Host ('CAPTURE_DIRECTORY=' + $root)
    Write-Host 'READY_CONTINUOUS (keep this window open; Ctrl+C preserves partial capture)'
    while ($finished.Count -lt $allowed.Count) {
        if ($clock.Elapsed.TotalSeconds -ge 7200) { throw 'Two-hour receiver deadline; incomplete streams preserved' }
        try { $data = $udp.Receive([ref]$peer) }
        catch [Net.Sockets.SocketException] {
            if ($_.Exception.SocketErrorCode -ne [Net.Sockets.SocketError]::TimedOut) { throw }
            if ($null -ne $file) { $file.Flush($true); $flushedAt = $clock.ElapsedMilliseconds }
            continue
        }
        if (-not $peer.Address.Equals($sender) -or $peer.Port -ne 6665) { $ignored++; continue }
        if ($data.Length -lt 64 -or $data.Length -gt 1344 -or
            [Text.Encoding]::ASCII.GetString($data,0,4) -cne 'R5O1' -or $data[4] -ne 1) {
            [IO.File]::WriteAllBytes((Join-Path $root 'rejected-datagram.bin'),$data)
            throw 'Malformed sender datagram preserved'
        }
        $nonce = ([BitConverter]::ToString($data,8,16)).Replace('-','').ToLowerInvariant()
        if ($allowed -cnotcontains $nonce) { $ignored++; continue }
        if ($finished -ccontains $nonce -or ($active -ne '' -and $active -cne $nonce)) {
            [IO.File]::WriteAllBytes((Join-Path $root 'rejected-datagram.bin'),$data)
            throw 'Interleaved or already-ended stream'
        }
        if ($null -eq $file) {
            $active = $nonce
            $file = [IO.File]::Open((Join-Path $root ($active + '.r5o')),'CreateNew','Write','Read')
            $writer = [IO.BinaryWriter]::new($file)
            $magic = [Text.Encoding]::ASCII.GetBytes("R5CAP01`n")
            $writer.Write($magic,0,$magic.Length)
        }
        $writer.Write([uint32]$data.Length)
        $writer.Write([int64][DateTime]::UtcNow.Ticks)
        $writer.Write($data,0,$data.Length)
        $packets++; $bytesStored += 12 + $data.Length
        if ($clock.ElapsedMilliseconds - $flushedAt -ge 100) {
            $file.Flush($true); $flushedAt = $clock.ElapsedMilliseconds
        }
        if ($bytesStored -ge 1073741824) { throw 'One-GiB capture budget reached; partial evidence preserved' }
        $length = [int]$data[6]*256 + [int]$data[7]
        if ($length -ne $data.Length-64 -or $data[5] -lt 1 -or $data[5] -gt 4) {
            throw 'Malformed expected stream preserved'
        }
        if ($data[5] -eq 4) {
            if ($length -ne 16) { throw 'Malformed END preserved' }
            $writer.Flush(); $file.Flush($true); $writer.Dispose(); $writer = $null; $file = $null
            $finished += $active
            $path = Join-Path $root ($active + '.r5o')
            Write-Host ('RECEIVED_AND_FLUSHED session=' + $active)
            Get-FileHash -Algorithm SHA256 -LiteralPath $path | Format-List
            Write-Host 'Coverage remains UNVERIFIED until Linux wire-check and event review'
            $active = ''
        }
    }
}
catch { $failure = $_.Exception.Message; $exitCode = 1; Write-Host ('STOP: ' + $failure) }
finally {
    if ($null -ne $writer) { $writer.Flush() }
    if ($null -ne $file) { $file.Flush($true) }
    if ($null -ne $writer) { $writer.Dispose() }
    if ($null -ne $udp) { $udp.Dispose() }
    if ($ruleMade) { Get-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue | Remove-NetFirewallRule }
    $files = @(Get-ChildItem -LiteralPath $root -File | ForEach-Object {
        @{name=$_.Name; bytes=$_.Length; sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
    })
    @{script_sha256=$scriptHash; sessions=$allowed; ended=$finished; packets=$packets;
      ignored=$ignored; failure=$failure; seconds=$clock.Elapsed.TotalSeconds;
      capture=$files; coverage='UNVERIFIED'; R5='FAIL'} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $root 'receipt.json') -Encoding UTF8
    Write-Host ('Evidence preserved: ' + $root)
}
exit $exitCode
'''

def prepare_receiver(output):
    with output.open('x', encoding='ascii', newline='\n') as stream:
        stream.write(WINDOWS_RECEIVER)
    return {'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
            'Windows_execution': 'NOT_RUN', 'PM': 'NOT_RUN'}

# The generated Linux module is separately licensed GPL-2.0-only. The Python
# generator retains the repository's original-tools license.
EXPORT_MODULE = r'''// SPDX-License-Identifier: GPL-2.0-only
/* R36: consume one private ftrace instance; no PM trigger or watchdog. */
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/etherdevice.h>
#include <linux/netpoll.h>
#include <linux/ring_buffer.h>
#include <linux/trace.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/utsname.h>
#include <linux/ctype.h>
#include <net/net_namespace.h>
#include "trace.h"

#define PAYLOAD 1280
#define BURST 256
#define QUEUE_LIMIT 64
#define CPU_LIMIT 64
static char *instance, *session, *interface, *sender, *receiver, *receiver_mac;
module_param(instance, charp, 0400);
module_param(session, charp, 0400);
module_param(interface, charp, 0400);
module_param(sender, charp, 0400);
module_param(receiver, charp, 0400);
module_param(receiver_mac, charp, 0400);
static unsigned int interval_ms = 20;
module_param(interval_ms, uint, 0400);

struct wire_header {
    u8 magic[4], version, kind;
    __be16 length;
    u8 session[16];
    __be64 sequence;
    __be32 cpu, worker_pid;
    __be64 timestamp, lost, oversized;
} __packed;
static struct {
    struct wire_header header;
    u8 payload[PAYLOAD];
} packet;
static struct trace_array *array;
static struct trace_buffer *buffer;
static struct netpoll np = { .name = "r5_trace_export", .local_port = 6665,
                             .remote_port = 6666 };
static struct delayed_work drain_work;
static u64 sequence, lost, oversized, last_stats;
static unsigned int cursor, transport_error;
static bool stopping;

static bool queue_room(void)
{
    struct netpoll_info *info;
    bool room;
    rcu_read_lock();
    info = rcu_dereference(np.dev->npinfo);
    room = info && skb_queue_len(&info->txq) < QUEUE_LIMIT &&
           netif_running(np.dev) && netif_device_present(np.dev) &&
           netif_carrier_ok(np.dev);
    rcu_read_unlock();
    return room;
}

static void send_packet(u8 kind, u32 cpu, u64 timestamp, unsigned int length)
{
    unsigned long flags;
    packet.header.kind = kind;
    packet.header.length = cpu_to_be16(length);
    packet.header.sequence = cpu_to_be64(sequence++);
    packet.header.cpu = cpu_to_be32(cpu);
    packet.header.worker_pid = cpu_to_be32(task_pid_nr(current));
    packet.header.timestamp = cpu_to_be64(timestamp);
    packet.header.lost = cpu_to_be64(lost);
    packet.header.oversized = cpu_to_be64(oversized);
    /* netpoll requires IRQs off. Never called inside a traced producer. */
    local_irq_save(flags);
    netpoll_send_udp(&np, (char *)&packet, sizeof(packet.header) + length);
    local_irq_restore(flags);
}

static void stats(void)
{
    int cpu;
    for_each_possible_cpu(cpu) {
        __be64 values[4];
        if (!queue_room())
            return; /* Missing final stats is a receiver-side failure. */
        values[0] = cpu_to_be64(ring_buffer_overrun_cpu(buffer, cpu));
        values[1] = cpu_to_be64(ring_buffer_commit_overrun_cpu(buffer, cpu));
        values[2] = cpu_to_be64(ring_buffer_dropped_events_cpu(buffer, cpu));
        values[3] = cpu_to_be64(ring_buffer_entries_cpu(buffer, cpu));
        memcpy(packet.payload, values, sizeof(values));
        send_packet(2, cpu, ktime_get_ns(), sizeof(values));
    }
}

static void drain(struct work_struct *work)
{
    unsigned int n, empty = 0;
    u64 deadline = ktime_get_ns() + 5 * NSEC_PER_MSEC;
    /* Admission requires exclusive ownership: no trace_pipe/snapshot reader. */
    if (!cpumask_empty(array->pipe_cpumask) ||
        (array->trace_flags & TRACE_ITER_OVERWRITE) ||
        strcmp(array->current_trace->name, "function_graph")) {
        transport_error = 1;
        return;
    }
    for (n = 0; n < BURST && ktime_get_ns() < deadline; n++) {
        struct ring_buffer_event *event;
        unsigned long missed = 0;
        unsigned int size, cpu = cursor++ % nr_cpu_ids;
        u64 timestamp;
        if (!queue_room())
            break;
        if (!cpu_possible(cpu))
            continue;
        event = ring_buffer_consume(buffer, cpu, &timestamp, &missed);
        if (!event) {
            if (++empty >= num_possible_cpus())
                break;
            continue;
        }
        empty = 0;
        lost += missed;
        size = ring_buffer_event_length(event);
        if (size > PAYLOAD) {
            oversized++;
            continue;
        }
        /* Copy before any further consume; only this worker owns the reader. */
        memcpy(packet.payload, ring_buffer_event_data(event), size);
        send_packet(1, cpu, timestamp, size);
    }
    if (ktime_get_ns() - last_stats >= NSEC_PER_SEC) {
        stats();
        last_stats = ktime_get_ns();
    }
    if (!READ_ONCE(stopping))
        queue_delayed_work(system_unbound_wq, &drain_work,
                           msecs_to_jiffies(interval_ms));
}

static int __init export_init(void)
{
    struct net_device *dev;
    __be32 begin[4];
    u64 allocated = 0;
    int ret, cpu;
    unsigned int i;
    BUILD_BUG_ON(sizeof(struct wire_header) != 64);
    if (strcmp(init_utsname()->release, "6.12.101-r5obs1") ||
        !instance || strncmp(instance, "r5obs1-", 7) ||
        strlen(instance) > 63 || !session || strlen(session) != 32 ||
        !interface || !sender || !receiver || !receiver_mac ||
        interval_ms < 1 || interval_ms > 100 || nr_cpu_ids > CPU_LIMIT)
        return -EINVAL;
    for (i = 0; instance[i]; i++)
        if (!isalnum(instance[i]) && !strchr("_.-", instance[i]))
            return -EINVAL;
    if (hex2bin(packet.header.session, session, 16) ||
        strscpy(np.dev_name, interface, sizeof(np.dev_name)) < 0 ||
        !in4_pton(sender, -1, (u8 *)&np.local_ip.ip, -1, NULL) ||
        !in4_pton(receiver, -1, (u8 *)&np.remote_ip.ip, -1, NULL) ||
        !mac_pton(receiver_mac, np.remote_mac) ||
        !is_valid_ether_addr(np.remote_mac) || !np.local_ip.ip || !np.remote_ip.ip)
        return -EINVAL;
    dev = dev_get_by_name(&init_net, interface);
    if (!dev)
        return -ENODEV;
    /* Do not open a down interface or share another netpoll owner's queue. */
    ret = (!netif_running(dev) || !netif_carrier_ok(dev) ||
           !dev->netdev_ops->ndo_poll_controller ||
           rcu_access_pointer(dev->npinfo) || dev->mtu < sizeof(packet) + 28)
          ? -EBUSY : 0;
    dev_put(dev);
    if (ret)
        return ret;
    array = trace_array_get_by_name(instance, NULL);
    if (!array)
        return -ENOMEM;
    buffer = array->array_buffer.buffer;
    if (!READ_ONCE(array->buffer_disabled) ||
        !cpumask_empty(array->pipe_cpumask) ||
        (array->trace_flags & TRACE_ITER_OVERWRITE) ||
        strcmp(array->current_trace->name, "function_graph") ||
        ring_buffer_entries(buffer)) {
        ret = -EBUSY;
        goto put;
    }
    for_each_possible_cpu(cpu)
        allocated += ring_buffer_size(buffer, cpu);
    if (!allocated || allocated > 64ULL * 1024 * 1024) {
        ret = -E2BIG;
        goto put;
    }
    ret = netpoll_setup(&np);
    if (ret)
        goto put;
    memcpy(packet.header.magic, "R5O1", 4);
    packet.header.version = 1;
    begin[0] = cpu_to_be32(num_possible_cpus());
    begin[1] = cpu_to_be32(interval_ms);
    begin[2] = cpu_to_be32(BURST);
    begin[3] = cpu_to_be32(PAYLOAD);
    memcpy(packet.payload, begin, sizeof(begin));
    send_packet(3, ~0U, ktime_get_ns(), sizeof(begin));
    INIT_DELAYED_WORK(&drain_work, drain);
    queue_delayed_work(system_unbound_wq, &drain_work, 0);
    return 0;
put:
    trace_array_put(array);
    return ret;
}

static void __exit export_exit(void)
{
    struct { __be64 remaining; __be32 active, error; } __packed end;
    WRITE_ONCE(stopping, true);
    cancel_delayed_work_sync(&drain_work);
    /* Caller must stop tracing and let the worker drain before rmmod. */
    stats();
    end.remaining = cpu_to_be64(ring_buffer_entries(buffer));
    end.active = cpu_to_be32(!READ_ONCE(array->buffer_disabled));
    end.error = cpu_to_be32(transport_error);
    memcpy(packet.payload, &end, sizeof(end));
    if (queue_room())
        send_packet(4, ~0U, ktime_get_ns(), sizeof(end));
    netpoll_cleanup(&np);
    trace_array_put(array);
}
module_init(export_init);
module_exit(export_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("R36 bounded ftrace consumer; no PM or watchdog");
'''

def prepare_export(source, output):
    """Emit an external module for the exact observation kernel, never load it."""
    path = source/'kernel/trace/trace.h'
    expected = 'c614689246f36f68bdf32ea4b4e9981cfc357da5485fa8a1a7ea1a3732441560'
    if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('internal trace layout differs from the locked kernel')
    output.mkdir()  # Never overwrite an old experiment's source or products.
    (output/'r5_trace_export.c').write_text(EXPORT_MODULE)
    (output/'Makefile').write_text('obj-m := r5_trace_export.o\nccflags-y += -I$(srctree)/kernel/trace\n')
    return {'source_sha256': hashlib.sha256(EXPORT_MODULE.encode()).hexdigest(),
            'trace_header_sha256': expected, 'install': 'NOT_RUN', 'PM': 'NOT_RUN'}

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
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--export-module', action='store_true')
    modes.add_argument('--receiver-script', action='store_true')
    args = parser.parse_args()
    if args.receiver_script:
        result = prepare_receiver(args.output)
    else:
        if args.source is None:
            parser.error('--source required for source/module preparation')
        operation = prepare_export if args.export_module else prepare
        result = operation(args.source, args.output)
    print(json.dumps(result, indent=2))
