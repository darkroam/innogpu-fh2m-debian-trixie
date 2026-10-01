#!/usr/bin/env python3
"""Offline source/transport preparation; no build, mount, install, probe or PM."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil

# One contract owner: the production checker. Do not maintain a second schema
# or budget in the generator or its test fixtures.
_spec = importlib.util.spec_from_file_location('r5_observation', Path(__file__).with_name('r5-observation.py'))
_analysis = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_analysis)

WINDOWS_RECEIVER = r'''# R41 raw UDP capture. No PM action; a receipt is not a coverage verdict.
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
$root = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('R41-stream-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$rule = Split-Path $root -Leaf
$udp = $null; $file = $null; $writer = $null; $ruleMade = $false
$active = ''; $finished = @(); $packets = 0L; $ignored = 0L; $bytesStored = 0L
$failure = $null; $exitCode = 0
$scriptHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()
$clock = [Diagnostics.Stopwatch]::StartNew(); $flushedAt = 0L
$heartbeatAt = -1000L; $receivedAt = -1L; $expectedSequence = [uint64]0
$sequenceGaps = [uint64]0; $sequenceErrors = 0L; $reportedLost = [uint64]0; $oversized = [uint64]0
$cpuDropped = @{}
function Read-BigEndian64([byte[]]$Data, [int]$Offset) {
    $part = [byte[]]::new(8); [Array]::Copy($Data,$Offset,$part,0,8)
    if ([BitConverter]::IsLittleEndian) { [Array]::Reverse($part) }
    return [BitConverter]::ToUInt64($part,0)
}
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
        if ($clock.ElapsedMilliseconds - $heartbeatAt -ge 1000) {
            $age = if ($receivedAt -lt 0) { 'WAITING' } else { [string]($clock.ElapsedMilliseconds-$receivedAt) }
            $dropped = [uint64]0
            foreach ($v in $cpuDropped.Values) { $dropped += $v }
            Write-Host ('HEARTBEAT session={0} packets={1} bytes={2} last_rx_ms={3} gaps={4} sequence_errors={5} header_lost={6} dropped={7} oversized={8} flush_age_ms={9}' -f `
                $active,$packets,$bytesStored,$age,$sequenceGaps,$sequenceErrors,$reportedLost,$dropped,$oversized,($clock.ElapsedMilliseconds-$flushedAt))
            $heartbeatAt = $clock.ElapsedMilliseconds
        }
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
            $expectedSequence = [uint64]0; $cpuDropped = @{}
            $file = [IO.File]::Open((Join-Path $root ($active + '.r5o')),'CreateNew','Write','Read')
            $writer = [IO.BinaryWriter]::new($file)
            $magic = [Text.Encoding]::ASCII.GetBytes("R5CAP01`n")
            $writer.Write($magic,0,$magic.Length)
        }
        $writer.Write([uint32]$data.Length)
        $writer.Write([int64][DateTime]::UtcNow.Ticks)
        $writer.Write($data,0,$data.Length)
        $packets++; $bytesStored += 12 + $data.Length
        $receivedAt = $clock.ElapsedMilliseconds
        $seq = Read-BigEndian64 $data 24
        if ($seq -gt $expectedSequence) { $sequenceGaps += $seq-$expectedSequence }
        if ($seq -lt $expectedSequence) { $sequenceErrors++ }
        $expectedSequence = [Math]::Max($expectedSequence,$seq+[uint64]1)
        $reportedLost = [Math]::Max($reportedLost,(Read-BigEndian64 $data 48))
        $oversized = [Math]::Max($oversized,(Read-BigEndian64 $data 56))
        if ($clock.ElapsedMilliseconds - $flushedAt -ge 100) {
            $file.Flush($true); $flushedAt = $clock.ElapsedMilliseconds
        }
        if ($bytesStored -ge 1073741824) { throw 'One-GiB capture budget reached; partial evidence preserved' }
        $length = [int]$data[6]*256 + [int]$data[7]
        if ($length -ne $data.Length-64 -or $data[5] -lt 1 -or $data[5] -gt 4) {
            throw 'Malformed expected stream preserved'
        }
        if ($data[5] -eq 2) {
            if ($length -ne 32) { throw 'Malformed CPU counters preserved' }
            $cpu = [int]$data[32]*16777216 + [int]$data[33]*65536 + [int]$data[34]*256 + [int]$data[35]
            $cpuDropped[$cpu] = (Read-BigEndian64 $data 64) + (Read-BigEndian64 $data 72) + (Read-BigEndian64 $data 80)
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
      sequence_gaps=$sequenceGaps; sequence_errors=$sequenceErrors; reported_lost=$reportedLost;
      oversized=$oversized; cpu_dropped=$cpuDropped;
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
/* R41 transport only; r5obs2 semantic events/admission are separate gates. */
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
#include <linux/mm.h>
#include <net/net_namespace.h>
#include "trace.h"

#define PAYLOAD 256
#define BURST 2048
#define CPU_BURST 128
#define QUEUE_LIMIT 64
#define CPU_LIMIT 16
/* R44 scheme 2: 1920 data pages/CPU, 120MiB across 16 CPUs.
 * This is only the data-page limit: memory-budget.json accounts known
 * reader/metadata buckets. Full 133MiB admission still needs target receipts. */
#define CPU_BUFFER_BYTES (@CPU_BUFFER_KIB@ULL * 1024)
#define TOTAL_PAGE_BYTES (@DATA_PAGE_BYTES@ULL)
static char *instance, *session, *interface, *sender, *receiver, *receiver_mac;
module_param(instance, charp, 0400);
module_param(session, charp, 0400);
module_param(interface, charp, 0400);
module_param(sender, charp, 0400);
module_param(receiver, charp, 0400);
module_param(receiver_mac, charp, 0400);
static unsigned int interval_ms = 1;
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
static struct trace_event_file *semantic_files[3];
static struct trace_buffer *buffer;
static struct netpoll np = { .name = "r5_trace_export", .local_port = 6665,
                             .remote_port = 6666 };
static struct delayed_work drain_work;
static u64 sequence, lost, oversized, last_stats;
static unsigned int cursor, transport_error;
static bool stopping;
#include <linux/r5obs2.h>

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

static int geometry_reject(const char *gate, int cpu, long actual, unsigned long limit)
{
    pr_err("r5_trace_export reject: gate=%s cpu=%d actual=%ld limit=%lu rc=%d\n",
           gate, cpu, actual, limit, -E2BIG);
    return -E2BIG;
}

static int check_buffer_geometry(void)
{
    u64 allocated = 0;
    unsigned long size;
    int cpu, subbuf = ring_buffer_subbuf_size_get(buffer);
    if (subbuf != PAGE_SIZE)
        return geometry_reject("primary-subbuf", -1, subbuf, PAGE_SIZE);
#ifdef CONFIG_TRACER_MAX_TRACE
    if (array->allocated_snapshot)
        return geometry_reject("snapshot-active", -1, 1, 0);
#endif
    for_each_possible_cpu(cpu) {
        size = ring_buffer_size(buffer, cpu);
        if (size != CPU_BUFFER_BYTES)
            return geometry_reject("primary-size", cpu, size, CPU_BUFFER_BYTES);
#ifdef CONFIG_TRACER_MAX_TRACE
        subbuf = ring_buffer_subbuf_size_get(array->max_buffer.buffer);
        if (subbuf != PAGE_SIZE)
            return geometry_reject("snapshot-subbuf", cpu, subbuf, PAGE_SIZE);
        size = ring_buffer_size(array->max_buffer.buffer, cpu);
        if (size != 2 * (PAGE_SIZE - 16))
            return geometry_reject("snapshot-size", cpu, size, 2 * (PAGE_SIZE - 16));
#endif
        allocated += CPU_BUFFER_BYTES / (PAGE_SIZE - 16) * PAGE_SIZE;
    }
    if (!allocated || allocated > TOTAL_PAGE_BYTES)
        return geometry_reject("total-pages", -1, allocated, TOTAL_PAGE_BYTES);
    return 0;
}

static bool semantic_profile_ok(void)
{
    unsigned int i;
    if (!cpumask_equal(array->tracing_cpumask, cpu_possible_mask))
        return false;
    for (i = 0; i < 3; i++) {
        unsigned long flags = READ_ONCE(semantic_files[i]->flags);
        if (!(flags & EVENT_FILE_FL_ENABLED) ||
            (flags & ~(EVENT_FILE_FL_ENABLED | EVENT_FILE_FL_WAS_ENABLED)))
            return false; /* no filter, trigger, PID selection or soft disable */
    }
    return true;
}

static void drain(struct work_struct *work)
{
    unsigned int n = 0, empty = 0;
    u64 deadline = ktime_get_ns() + 5 * NSEC_PER_MSEC;
    /* Locked trace.c clock index 5 is mono; cross-CPU pairing needs it.
     * Admission requires exclusive ownership: no trace_pipe/snapshot reader. */
    if (!cpumask_empty(array->pipe_cpumask) ||
        (array->trace_flags & TRACE_ITER_OVERWRITE) ||
        strcmp(array->current_trace->name, "nop") || array->clock_id != 5) {
        transport_error = 1;
        return;
    }
    if (!semantic_profile_ok() || r5obs2_status() || (!READ_ONCE(array->buffer_disabled) &&
                           r5obs2_emit_dictionary())) {
        transport_error = 1;
        return;
    }
    /* Empty CPU visits do not spend the event budget. A busy CPU yields after
     * CPU_BURST records, including across worker runs, so peers cannot starve.
     * The deadline is tested per record; one netpoll call is not preempted. */
    while (n < BURST && ktime_get_ns() < deadline) {
        unsigned int quota, cpu = cursor;
        cursor = (cursor + 1) % nr_cpu_ids;
        for (quota = 0; quota < CPU_BURST && n < BURST &&
             ktime_get_ns() < deadline; quota++) {
            struct ring_buffer_event *event;
            unsigned long missed = 0;
            unsigned int size;
            u64 timestamp;
            if (!queue_room())
                goto reschedule;
            event = ring_buffer_consume(buffer, cpu, &timestamp, &missed);
            if (!event) {
                if (++empty >= nr_cpu_ids)
                    goto reschedule;
                break;
            }
            empty = 0;
            n++;
            lost += missed;
            size = ring_buffer_event_length(event);
            if (size > PAYLOAD) {
                oversized++;
                transport_error = 1;
                return; /* Do not silently truncate an unapproved layout. */
            }
            memcpy(packet.payload, ring_buffer_event_data(event), size);
            send_packet(1, cpu, timestamp, size);
        }
    }
reschedule:
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
    int ret;
    unsigned int i;
    BUILD_BUG_ON(sizeof(struct wire_header) != 64);
    BUILD_BUG_ON(PAGE_SIZE != 4096);
    if (strcmp(init_utsname()->release, "@KERNEL_RELEASE@") ||
        !instance || strncmp(instance, "r5obs2-", 7) ||
        strlen(instance) > 63 || !session || strlen(session) != 32 ||
        !interface || !sender || !receiver || !receiver_mac ||
        interval_ms < 1 || interval_ms > 5 || nr_cpu_ids > CPU_LIMIT ||
        nr_cpu_ids != num_possible_cpus())
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
    for (i = 0; i < 3; i++) {
        semantic_files[i] = trace_get_event_file(instance, "power",
                                i == 0 ? "r5_pair" : (i == 1 ? "r5_dictionary" : "r5_aux"));
        if (IS_ERR(semantic_files[i])) {
            ret = PTR_ERR(semantic_files[i]);
            semantic_files[i] = NULL;
            goto put;
        }
    }
    if (!semantic_profile_ok()) {
        ret = -EINVAL;
        goto put;
    }
    buffer = array->array_buffer.buffer;
    if (!READ_ONCE(array->buffer_disabled) ||
        !cpumask_empty(array->pipe_cpumask) ||
        (array->trace_flags & TRACE_ITER_OVERWRITE) ||
        strcmp(array->current_trace->name, "nop") || array->clock_id != 5 ||
        ring_buffer_entries(buffer)) {
        ret = -EBUSY;
        goto put;
    }
    ret = check_buffer_geometry();
    if (ret)
        goto put;
    ret = r5obs2_census();
    if (ret) {
        pr_err("r5_trace_export reject: gate=census rc=%d\n", ret);
        goto put;
    }
    ret = netpoll_setup(&np);
    if (ret) {
        pr_err("r5_trace_export reject: gate=netpoll rc=%d\n", ret);
        goto put;
    }
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
    for (i = 0; i < 3; i++)
        if (semantic_files[i])
            trace_put_event_file(semantic_files[i]);
    r5obs2_stop();
    trace_array_put(array);
    return ret;
}

static void __exit export_exit(void)
{
    struct { __be64 remaining; __be32 active, error; } __packed end;
    unsigned int i;
    WRITE_ONCE(stopping, true);
    cancel_delayed_work_sync(&drain_work);
    /* Caller must stop tracing and let the worker drain before rmmod. */
    stats();
    end.remaining = cpu_to_be64(ring_buffer_entries(buffer));
    end.active = cpu_to_be32(!READ_ONCE(array->buffer_disabled));
    end.error = cpu_to_be32(transport_error || !semantic_profile_ok() || r5obs2_status());
    memcpy(packet.payload, &end, sizeof(end));
    if (queue_room())
        send_packet(4, ~0U, ktime_get_ns(), sizeof(end));
    r5obs2_stop();
    netpoll_cleanup(&np);
    for (i = 0; i < 3; i++)
        trace_put_event_file(semantic_files[i]);
    trace_array_put(array);
}
module_init(export_init);
module_exit(export_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("R41 bounded nop/event ftrace consumer; no PM or watchdog");
'''
EXPORT_MODULE = EXPORT_MODULE.replace('@CPU_BUFFER_KIB@', str(_analysis.BUFFER_KIB)).replace(
    '@DATA_PAGE_BYTES@', str(_analysis.memory_budget()['components']['data_pages'])).replace(
    '@KERNEL_RELEASE@', _analysis.KERNEL_RELEASE)

def prepare_export(source, output):
    """Emit an external module for the exact observation kernel, never load it."""
    path = source/'kernel/trace/trace.h'
    expected = 'c614689246f36f68bdf32ea4b4e9981cfc357da5485fa8a1a7ea1a3732441560'
    if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('internal trace layout differs from the locked kernel')
    ring = source/'kernel/trace/ring_buffer.c'
    ring_sha = 'b83a8b848a44e0785912328f613bd66de23b182483c1820a5f6448f0cc53d338'
    if ring.is_symlink() or hashlib.sha256(ring.read_bytes()).hexdigest() != ring_sha:
        raise ValueError('ring page accounting differs from the locked kernel')
    trace = source/'kernel/trace/trace.c'
    trace_sha = '5bbeb2364266a38ef93da3b669d45d02e4b879f900225631378a3d4b7be84a57'
    if trace.is_symlink() or hashlib.sha256(trace.read_bytes()).hexdigest() != trace_sha:
        raise ValueError('snapshot/trace-array accounting differs from the locked kernel')
    output.mkdir()  # Never overwrite an old experiment's source or products.
    (output/'r5_trace_export.c').write_text(EXPORT_MODULE)
    (output/'Makefile').write_text('obj-m := r5_trace_export.o\nccflags-y += -I$(srctree)/kernel/trace\n')
    contract = _analysis.semantic_contract()
    budget = _analysis.memory_budget()
    (output/'semantic-contract.json').write_text(json.dumps(contract, indent=2)+'\n')
    (output/'memory-budget.json').write_text(json.dumps(budget, indent=2)+'\n')
    return {'source_sha256': hashlib.sha256(EXPORT_MODULE.encode()).hexdigest(),
            'trace_header_sha256': expected, 'ring_source_sha256': ring_sha,
            'trace_source_sha256': trace_sha,
            'kernel_release': _analysis.KERNEL_RELEASE,
            'semantic_schema': contract['schema'], 'semantic_events': 'OFFLINE_IMPLEMENTED_NOT_BOOTED',
            'parser': 'RAW_SEMANTIC_PAIRS_IMPLEMENTED', 'memory_budget': budget,
            'admission': 'UNVERIFIED',
            'install': 'NOT_RUN', 'PM': 'NOT_RUN'}

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


# Generated kernel C is GPL-2.0-only; Python keeps the original-tools license.
SEMANTIC_HEADER = r'''// SPDX-License-Identifier: GPL-2.0-only
#ifndef _LINUX_R5OBS2_H
#define _LINUX_R5OBS2_H
#include <linux/types.h>
struct r5obs2_token {
    u64 object_id, object_generation, completion_generation;
    u64 task_id, task_generation, call_id;
    u32 peer_id, operation, pm_phase, flags;
};
enum { R5_WAIT, R5_COMPLETE, R5_CALLBACK, R5_RESUME, R5_REINIT, R5_LOCK,
       R5_NOTIFIER, R5_PREPARE, R5_TASK, R5_SKIP, R5_SUPERIOR, R5_UNLOCK, R5_STAGE,
       R5_WORKER, R5_RUNTIME, R5_PROBE };
enum { R5_PROGRESS = 1, R5_STAGE_BEGIN, R5_STAGE_END, R5_ASYNC,
       R5_CALLBACK_META, R5_NOTIFIER_BEGIN, R5_NOTIFIER_END, R5_WORKER_META, R5_WAKE_META,
       R5_CALLBACK_ENTRY, R5_WORKER_ENTRY };
enum { R5_PREPARE_PHASE, R5_SUSPEND_PHASE, R5_RESUME_PHASE, R5_COMPLETE_PHASE };
#ifdef CONFIG_PM_SLEEP
void r5obs2_topology_changed(void);
void r5obs2_links_lock(void);
void r5obs2_links_unlock(void);
int r5obs2_census(void);
int r5obs2_emit_dictionary(void);
int r5obs2_status(void);
void r5obs2_stop(void);
struct r5obs2_token r5obs2_stage_begin(unsigned int stage, u64 value);
void r5obs2_stage_end(struct r5obs2_token *t, unsigned int stage, u64 value, int ret);
struct notifier_block;
int r5obs2_pm_census(void);
int r5obs2_notifier_add(struct notifier_block *nb);
struct r5obs2_token r5obs2_notifier_begin(struct notifier_block *nb, u64 action);
void r5obs2_notifier_end(struct r5obs2_token *t, struct notifier_block *nb, u64 action, int ret);
struct device;
struct r5obs2_token r5obs2_runtime_begin(struct device *dev);
struct r5obs2_token r5obs2_probe_begin(struct device *dev);
void r5obs2_runtime_end(struct r5obs2_token *t, int ret);
void r5obs2_wake_note(struct r5obs2_token *t, unsigned int irq, bool pending);
#else
static inline void r5obs2_topology_changed(void) { }
#endif
#endif
'''

SEMANTIC_EVENTS = r'''
/* R45: fixed layouts; no pointers, strings or allocation in paired events. */
TRACE_EVENT(r5_pair,
    TP_PROTO(const struct r5obs2_token *t, bool exit, int ret),
    TP_ARGS(t, exit, ret),
    TP_STRUCT__entry(
        __field_struct(struct r5obs2_token, token)
        __field(u32, exit)
        __field(s32, ret)
    ),
    TP_fast_assign(__entry->token = *t; __entry->exit = exit; __entry->ret = ret;),
    TP_printk("object=%llu call=%llu exit=%u ret=%d", __entry->token.object_id,
              __entry->token.call_id, __entry->exit, __entry->ret)
);
TRACE_EVENT(r5_dictionary,
    TP_PROTO(u32 id, u32 parent, u32 supplier, u64 life, u32 kind,
             const char *name, const char *driver),
    TP_ARGS(id, parent, supplier, life, kind, name, driver),
    TP_STRUCT__entry(
        __field(u64, life)
        __field(u32, id)
        __field(u32, parent)
        __field(u32, supplier)
        __field(u32, kind)
        __array(char, name, 48)
        __array(char, driver, 56)
    ),
    TP_fast_assign(
        __entry->life = life; __entry->id = id; __entry->parent = parent;
        __entry->supplier = supplier; __entry->kind = kind;
        memset(__entry->name, 0, sizeof(__entry->name));
        memset(__entry->driver, 0, sizeof(__entry->driver));
        strscpy(__entry->name, name, sizeof(__entry->name));
        strscpy(__entry->driver, driver, sizeof(__entry->driver));
    ),
    TP_printk("id=%u parent=%u supplier=%u life=%llu kind=%u name=%s driver=%s",
        __entry->id, __entry->parent, __entry->supplier, __entry->life,
        __entry->kind, __entry->name, __entry->driver)
);
'''

SEMANTIC_EVENTS += r'''
TRACE_EVENT(r5_aux,
    TP_PROTO(const struct r5obs2_token *t, u64 a, u64 b, u32 code, s32 ret),
    TP_ARGS(t, a, b, code, ret),
    TP_STRUCT__entry(
        __field_struct(struct r5obs2_token, token)
        __field(u64, a)
        __field(u64, b)
        __field(u32, code)
        __field(s32, ret)
    ),
    TP_fast_assign(__entry->token = *t; __entry->a = a; __entry->b = b;
                   __entry->code = code; __entry->ret = ret;),
    TP_printk("call=%llu code=%u a=%llu b=%llu ret=%d", __entry->token.call_id,
              __entry->code, __entry->a, __entry->b, __entry->ret)
);
'''

SEMANTIC_EMITTER = r'''// SPDX-License-Identifier: GPL-2.0-only
/* Included only in the generated observation kernel's PM main.c.
 * Static storage, no allocation/extra device lock in a recorded boundary.
 * Any topology change invalidates this census; stale pointers are compared,
 * never dereferenced after census. No PM outcome or return value is changed.
 */
#include <linux/uaccess.h>
#define R5_OBJECTS @OBJECTS@
#define R5_EDGES @EDGES@
#define R5_FUNCTIONS @FUNCTIONS@
#define R5_NOTIFIERS @NOTIFIERS@
#define R5_DICTIONARY_CAP @DICTIONARY_CAP@
#define R5_DICTIONARY_RESERVE @DICTIONARY_RESERVE@
struct r5_object {
	struct device *dev;
	atomic64_t generation;
	atomic_t resetting;
	atomic_t prepares;
	u32 parent;
	char name[48];
	char driver[56];
};
struct r5_edge { u32 consumer, supplier; };
static struct r5_object r5_objects[R5_OBJECTS];
static struct r5_edge r5_edges[R5_EDGES];
static unsigned int r5_n, r5_e;
static atomic_t r5_state = ATOMIC_INIT(0); /* 0 off, 1 census, 2 ready, 3 invalid */
static atomic64_t r5_lifetime = ATOMIC64_INIT(0);
static atomic64_t r5_calls = ATOMIC64_INIT(0);
static atomic_t r5_exported = ATOMIC_INIT(0);
static atomic_t r5_records = ATOMIC_INIT(0);
/* R40 profile: detailed dependency/callback flow only in ordinary resume.
 * Other phases retain all reinit/complete pairs plus paired stage summaries. */
static bool r5_ordinary;
/* Admission-only latch; print after census locks, never in a PM boundary. */
static const char *r5_reject_gate = "unclassified";
static long r5_reject_value;
static unsigned long r5_reject_limit;
static int r5_size_error(const char *gate, long value, unsigned long limit)
{
    r5_reject_gate = gate;
    r5_reject_value = value;
    r5_reject_limit = limit;
    return -E2BIG;
}

void r5obs2_topology_changed(void)
{
	/* Includes census in flight; never silently reassign an active ID. */
	int state;
	do {
		state = atomic_read(&r5_state);
		if (!state || state == 3)
			return;
	} while (atomic_cmpxchg(&r5_state, state, 3) != state);
}
EXPORT_SYMBOL_GPL(r5obs2_topology_changed);

static bool r5_slot(void)
{
	if (atomic_inc_return(&r5_records) <= 39936)
		return true;
	r5obs2_topology_changed();
	return false;
}

/* ponytail: bounded linear lookup (R5_OBJECTS); preallocated hash if measured cost requires it. */
static unsigned int r5_find(struct device *dev)
{
	unsigned int i;
	if (!dev)
		return 0;
	for (i = 0; i < r5_n; i++)
		if (r5_objects[i].dev == dev)
			return i + 1;
	return 0;
}

/* Called only while census holds dpm_list_mtx and device-links SRCU. */
static int r5_add(struct device *dev)
{
	struct r5_object *o;
	unsigned int id = r5_find(dev);
	if (!dev || id)
		return id;
	if (r5_n == R5_OBJECTS)
		return r5_size_error("objects", r5_n + 1, R5_OBJECTS);
	o = &r5_objects[r5_n];
	o->dev = get_device(dev); /* census references, released before ready */
	atomic64_set(&o->generation, 1);
	atomic_set(&o->resetting, 0);
	atomic_set(&o->prepares, 0);
	return ++r5_n;
}

int r5obs2_census(void)
{
	struct device *dev;
	struct device_link *link;
	int idx, ret = 0, id;
	unsigned int i;
	BUILD_BUG_ON(sizeof(struct r5_object) != 136 || sizeof(struct r5_edge) != 8);
	BUILD_BUG_ON(sizeof(r5_objects) + sizeof(r5_edges) > R5_DICTIONARY_CAP);
	BUILD_BUG_ON(sizeof(struct r5obs2_token) > 96);
	if (!trace_r5_pair_enabled() || !trace_r5_dictionary_enabled() || !trace_r5_aux_enabled())
		return -EINVAL;
	if (atomic_cmpxchg(&r5_state, 0, 1))
		return -EBUSY;
	r5obs2_links_lock();
	mutex_lock(&dpm_list_mtx);
	if (!list_empty(&dpm_prepared_list) || !list_empty(&dpm_suspended_list) ||
	    !list_empty(&dpm_late_early_list) || !list_empty(&dpm_noirq_list)) {
		ret = -EBUSY;
		goto out;
	}
	r5_n = r5_e = 0;
	atomic_set(&r5_exported, 0);
	atomic64_inc(&r5_lifetime);
	atomic64_set(&r5_calls, 0);
	idx = device_links_read_lock();
	list_for_each_entry(dev, &dpm_list, power.entry) {
		ret = r5_add(dev);
		if (ret < 0)
			goto unlock;
	}
	/* Include parents/suppliers even if they are not PM list members. */
	for (i = 0; i < r5_n; i++) {
		dev = r5_objects[i].dev;
		id = r5_add(dev->parent);
		if (id < 0) { ret = id; goto unlock; }
		r5_objects[i].parent = id;
		list_for_each_entry_rcu_locked(link, &dev->links.suppliers, c_node) {
			if (READ_ONCE(link->status) == DL_STATE_DORMANT)
				continue;
			id = r5_add(link->supplier);
			if (id < 0) { ret = id; goto unlock; }
			if (r5_e == R5_EDGES) {
				ret = r5_size_error("edges", r5_e + 1, R5_EDGES);
				goto unlock;
			}
			r5_edges[r5_e++] = (struct r5_edge){ i + 1, id };
		}
	}
	ret = r5_n ? 0 : -ENODEV;
unlock:
	device_links_read_unlock(idx);
out:
	mutex_unlock(&dpm_list_mtx);
	r5obs2_links_unlock();
	/* Locking/refcounting exists only during pre-trigger census. Never take
	 * a device lock under dpm_list_mtx; driver unbind must not race labels.
	 * Rename does not take device_lock, so copy the name with nofault and
	 * rely on rename invalidation; never follow freed string storage. */
	for (i = 0; i < r5_n; i++) {
		struct r5_object *o = &r5_objects[i];
		long copied;
		if (!ret && atomic_read(&r5_state) == 1) {
			device_lock(o->dev);
			copied = strncpy_from_kernel_nofault(o->name, dev_name(o->dev), sizeof(o->name));
			if (copied <= 0 || copied >= sizeof(o->name))
				ret = r5_size_error("object-name-copy", copied, sizeof(o->name));
			else {
				copied = strscpy(o->driver, dev_driver_string(o->dev), sizeof(o->driver));
				if (copied < 0)
					ret = r5_size_error("driver-name-copy", copied, sizeof(o->driver));
			}
			device_unlock(o->dev);
		}
		put_device(o->dev);
	}
	if (ret || atomic_cmpxchg(&r5_state, 1, 2) != 1) {
		atomic_set(&r5_state, 3);
		pr_err("r5obs2 census reject: gate=%s value=%ld limit=%lu objects=%u edges=%u rc=%d\n",
		       r5_reject_gate, r5_reject_value, r5_reject_limit, r5_n, r5_e, ret ? ret : -ESTALE);
		return ret ? ret : -ESTALE;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(r5obs2_census);

/* Called from the exporter worker after tracing starts, before any PM action.
 * Receiver must retain all dictionary records and the final census count.
 */
int r5obs2_emit_dictionary(void)
{
	unsigned int i;
	u64 life = atomic64_read(&r5_lifetime);
	if (atomic_read(&r5_state) != 2)
		return -ESTALE;
	if (atomic_cmpxchg(&r5_exported, 0, 1))
		return 0;
	for (i = 0; i < r5_n; i++) {
		if (!r5_slot()) return -E2BIG;
		trace_r5_dictionary(i + 1, r5_objects[i].parent, 0, life, 0,
				    r5_objects[i].name, r5_objects[i].driver);
	}
	for (i = 0; i < r5_e; i++) {
		if (!r5_slot()) return -E2BIG;
		trace_r5_dictionary(r5_edges[i].consumer, 0, r5_edges[i].supplier,
				    life, 1, "", "");
	}
	if (!r5_slot()) return -E2BIG;
	trace_r5_dictionary(r5_n, r5_e, 0, life, 2, "END_DICTIONARY", "");
	return atomic_read(&r5_state) == 2 ? 0 : -ESTALE;
}
EXPORT_SYMBOL_GPL(r5obs2_emit_dictionary);

int r5obs2_status(void) { return atomic_read(&r5_state) == 2 ? 0 : -ESTALE; }
EXPORT_SYMBOL_GPL(r5obs2_status);

/* One census per observation boot: no active pointer/epoch reuse on unload. */
void r5obs2_stop(void) { atomic_set(&r5_state, 3); }
EXPORT_SYMBOL_GPL(r5obs2_stop);

static struct r5obs2_token r5_begin(struct device *dev, unsigned int op,
				  unsigned int phase, struct device *peer)
{
	struct r5obs2_token t = { };
	unsigned int id, peer_id;
	if (!atomic_read(&r5_exported) || atomic_read(&r5_state) != 2)
		return t;
	if (!READ_ONCE(r5_ordinary) &&
	    (op == R5_WAIT || op == R5_SKIP || op == R5_SUPERIOR ||
	     op == R5_TASK || op == R5_WORKER ||
	     (op == R5_CALLBACK && phase != R5_PREPARE_PHASE && phase != R5_COMPLETE_PHASE)))
		return t;
	id = r5_find(dev);
	peer_id = r5_find(peer);
	if (!id || (peer && !peer_id)) {
		r5obs2_topology_changed();
		return t;
	}
	if (op == R5_PREPARE && atomic_inc_return(&r5_objects[id-1].prepares) > 4) {
		r5obs2_topology_changed();
		return t;
	}
	/* Callback/worker metadata carries the entry itself, once, without a
	 * duplicate pair entry. Reserve its slot only at that real emission. */
	if (op != R5_CALLBACK && op != R5_WORKER && !r5_slot())
		return t;
	t.object_id = id;
	t.object_generation = atomic64_read(&r5_lifetime);
	t.completion_generation = atomic64_read(&r5_objects[id-1].generation);
	t.task_id = (u64)task_pid_nr(current) + 1;
	t.task_generation = current->start_boottime + 1;
	t.call_id = atomic64_inc_return(&r5_calls);
	t.peer_id = peer_id;
	t.operation = op;
	t.pm_phase = phase;
	if (atomic_read(&r5_objects[id-1].resetting) && op != R5_REINIT) {
		t.flags = 1;
		r5obs2_topology_changed();
	}
	if (op != R5_CALLBACK && op != R5_WORKER)
		trace_r5_pair(&t, false, 0);
	return t;
}

static void r5_end(struct r5obs2_token *t, int ret)
{
	if (!t->object_id)
		return;
	if (atomic_read(&r5_state) != 2 ||
	    atomic64_read(&r5_objects[t->object_id-1].generation) != t->completion_generation ||
	    t->task_id != (u64)task_pid_nr(current)+1 ||
	    t->task_generation != current->start_boottime+1)
		t->flags = 1;
	if (r5_slot())
		trace_r5_pair(t, true, ret);
}


/* Auxiliary records share the slot budget and exact task/call identity.
 * Progress is an observation of actual list movement, not a second callback.
 */
static void r5_aux(struct r5obs2_token *t, u64 a, u64 b, u32 code, int ret)
{
    if (!t->call_id)
        return;
    if (atomic_read(&r5_state) != 2 || t->task_id != (u64)task_pid_nr(current)+1 ||
        t->task_generation != current->start_boottime+1)
        t->flags = 1;
    if (r5_slot())
        trace_r5_aux(t, a, b, code, ret);
}

struct r5obs2_token r5obs2_stage_begin(unsigned int stage, u64 value)
{
    struct r5obs2_token t = { };
    if (!atomic_read(&r5_exported) || atomic_read(&r5_state) != 2)
        return t;
    /* Namespace separate from device IDs: object_id=completion_generation=0. */
    t.object_generation = atomic64_read(&r5_lifetime);
    t.task_id = (u64)task_pid_nr(current)+1;
    t.task_generation = current->start_boottime+1;
    t.call_id = atomic64_inc_return(&r5_calls);
    t.operation = R5_STAGE;
    t.pm_phase = R5_PREPARE_PHASE;
    r5_aux(&t, stage, value, R5_STAGE_BEGIN, 0);
    return t;
}
EXPORT_SYMBOL_GPL(r5obs2_stage_begin);

void r5obs2_stage_end(struct r5obs2_token *t, unsigned int stage, u64 value, int ret)
{
    r5_aux(t, stage, value, R5_STAGE_END, ret);
}
EXPORT_SYMBOL_GPL(r5obs2_stage_end);

static unsigned int r5_phase(void)
{
	switch (pm_transition.event) {
	case PM_EVENT_RESUME: case PM_EVENT_RECOVER: case PM_EVENT_THAW: case PM_EVENT_RESTORE:
		return R5_RESUME_PHASE;
	default: return R5_SUSPEND_PHASE;
	}
}

static void r5_complete(struct device *dev)
{
	struct r5obs2_token t = r5_begin(dev, R5_COMPLETE, r5_phase(), NULL);
	complete_all(&dev->power.completion);
	r5_end(&t, 0);
}

static void r5_reinit(struct device *dev)
{
	struct r5obs2_token t = { };
	unsigned int id = atomic_read(&r5_state) == 2 ? r5_find(dev) : 0;
	bool owner = false;
	if (atomic_read(&r5_state) == 2 && id) {
		owner = atomic_cmpxchg(&r5_objects[id-1].resetting, 0, 1) == 0;
		if (!owner)
			r5obs2_topology_changed();
		else if (atomic64_inc_return(&r5_objects[id-1].generation) > 7)
			r5obs2_topology_changed();
		else {
			/* The owner is the one legitimate generation transition. */
			t = r5_begin(dev, R5_REINIT, r5_phase(), NULL);
		}
	}
	reinit_completion(&dev->power.completion);
	r5_end(&t, 0);
	if (owner)
		atomic_set(&r5_objects[id-1].resetting, 0);
}
'''


for _key, _value in {**_analysis.dictionary_limits(_analysis.CAPACITY_PROFILE),
                     'dictionary_cap': _analysis.DICTIONARY_CAP,
                     'dictionary_reserve': _analysis.DICTIONARY_RESERVE}.items():
    SEMANTIC_EMITTER = SEMANTIC_EMITTER.replace('@'+_key.upper()+'@', str(_value))

DETAIL_TABLES = r'''
#include <linux/kallsyms.h>
#include <linux/notifier.h>
/* Pre-trigger immutable dictionaries; pointer identities never go on wire. */
static struct { void *fn; char name[48]; } r5_functions[R5_FUNCTIONS];
static struct { struct notifier_block *nb; u32 function; } r5_notifiers[R5_NOTIFIERS];
static u64 r5_queued_call[R5_OBJECTS];
static unsigned int r5_nf, r5_nn;
static unsigned int r5_function(void *fn)
{
    unsigned int i;
    if (!fn) return 0;
    for (i=0;i<r5_nf;i++) if (r5_functions[i].fn==fn) return i+1;
    return 0;
}
static int r5_function_add(void *fn)
{
    char symbol[KSYM_SYMBOL_LEN];
    unsigned int id=r5_function(fn);
    if (!fn || id) return id;
    if (r5_nf==ARRAY_SIZE(r5_functions))
        return r5_size_error("functions",r5_nf+1,ARRAY_SIZE(r5_functions));
    sprint_symbol_no_offset(symbol,(unsigned long)fn);
    if (strscpy(r5_functions[r5_nf].name,symbol,sizeof(r5_functions[0].name))<0)
        return r5_size_error("function-name",strlen(symbol)+1,sizeof(r5_functions[0].name));
    r5_functions[r5_nf].fn=fn;
    return ++r5_nf;
}
static int r5_ops_add(const struct dev_pm_ops *ops)
{
    unsigned int i;
    void *functions[23];
    if (!ops) return 0;
#define OP(n,f) functions[n]=(void *)ops->f
    OP(0,prepare); OP(1,complete); OP(2,suspend); OP(3,resume);
    OP(4,freeze); OP(5,thaw); OP(6,poweroff); OP(7,restore);
    OP(8,suspend_late); OP(9,resume_early); OP(10,freeze_late); OP(11,thaw_early);
    OP(12,poweroff_late); OP(13,restore_early); OP(14,suspend_noirq); OP(15,resume_noirq);
    OP(16,freeze_noirq); OP(17,thaw_noirq); OP(18,poweroff_noirq); OP(19,restore_noirq);
    OP(20,runtime_suspend); OP(21,runtime_resume); OP(22,runtime_idle);
#undef OP
    for (i=0;i<ARRAY_SIZE(functions);i++) if (r5_function_add(functions[i])<0) return -E2BIG;
    return 0;
}
int r5obs2_notifier_add(struct notifier_block *nb)
{
    int id;
    if (r5_nn==ARRAY_SIZE(r5_notifiers))
        return r5_size_error("notifiers",r5_nn+1,ARRAY_SIZE(r5_notifiers));
    id=r5_function_add((void *)nb->notifier_call);
    if (id<=0) return -EINVAL;
    r5_notifiers[r5_nn].nb=nb;
    r5_notifiers[r5_nn++].function=id;
    return 0;
}
'''

DETAIL_EMITTER = r'''
static unsigned int r5_notifier_id(struct notifier_block *nb)
{
    unsigned int i;
    for(i=0;i<r5_nn;i++) if(r5_notifiers[i].nb==nb) return i+1;
    return 0; /* Non-PM chains are deliberately not part of this profile. */
}
struct r5obs2_token r5obs2_notifier_begin(struct notifier_block *nb, u64 action)
{
    struct r5obs2_token t={};
    unsigned int id;
    if (atomic_read(&r5_state)!=2 || !atomic_read(&r5_exported)) return t;
    id=r5_notifier_id(nb);
    if (!id) return t;
    t.object_generation=atomic64_read(&r5_lifetime);
    t.task_id=(u64)task_pid_nr(current)+1;
    t.task_generation=current->start_boottime+1;
    t.call_id=atomic64_inc_return(&r5_calls);
    t.operation=R5_NOTIFIER;
    r5_aux(&t,id,action,R5_NOTIFIER_BEGIN,0);
    return t;
}
void r5obs2_notifier_end(struct r5obs2_token *t, struct notifier_block *nb, u64 action, int ret)
{ r5_aux(t,r5_notifier_id(nb),action,R5_NOTIFIER_END,ret); }
static void r5_callback_meta(struct r5obs2_token *t, void *fn, const char *info,
                             unsigned int event)
{
    unsigned int layer=0, phase=0, id=r5_function(fn);
    if (!t->call_id) return;
    if (info) {
        if (strstr(info,"power domain")) layer=1;
        else if (strstr(info,"type")) layer=2;
        else if (strstr(info,"class")) layer=3;
        else if (strstr(info,"legacy bus")) layer=6;
        else if (strstr(info,"bus")) layer=4;
        else if (strstr(info,"driver")) layer=5;
        if (strstr(info,"noirq")) phase=3;
        else if (strstr(info,"early")) phase=2;
        else if (strstr(info,"late")) phase=1;
    }
    if (fn && (!id || !layer)) { t->flags=1; r5obs2_topology_changed(); }
    r5_aux(t,id,((u64)event<<16)|(phase<<8)|layer,R5_CALLBACK_ENTRY,0);
}
struct r5obs2_token r5obs2_runtime_begin(struct device *dev)
{ return r5_begin(dev,R5_RUNTIME,r5_phase(),NULL); }
struct r5obs2_token r5obs2_probe_begin(struct device *dev)
{ return r5_begin(dev,R5_PROBE,r5_phase(),NULL); }
void r5obs2_runtime_end(struct r5obs2_token *t, int ret)
{ r5_end(t,ret); }
void r5obs2_wake_note(struct r5obs2_token *t, unsigned int irq, bool pending)
{ r5_aux(t,irq,pending,R5_WAKE_META,0); }
'''

METER_SOURCE = r'''// SPDX-License-Identifier: GPL-2.0-only
/* R47 passive allocator journal. Never uses the measured trace ring.
 * Global, unfiltered observations are a conservative superset, NOT proof of
 * attribution or an upper bound. No allocation/printing inside callbacks.
 * ponytail: one IRQ-safe lock; measure its perturbation before accepting data.
 */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/nmi.h>
#include <linux/slab.h>
#include "../../../mm/slab.h"
#include <trace/events/kmem.h>
#include <trace/events/percpu.h>

#define R5M_SLOTS 8192
/* ABI: little endian x86-64, 88 bytes. Raw addresses remain local evidence. */
struct r5m_record {
    u64 seq, ns, ptr, site, requested, allocated, pfn, birth, extra;
    u32 pid, order, kind, phase;
};
static struct r5m_record r5m_log[R5M_SLOTS];
static DEFINE_RAW_SPINLOCK(r5m_lock);
static DEFINE_MUTEX(r5m_reader);
static u64 r5m_head, r5m_tail, r5m_seq;
static atomic64_t r5m_lost = ATOMIC64_INIT(0);
static atomic_t r5m_opened = ATOMIC_INIT(0);
static int r5m_phase = -1;
static bool r5m_active, r5m_finished;

/* Called under the lock; seq counts attempts, so loss cannot be hidden. */
static void r5m_append(struct r5m_record *r)
{
    r->seq = ++r5m_seq;
    r->ns = ktime_get_mono_fast_ns();
    r->pid = task_pid_nr(current);
    r->birth = current->start_boottime;
    r->phase = r5m_phase;
    if (r5m_head - r5m_tail == R5M_SLOTS) {
        atomic64_inc(&r5m_lost);
        return;
    }
    r5m_log[r5m_head++ % R5M_SLOTS] = *r;
}
static void r5m_emit(struct r5m_record r)
{
    unsigned long flags;
    if (!READ_ONCE(r5m_active)) return;
    if (in_nmi()) { atomic64_inc(&r5m_lost); return; }
    raw_spin_lock_irqsave(&r5m_lock, flags);
    if (r5m_active) r5m_append(&r);
    raw_spin_unlock_irqrestore(&r5m_lock, flags);
}
static void r5m_object(unsigned long site, const void *ptr, size_t req,
                       size_t allocated, u64 cache)
{
    struct page *head;
    struct r5m_record r = { .kind=1, .site=site, .ptr=(u64)ptr,
        .requested=req, .allocated=allocated, .extra=cache };
    if (!READ_ONCE(r5m_active) || ZERO_OR_NULL_PTR(ptr)) return;
    /* Allocator tracepoints supply direct-map slab/large-kmalloc objects. */
    head = compound_head(virt_to_page(ptr));
    r.pfn = page_to_pfn(head);
    r.order = compound_order(head);
    r5m_emit(r);
}
static void r5m_kmalloc(void *unused, unsigned long site, const void *ptr,
                        size_t req, size_t allocated, gfp_t flags, int node)
{ r5m_object(site, ptr, req, allocated, 0); }
static void r5m_cache_alloc(void *unused, unsigned long site, const void *ptr,
                            struct kmem_cache *s, gfp_t flags, int node)
{ r5m_object(site, ptr, s->object_size, s->size, (u64)s); }
static void r5m_free(void *unused, unsigned long site, const void *ptr)
{
    if (!ZERO_OR_NULL_PTR(ptr))
        r5m_emit((struct r5m_record){.kind=2,.ptr=(u64)ptr,.site=site});
}
static void r5m_cache_free(void *unused, unsigned long site, const void *ptr,
                           const struct kmem_cache *s)
{ r5m_free(unused, site, ptr); }
static void r5m_page_alloc(void *unused, struct page *p, unsigned int order,
                           gfp_t flags, int migratetype)
{
    if (p) r5m_emit((struct r5m_record){.kind=3,.pfn=page_to_pfn(p),.order=order,
                                      .extra=(u64)flags});
}
static void r5m_page_free(void *unused, struct page *p, unsigned int order)
{ r5m_emit((struct r5m_record){.kind=4,.pfn=page_to_pfn(p),.order=order}); }
static void r5m_page_batch(void *unused, struct page *p)
{ r5m_page_free(unused,p,0); }
static void r5m_percpu_alloc(void *unused, unsigned long site, bool reserved,
 bool atomic, size_t size, size_t align, void *base, int off,
 void __percpu *ptr, size_t allocated, gfp_t flags)
{
    r5m_emit((struct r5m_record){.kind=5,.site=site,.ptr=(u64)ptr,
        .requested=size,.allocated=allocated,.extra=(u64)base,.order=off});
}
static void r5m_percpu_free(void *unused, void *base, int off, void __percpu *ptr)
{ r5m_emit((struct r5m_record){.kind=6,.ptr=(u64)ptr,.extra=(u64)base,.order=off}); }
static void r5m_chunk_create(void *unused, void *base)
{ r5m_emit((struct r5m_record){.kind=7,.ptr=(u64)base}); }
static void r5m_chunk_destroy(void *unused, void *base)
{ r5m_emit((struct r5m_record){.kind=8,.ptr=(u64)base}); }

static int r5m_open(struct inode *i, struct file *f)
{
    if (!capable(CAP_SYS_ADMIN)) return -EPERM;
    if (atomic_cmpxchg(&r5m_opened,0,1)) return -EBUSY;
    return 0;
}
static int r5m_release(struct inode *i, struct file *f)
{
    unsigned long flags;
    raw_spin_lock_irqsave(&r5m_lock,flags);
    if (r5m_active) { atomic64_inc(&r5m_lost); r5m_active=false; r5m_finished=true; }
    raw_spin_unlock_irqrestore(&r5m_lock,flags);
    atomic_set(&r5m_opened,0);
    return 0;
}
static ssize_t r5m_read(struct file *f, char __user *buf, size_t size, loff_t *off)
{
    struct r5m_record batch[16];
    unsigned long flags;
    unsigned int n, k;
    bool finished;
    if (!capable(CAP_SYS_ADMIN)) return -EPERM;
    if (size < sizeof(batch[0])) return -EINVAL;
    if (mutex_lock_interruptible(&r5m_reader)) return -ERESTARTSYS;
    raw_spin_lock_irqsave(&r5m_lock,flags);
    n = min_t(u64,r5m_head-r5m_tail,min_t(size_t,16,size/sizeof(batch[0])));
    for (k=0;k<n;k++) batch[k]=r5m_log[(r5m_tail+k)%R5M_SLOTS];
    finished=r5m_finished;
    raw_spin_unlock_irqrestore(&r5m_lock,flags);
    if (copy_to_user(buf,batch,n*sizeof(batch[0]))) {
        mutex_unlock(&r5m_reader); return -EFAULT;
    }
    raw_spin_lock_irqsave(&r5m_lock,flags);
    r5m_tail+=n;
    raw_spin_unlock_irqrestore(&r5m_lock,flags);
    mutex_unlock(&r5m_reader);
    return n ? n*sizeof(batch[0]) : (finished ? 0 : -EAGAIN);
}
static long r5m_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    unsigned long flags;
    struct r5m_record r={.kind=0};
    int phase=cmd-0x523500, ret=0;
    if (!capable(CAP_SYS_ADMIN)) return -EPERM;
    if (arg || phase<0 || phase>7) return -EINVAL;
    raw_spin_lock_irqsave(&r5m_lock,flags);
    if (r5m_finished || phase!=r5m_phase+1 || r5m_head-r5m_tail==R5M_SLOTS) {
        ret=-EINVAL; goto out;
    }
    r5m_phase=phase;
    r5m_active=phase<7;
    r5m_finished=phase==7;
    r.requested=sizeof(r5m_log); r.allocated=PAGE_ALIGN(sizeof(r5m_log));
    r.extra=atomic64_read(&r5m_lost);
    r.ptr=0x52354d31; /* R5M1 ABI identity, no arbitrary marker writes */
    r.order=PAGE_SHIFT;
    r.site=num_possible_cpus();
    r5m_append(&r);
out:
    raw_spin_unlock_irqrestore(&r5m_lock,flags);
    return ret;
}
static const struct file_operations r5m_fops={.owner=THIS_MODULE,
    .open=r5m_open,.release=r5m_release,.read=r5m_read,.unlocked_ioctl=r5m_ioctl,
    .llseek=noop_llseek};
static struct miscdevice r5m_dev={.minor=MISC_DYNAMIC_MINOR,.name="r5_meter",
    .fops=&r5m_fops,.mode=0600};
#define R5M_HOOKS(X) \
 X(kmalloc,r5m_kmalloc) X(kmem_cache_alloc,r5m_cache_alloc) \
 X(kfree,r5m_free) X(kmem_cache_free,r5m_cache_free) \
 X(mm_page_alloc,r5m_page_alloc) X(mm_page_free,r5m_page_free) \
 X(mm_page_free_batched,r5m_page_batch) X(percpu_alloc_percpu,r5m_percpu_alloc) \
 X(percpu_free_percpu,r5m_percpu_free) X(percpu_create_chunk,r5m_chunk_create) \
 X(percpu_destroy_chunk,r5m_chunk_destroy)
static int __init r5m_init(void)
{
    int ret,done=0,index=0;
    BUILD_BUG_ON(sizeof(struct r5m_record)!=88);
#define ATTACH(event,fn) ret=register_trace_##event(fn,NULL); if(ret) goto fail; done++;
    R5M_HOOKS(ATTACH)
#undef ATTACH
    ret=misc_register(&r5m_dev);
    if (!ret) return 0;
fail:
#define DETACH(event,fn) if(index++<done) unregister_trace_##event(fn,NULL);
    R5M_HOOKS(DETACH)
#undef DETACH
    tracepoint_synchronize_unregister();
    return ret;
}
late_initcall(r5m_init);
'''

SEMANTIC_LOCK = {
    'include/trace/events/kmem.h': '4b51b144563f7790e01290ccc41743711abed9f30e628e5f761b20bd8a61ffda',
    'include/trace/events/percpu.h': '16ff39044a649eb1de63f122dbbc97e166be1fe40bd5aaaabac8d24ab9fed41f',
    'mm/slab.h': '2ba8cb201c943475a5010a7c5cdd2e72ef62e504f47a9d585e4f3a6aa88712de',
    'mm/slub.c': '7f94d9306c51c97f7412a906fe37c8d2a0d6d1f090aeba6b289b4ab25b018e70',
    'mm/page_alloc.c': '3f9d3738901812a3bc5431ec20aa74c471fdc92fd549b1ae3eadf4ef796c9652',
    'mm/percpu.c': 'b74279d0cd0d8a3b31f26f0892a1bbb718f32812ee8ffb44f3f71199584964e0',
    'drivers/base/power/runtime.c': 'c914bbc53e8f05cdc8fb887e48f0af514bcbd8c152fec69515055705ae8e4d15',
    'kernel/notifier.c': '7c9a7a5d299f8673744826e6a0f54b6ca11aae7f318d23a80af4438ba2839661',
    'drivers/base/dd.c': 'd7321d96a19b4dbf2fdea8bae5ac3cbd1956a775668c70f34845c6594cab6689',
    'drivers/base/power/Makefile': '7e5c4b1addd63149e139a9197153e4317a2b99783e3af622eeb2e7ec298110a4',
    'drivers/base/power/main.c': 'e84341387dced0c3c64377b714c66af98a7338226ddd1cb7e861ba5a20caff42',
    'include/trace/events/power.h': '62b7be15bf5e70af7cfc20071b50b1016a9a33adc2e184c17e46a265794f86e8',
    'drivers/base/core.c': 'efbd4c9ba455cc068678b440d04554f24fa459c391ff935c9071ec4d811c29b5',
    'Makefile': LOCK['Makefile'],
    'kernel/power/main.c': 'a94a32d69a95deabbd9c0c185ea511c18fa832f734d779930b7090f7389d74ff',
    'kernel/power/suspend.c': '52d4d2085285052d4f55c28798a0e797a3812b4d3c93dc7c38da560405c0255b',
    'drivers/base/base.h': 'f0727aa65d08af024bda1fd5ac75b8a40a91c05b7a6160f5859203f1b96301ba',
}

def semantic_overlay(files):
    result = dict(files)
    p = 'include/trace/events/power.h'
    result[p] = replace_once(files[p], 'TRACE_EVENT(r5_prepare_progress,',
                            SEMANTIC_EVENTS+'\nTRACE_EVENT(r5_prepare_progress,')
    result[p] = '#include <linux/r5obs2.h>\n'+result[p]
    result['include/linux/r5obs2.h'] = SEMANTIC_HEADER
    result['drivers/base/power/r5obs2-emitter.h'] = SEMANTIC_EMITTER
    p = 'drivers/base/power/main.c'
    t = replace_once(files[p], 'static int async_error;',
                    'static int async_error;\n#include "r5obs2-emitter.h"')
    # Every existing completion/reset site keeps its original primitive.
    assert t.count('complete_all(&dev->power.completion);') == 7
    t = t.replace('complete_all(&dev->power.completion);', 'r5_complete(dev);')
    t = replace_once(t, 'reinit_completion(&dev->power.completion);', 'r5_reinit(dev);')
    for function in ('device_pm_add', 'device_pm_remove'):
        anchor = 'void '+function+'(struct device *dev)\n{'
        t = replace_once(t, anchor, anchor+'\n\tr5obs2_topology_changed();')
    for mutation in ('list_add_tail(&dev->power.entry, &dpm_list);',
                     'list_del_init(&dev->power.entry);'):
        t = replace_once(t, '\t'+mutation, '\tr5obs2_topology_changed();\n\t'+mutation)
    t = replace_once(t, 'static void dpm_wait(struct device *dev, bool async)',
                    'static void dpm_wait(struct device *dev, bool async, struct device *peer)')
    t = replace_once(t, '\tif (async || (pm_async_enabled && dev->power.async_suspend))\n'
                        '\t\twait_for_completion(&dev->power.completion);',
                    '\tif (async || (pm_async_enabled && dev->power.async_suspend)) {\n'
                    '\t\tstruct r5obs2_token t = r5_begin(dev, R5_WAIT, r5_phase(), peer);\n'
                    '\t\twait_for_completion(&dev->power.completion);\n\t\tr5_end(&t, 0);\n\t}')
    for before, after in [
        ('dpm_wait(dev, *((bool *)async_ptr));', 'dpm_wait(dev, *((bool *)async_ptr), NULL);'),
        ('dpm_wait(link->supplier, async);', 'dpm_wait(link->supplier, async, dev);'),
        ('dpm_wait(link->consumer, async);', 'dpm_wait(link->consumer, async, dev);'),
        ('dpm_wait(parent, async);', 'dpm_wait(parent, async, dev);'),
        ('dpm_wait(dev, subordinate->power.async_suspend);',
         'dpm_wait(dev, subordinate->power.async_suspend, subordinate);')]:
        t = replace_once(t, before, after)
    t = replace_once(t, '\tktime_t calltime;\n\tint error;',
                    '\tktime_t calltime;\n\tint error;\n\tstruct r5obs2_token r5;')
    t = replace_once(t, '\terror = cb(dev);',
                    '\tr5 = r5_begin(dev, R5_CALLBACK, r5_phase(), NULL);\n'
                    '\terror = cb(dev);\n\tr5_end(&r5, error);')
    # Only the ordinary resume entry; explicit scope, not all phases inferred.
    start = t.index('static void device_resume(struct device *dev,')
    stop = t.index('static void async_resume(', start)
    part = t[start:stop]
    part = replace_once(part, '\tint error = 0;',
                        '\tint error = 0;\n\tstruct r5obs2_token r5_resume, r5_lock;')
    part = replace_once(part, '\tTRACE_DEVICE(dev);',
                        '\tr5_resume = r5_begin(dev, R5_RESUME, R5_RESUME_PHASE, NULL);\n\tTRACE_DEVICE(dev);')
    part = replace_once(part, '\tdevice_lock(dev);',
                        '\tr5_lock = r5_begin(dev, R5_LOCK, R5_RESUME_PHASE, NULL);\n'
                        '\tdevice_lock(dev);\n\tr5_end(&r5_lock, 0);')
    part = replace_once(part, '\tTRACE_RESUME(error);',
                        '\tr5_end(&r5_resume, error);\n\tTRACE_RESUME(error);')
    t = t[:start]+part+t[stop:]
    t = replace_once(t, '\t\terror = device_prepare(dev, state);',
                    '\t\t{\n\t\t\tstruct r5obs2_token r5 = r5_begin(dev, R5_PREPARE, R5_PREPARE_PHASE, NULL);\n'
                    '\t\t\terror = device_prepare(dev, state);\n\t\t\tr5_end(&r5, error);\n\t\t}')
    result[p] = t
    p = 'drivers/base/base.h'
    t = '#include <linux/r5obs2.h>\n'+files[p]
    t = replace_once(t, '\tWRITE_ONCE(dev->driver, (struct device_driver *)drv);',
                     '\tr5obs2_topology_changed();\n\tWRITE_ONCE(dev->driver, (struct device_driver *)drv);')
    result[p] = t
    p = 'drivers/base/core.c'
    t = '#include <linux/r5obs2.h>\n'+files[p]
    # Shared writer gate covers add/delete/autoremove and status transitions.
    anchor = 'static inline void device_links_write_lock(void)\n{'
    t = replace_once(t, anchor+'\n\tmutex_lock(&device_links_lock);',
                     anchor+'\n\tmutex_lock(&device_links_lock);\n\tr5obs2_topology_changed();')
    anchor = 'int device_links_read_lock(void) __acquires(&device_links_srcu)'
    t = replace_once(t, anchor,
        '#ifdef CONFIG_PM_SLEEP\n'
        'void r5obs2_links_lock(void) { mutex_lock(&device_links_lock); }\n'
        'void r5obs2_links_unlock(void) { mutex_unlock(&device_links_lock); }\n'
        '#endif\n\n'+anchor)
    for anchor in ('\tstruct kobject *new_parent_kobj;',):
        t = replace_once(t, anchor, anchor+'\n\n\tr5obs2_topology_changed();')
    t = replace_once(t, '\tdevice_pm_lock();\n\tnew_parent = get_device(new_parent);',
                     '\tdevice_pm_lock();\n\tr5obs2_topology_changed();\n\tnew_parent = get_device(new_parent);')
    # Rename invalidates human-readable census labels too.
    anchor = 'int device_rename(struct device *dev, const char *new_name)\n{'
    # Add at first statement, after declarations (kernel warning discipline).
    pos = t.index(anchor); end = t.index('\n}', pos)
    part = t[pos:end]
    part = replace_once(part, '\tdev = get_device(dev);',
                        '\tr5obs2_topology_changed();\n\tdev = get_device(dev);')
    part = replace_once(part, '\treturn error;',
                        '\tr5obs2_topology_changed();\n\treturn error;')
    t = t[:pos]+part+t[end:]

    result[p] = t
    p = 'drivers/base/power/main.c'
    t = result[p]
    # The pair call survives to the list movement after device_prepare returns.
    t = replace_once(t, '\t\tint r5_ret;', '\t\tint r5_ret;\n\t\tstruct r5obs2_token r5_prepare;')
    t = replace_once(t, '\t\t\tstruct r5obs2_token r5 = r5_begin(dev, R5_PREPARE, R5_PREPARE_PHASE, NULL);',
                     '\t\t\tr5_prepare = r5_begin(dev, R5_PREPARE, R5_PREPARE_PHASE, NULL);')
    t = replace_once(t, '\t\t\tr5_end(&r5, error);', '\t\t\tr5_end(&r5_prepare, error);')
    anchor = '\t\ttrace_r5_prepare_progress(dev, ++r5_attempt, r5_moved,\n\t\t\t\t\t  r5_did_move, r5_ret);'
    t = replace_once(t, anchor, anchor+'\n\t\tr5_aux(&r5_prepare, r5_attempt, (r5_moved << 1) | r5_did_move, R5_PROGRESS, r5_ret);')
    # Explicit skips do not create fake wait events.
    start=t.index('static void dpm_wait(');end=t.index('static int dpm_wait_fn(',start)
    part=t[start:end]
    part=replace_once(part,'\tif (dev->power.no_pm)\n\t\treturn;',
        '\tif (dev->power.no_pm) {\n\t\tstruct r5obs2_token t = r5_begin(dev, R5_SKIP, r5_phase(), peer);\n'
        '\t\tr5_end(&t, 1); /* no_pm */\n\t\treturn;\n\t}')
    part=replace_once(part,'\t\tr5_end(&t, 0);\n\t}',
        '\t\tr5_end(&t, 0);\n\t} else {\n'
        '\t\tstruct r5obs2_token t = r5_begin(dev, R5_SKIP, r5_phase(), peer);\n'
        '\t\tr5_end(&t, 2); /* async condition false */\n\t}')
    t=t[:start]+part+t[end:]
    # Whole superior boundary includes both parent and supplier waits.
    start=t.index('static bool dpm_wait_for_superior(');end=t.index('static void dpm_wait_for_consumers(',start)
    part=t[start:end]
    part=replace_once(part,'\tstruct device *parent;',
        '\tstruct device *parent;\n\tbool ready;\n\tstruct r5obs2_token r5 = r5_begin(dev, R5_SUPERIOR, r5_phase(), NULL);')
    part=replace_once(part,'\t\treturn false;', '\t\tr5_end(&r5, 0);\n\t\treturn false;')
    part=replace_once(part,'\treturn device_pm_initialized(dev);',
        '\tready = device_pm_initialized(dev);\n\tr5_end(&r5, ready);\n\treturn ready;')
    t=t[:start]+part+t[end:]
    # Queue result never implies worker execution; task pair records actual work.
    start=t.index('static bool dpm_async_fn(');end=t.index('\n/**',start)
    part=t[start:end]
    part=replace_once(part,'{\n\tr5_reinit(dev);',
        '{\n\tstruct r5obs2_token r5;\n\tbool enabled;\n\tr5_reinit(dev);\n'
        '\tr5 = r5_begin(dev, R5_TASK, r5_phase(), NULL);\n\tenabled = is_async(dev);')
    part=replace_once(part,'\tif (is_async(dev)) {','\tif (enabled) {')
    part=replace_once(part,'\t\tif (async_schedule_dev_nocall(func, dev))\n\t\t\treturn true;',
        '\t\tif (async_schedule_dev_nocall(func, dev)) {\n'
        '\t\t\tr5_aux(&r5, 1, 1, R5_ASYNC, 0);\n\t\t\tr5_end(&r5, 1);\n\t\t\treturn true;\n\t\t}')
    part=replace_once(part,'\treturn false;',
        '\tr5_aux(&r5, enabled, 0, R5_ASYNC, 0);\n\tr5_end(&r5, 0);\n\treturn false;')
    t=t[:start]+part+t[end:]
    start=t.index('static void device_resume(');end=t.index('static void async_resume(',start)
    part=t[start:end]
    part=replace_once(part,'\tdevice_unlock(dev);',
        '\tr5_lock = r5_begin(dev, R5_UNLOCK, R5_RESUME_PHASE, NULL);\n\tdevice_unlock(dev);\n\tr5_end(&r5_lock, 0);')
    # Resume exit includes explicit skip reason via auxiliary enum, separate from errno.
    for condition,reason in [('dev->power.syscore',3),('!dev->power.is_suspended',4),('!dpm_wait_for_superior(dev, async)',6)]:
        anchor='\tif ('+condition+')\n\t\tgoto Complete;'
        part=replace_once(part,anchor,'\tif ('+condition+') {\n'
            '\t\tstruct r5obs2_token skip = r5_begin(dev, R5_SKIP, R5_RESUME_PHASE, NULL);\n'
            f'\t\tr5_end(&skip, {reason});\n\t\tgoto Complete;\n\t}}')
    part=replace_once(part,'\t\tpm_runtime_enable(dev);',
        '\t\tstruct r5obs2_token skip = r5_begin(dev, R5_SKIP, R5_RESUME_PHASE, NULL);\n'
        '\t\tpm_runtime_enable(dev);\n\t\tr5_end(&skip, 5);')
    t=t[:start]+part+t[end:]
    result[p]=t
    # Add narrow stage wrappers around existing statements. Stage IDs are
    # defined in the production parser contract; no tracepoint registration.
    def stage_call(text, statement, stage, value, ret, count=1):
        if text.count(statement) != count:
            raise ValueError('stage statement identity drift')
        return text.replace(statement, '{\n\t\tstruct r5obs2_token r5 = r5obs2_stage_begin('
            +str(stage)+', '+value+');\n\t\t'+statement+'\n\t\tr5obs2_stage_end(&r5, '
            +str(stage)+', '+value+', '+ret+');\n\t}')
    p='kernel/power/suspend.c';t='#include <linux/r5obs2.h>\n'+files[p]
    for statement,stage,value,ret,count in [
        ('pm_prepare_console();',1,'state','0',1),
        ('error = suspend_freeze_processes();',2,'state','error',1),
        ('suspend_console();',3,'state','0',1),
        ('resume_console();',4,'state','0',1),
        ('pm_restore_console();',5,'0','0',2),
        ('suspend_thaw_processes();',6,'0','0',1),
        ('error = suspend_ops->enter(state);',7,'state','error',1),
        ('platform_recover(state);',8,'state','0',1),
        ('error = enter_state(state);',9,'state','error',1),
        ('error = suspend_devices_and_enter(state);',10,'state','error',1)]:
        t=stage_call(t,statement,stage,value,ret,count)
    result[p]=t
    p='kernel/power/main.c';t='#include <linux/r5obs2.h>\n'+files[p]
    t=stage_call(t,'ret = blocking_notifier_call_chain_robust(&pm_chain_head, val_up, val_down, NULL);',
                 11,'val_up','ret')
    t=replace_once(t,'\treturn blocking_notifier_call_chain(&pm_chain_head, val, NULL);',
                   '\tint ret;\n\tret = blocking_notifier_call_chain(&pm_chain_head, val, NULL);\n\treturn ret;')
    t=stage_call(t,'ret = blocking_notifier_call_chain(&pm_chain_head, val, NULL);',12,'val','ret')
    result[p]=t
    return result


BULK_FREE_TRACE = '''
    /* R47 measurement: public bulk frees bypass the single-object tracepoint.
     * Capture pointers before build_detached_freelist permutes the array.
     * kfree accepts NULL cache/large objects; no cache-name dereference here. */
    if (trace_kfree_enabled()) {
        size_t r5_i;
        for (r5_i = 0; r5_i < size; r5_i++)
            trace_kfree(_RET_IP_, p[r5_i]);
    }
'''
BULK_ALLOC_TRACE = '''
    /* Only successful, post-hook allocations reach the caller. Internal
     * failed-batch temporaries still require separate attribution evidence. */
    if (trace_kmem_cache_alloc_enabled()) {
        int r5_i;
        for (r5_i = 0; r5_i < i; r5_i++)
            trace_kmem_cache_alloc(_RET_IP_, p[r5_i], s, flags, NUMA_NO_NODE);
    }
'''


def metered_overlay(result, files):
    """Complete the same semantic producer; no second test-only implementation."""
    result['drivers/base/power/r5-meter.c'] = METER_SOURCE
    result['drivers/base/power/Makefile'] = files['drivers/base/power/Makefile'] + '\nobj-$(CONFIG_PM_SLEEP) += r5-meter.o\n'
    # Extend the actual allocator paths; never infer missing frees in replay.
    slab = files['mm/slub.c']
    anchor = 'void kmem_cache_free_bulk(struct kmem_cache *s, size_t size, void **p)\n{'
    slab = replace_once(slab, anchor, anchor + BULK_FREE_TRACE)
    anchor = '\treturn i;\n}\nEXPORT_SYMBOL(kmem_cache_alloc_bulk_noprof);'
    slab = replace_once(slab, anchor, BULK_ALLOC_TRACE + anchor)
    # __do_kmalloc_node already traces its caller. The exported large wrapper
    # also traces, so calling that wrapper records one allocation twice.
    slab = replace_once(slab, '\t\tret = __kmalloc_large_node_noprof(size, flags, node);',
                        '\t\tret = ___kmalloc_large_node(size, flags, node);')
    result['mm/slub.c'] = slab
    t = result['drivers/base/power/r5obs2-emitter.h']
    t = replace_once(t, 'int r5obs2_census(void)', DETAIL_TABLES+'\nint r5obs2_census(void)')
    t = replace_once(t, '\tBUILD_BUG_ON(sizeof(r5_objects) + sizeof(r5_edges) > R5_DICTIONARY_CAP);', '''
    BUILD_BUG_ON(sizeof(r5_functions[0]) != 56 || sizeof(r5_notifiers[0]) != 16);
    BUILD_BUG_ON(sizeof(r5_queued_call[0]) != 8);
    BUILD_BUG_ON(ARRAY_SIZE(r5_queued_call) != R5_OBJECTS);
    BUILD_BUG_ON(sizeof(r5_objects) + sizeof(r5_edges) + sizeof(r5_queued_call) +
                 sizeof(r5_functions) + sizeof(r5_notifiers) + R5_DICTIONARY_RESERVE > R5_DICTIONARY_CAP);''')
    t = replace_once(t, '\t\t\tdevice_unlock(o->dev);', '''
            if (!ret && (r5_ops_add(o->dev->pm_domain ? &o->dev->pm_domain->ops : NULL) ||
                r5_ops_add(o->dev->type ? o->dev->type->pm : NULL) ||
                r5_ops_add(o->dev->class ? o->dev->class->pm : NULL) ||
                r5_ops_add(o->dev->bus ? o->dev->bus->pm : NULL) ||
                r5_ops_add(o->dev->driver ? o->dev->driver->pm : NULL) ||
                (o->dev->bus && r5_function_add((void *)o->dev->bus->resume)<0)))
                ret=-E2BIG;
            device_unlock(o->dev);''')
    t = replace_once(t, '\tif (ret || atomic_cmpxchg(&r5_state, 1, 2) != 1)',
                    '\tif (!ret) ret=r5obs2_pm_census();\n\tif (ret || atomic_cmpxchg(&r5_state, 1, 2) != 1)')
    t = replace_once(t, '\ttrace_r5_dictionary(r5_n, r5_e, 0, life, 2,', '''
    for(i=0;i<r5_nf;i++) {
        if (!r5_slot()) return -E2BIG;
        trace_r5_dictionary(i+1,0,0,life,3,r5_functions[i].name,"");
    }
    for(i=0;i<r5_nn;i++) {
        if (!r5_slot()) return -E2BIG;
        trace_r5_dictionary(i+1,r5_notifiers[i].function,0,life,4,"PM_NOTIFIER","");
    }
    if (!r5_slot()) return -E2BIG;
    trace_r5_dictionary(r5_nf,r5_nn,0,life,5,"DETAIL_COUNTS_R40_V2","");
    trace_r5_dictionary(r5_n, r5_e, 0, life, 2,''')
    result['drivers/base/power/r5obs2-emitter.h'] = t+DETAIL_EMITTER
    p='drivers/base/power/main.c';t=result[p]
    t=replace_once(t,'static int dpm_wait_fn(struct device *dev, void *async_ptr)',
        'struct r5_child_wait { bool async; struct device *parent; };\n'
        'static int dpm_wait_fn(struct device *dev, void *async_ptr)')
    t=replace_once(t,'dpm_wait(dev, *((bool *)async_ptr), NULL);',
        'struct r5_child_wait *context=async_ptr;\n\tdpm_wait(dev, context->async, context->parent);')
    t=replace_once(t,'       device_for_each_child(dev, &async, dpm_wait_fn);',
        '\tstruct r5_child_wait context={async,dev};\n\tdevice_for_each_child(dev, &context, dpm_wait_fn);')
    t=replace_once(t,'\tif (!cb)\n\t\treturn 0;',
        '\tr5 = r5_begin(dev, R5_CALLBACK, r5_phase(), NULL);\n'
        '\tr5_callback_meta(&r5,(void *)cb,info,state.event);\n'
        '\tif (!cb) { r5_end(&r5,0); return 0; }')
    t=replace_once(t,'\tr5 = r5_begin(dev, R5_CALLBACK, r5_phase(), NULL);\n\terror = cb(dev);','\terror = cb(dev);')
    t=replace_once(t,'\tenabled = is_async(dev);','\tenabled = is_async(dev);\n'
        '\tif (r5.object_id) WRITE_ONCE(r5_queued_call[r5.object_id-1],r5.call_id);')
    for name in ('resume_noirq','resume_early','resume','suspend_noirq','suspend_late','suspend'):
        start=t.index('static void async_'+name+'(');end=t.index('\n}',start)+2
        part=t[start:end]
        part=replace_once(part,'\tstruct device *dev = data;',
            '\tstruct device *dev = data;\n\tstruct r5obs2_token r5 = r5_begin(dev,R5_WORKER,r5_phase(),NULL);\n'
            '\tif (r5.object_id) r5_aux(&r5,READ_ONCE(r5_queued_call[r5.object_id-1]),cookie,R5_WORKER_ENTRY,0);')
        part=replace_once(part,'\tput_device(dev);','\tr5_end(&r5,0);\n\tput_device(dev);')
        t=t[:start]+part+t[end:]
    t=replace_once(t,'\tpm_runtime_barrier(dev);',
        '\t{ struct r5obs2_token r5 = r5_begin(dev,R5_RUNTIME,r5_phase(),NULL);\n'
        '\t  int ret=pm_runtime_barrier(dev); r5_end(&r5,ret); }')
    # Prepare/complete bypass dpm_run_callback. Preserve their real return
    # contracts (complete is void); do not label device_prepare's sanitized
    # return as the callback return.
    start=t.index('static int device_prepare(');end=t.index('\n/**',start)
    part=t[start:end]
    part=replace_once(part,'\tint ret = 0;', '\tint ret = 0;\n\tconst char *r5_info=NULL;')
    for test,expr,info in [('dev->pm_domain','dev->pm_domain->ops.prepare','power domain '),
        ('dev->type && dev->type->pm','dev->type->pm->prepare','type '),
        ('dev->class && dev->class->pm','dev->class->pm->prepare','class '),
        ('dev->bus && dev->bus->pm','dev->bus->pm->prepare','bus '),
        ('!callback && dev->driver && dev->driver->pm','dev->driver->pm->prepare','driver ')]:
        part=replace_once(part,'if ('+test+')\n\t\tcallback = '+expr+';',
            'if ('+test+') {\n\t\tr5_info="'+info+'"; callback = '+expr+';\n\t}')
    part=replace_once(part,'\tif (callback)\n\t\tret = callback(dev);',
        '\t{ struct r5obs2_token r5=r5_begin(dev,R5_CALLBACK,R5_PREPARE_PHASE,NULL);\n'
        '\t  r5_callback_meta(&r5,(void *)callback,r5_info,state.event);\n'
        '\t  if (callback) ret=callback(dev);\n\t  r5_end(&r5,ret); }')
    t=t[:start]+part+t[end:]
    t=replace_once(t,'\tif (callback) {\n\t\tpm_dev_dbg(dev, state, info);\n\t\tcallback(dev);\n\t}',
        '\t{ struct r5obs2_token r5=r5_begin(dev,R5_CALLBACK,R5_COMPLETE_PHASE,NULL);\n'
        '\t  r5_callback_meta(&r5,(void *)callback,info,state.event);\n'
        '\t  if (callback) { pm_dev_dbg(dev,state,info); callback(dev); }\n\t  r5_end(&r5,0); }')
    # All six phase summaries are paired even though only ordinary resume
    # carries detail. The scope encloses its final async_synchronize_full().
    for stage,name,result_type in [(18,'resume_noirq','void'),(19,'resume_early','void'),
        (20,'resume','void'),(21,'suspend_noirq','int'),(22,'suspend_late','int'),(23,'suspend','int')]:
        signature=result_type+' dpm_'+name+'(pm_message_t state)'
        t=replace_once(t,signature,'static '+signature.replace('dpm_'+name,'dpm_'+name+'_r5_body'))
        start=t.index('static '+signature.replace('dpm_'+name,'dpm_'+name+'_r5_body'))
        end=t.index('\n}',start)+2
        wrapper='\n'+signature+'\n{\n struct r5obs2_token t=r5obs2_stage_begin('+str(stage)+',state.event);\n'
        if name=='resume': wrapper+=' WRITE_ONCE(r5_ordinary,true);\n'
        wrapper+=(' int ret=' if result_type=='int' else ' ')+'dpm_'+name+'_r5_body(state);\n'
        if name=='resume': wrapper+=' WRITE_ONCE(r5_ordinary,false);\n'
        wrapper+=' r5obs2_stage_end(&t,'+str(stage)+',state.event,'+('ret' if result_type=='int' else '0')+');\n'
        if result_type=='int': wrapper+=' return ret;\n'
        t=t[:end]+wrapper+'}\n'+t[end:]
    result[p]=t
    p='kernel/power/main.c';t=result[p]
    anchor='static BLOCKING_NOTIFIER_HEAD(pm_chain_head);'
    t=replace_once(t,anchor,anchor+'''
int r5obs2_pm_census(void)
{
    struct notifier_block *nb;
    int ret=0;
    down_read(&pm_chain_head.rwsem);
    for(nb=pm_chain_head.head;nb;nb=nb->next) {
        ret=r5obs2_notifier_add(nb);
        if(ret) break;
    }
    up_read(&pm_chain_head.rwsem);
    return ret;
}
''')
    for call in ('blocking_notifier_chain_register','blocking_notifier_chain_unregister'):
        t=replace_once(t,'\treturn '+call+'(&pm_chain_head, nb);',
            '\tint ret;\n\tr5obs2_topology_changed();\n\tret='+call+'(&pm_chain_head, nb);\n'
            '\tr5obs2_topology_changed();\n\treturn ret;')
    result[p]=t
    p='kernel/notifier.c';t='#include <linux/r5obs2.h>\n'+files[p]
    t=replace_once(t,'\tvoid *r5_cb;','\tvoid *r5_cb;\n\tstruct r5obs2_token r5_token;')
    t=replace_once(t,'\t\tret = nb->notifier_call(nb, val, v);',
        '\t\tr5_token=r5obs2_notifier_begin(nb,val);\n'
        '\t\tret = nb->notifier_call(nb, val, v);\n'
        '\t\tr5obs2_notifier_end(&r5_token,nb,val,ret);')
    result[p]=t
    # Reuse stages for each constituent wait, keeping actual initial count in
    # both ends. A count of zero is not a claim that a driver was probed.
    p='drivers/base/dd.c';t='#include <linux/r5obs2.h>\n'+files[p]
    start=t.index('void wait_for_device_probe(void)');end=t.index('\n}',start)+2
    part=t[start:end]
    for statement,stage in [('flush_work(&deferred_probe_work);',13),
        ('wait_event(probe_waitqueue, atomic_read(&probe_count) == 0);',14),('async_synchronize_full();',15)]:
        part=replace_once(part,statement,'{ struct r5obs2_token t=r5obs2_stage_begin('
            +str(stage)+',atomic_read(&probe_count)); u64 initial=atomic_read(&probe_count);\n'
            '/* Count can change; reuse exactly the entry value via stage token helper below. */\n'
            +statement+'\nr5obs2_stage_end(&t,'+str(stage)+',initial,0); }')
        # Read once: avoid observational mismatch when probes finish concurrently.
        part=part.replace('struct r5obs2_token t=r5obs2_stage_begin('+str(stage)+
            ',atomic_read(&probe_count)); u64 initial=atomic_read(&probe_count);',
            'u64 initial=atomic_read(&probe_count); struct r5obs2_token t=r5obs2_stage_begin('+str(stage)+',initial);')
    t=t[:start]+part+t[end:]
    # Pair the full probe attempt, including every early failure. Census
    # invalidation still takes precedence over any claim of stable topology.
    name='__driver_probe_device';anchor='static int '+name+'(const struct device_driver *drv, struct device *dev)'
    t=replace_once(t,anchor,anchor.replace(name,name+'_r5_body'))
    at=t.index('\n/**\n * driver_probe_device')
    t=t[:at]+'''\nstatic int __driver_probe_device(const struct device_driver *drv, struct device *dev)
{
    struct r5obs2_token t=r5obs2_probe_begin(dev);
    int ret=__driver_probe_device_r5_body(drv,dev);
    r5obs2_runtime_end(&t,ret);
    return ret;
}
'''+t[at:]
    result[p]=t
    p='drivers/base/power/runtime.c';t='#include <linux/r5obs2.h>\n'+files[p]
    anchor='static void __pm_runtime_barrier(struct device *dev)'
    t=replace_once(t,anchor,anchor.replace('__pm_runtime_barrier','__pm_runtime_barrier_r5_body'))
    at=t.index('\n/**',t.index('__pm_runtime_barrier_r5_body'))
    t=t[:at]+'''\nstatic void __pm_runtime_barrier(struct device *dev)
{
    struct r5obs2_token t=r5obs2_runtime_begin(dev);
    __pm_runtime_barrier_r5_body(dev);
    r5obs2_runtime_end(&t,0);
}
'''+t[at:];result[p]=t
    p='kernel/power/suspend.c';t=result[p]
    # Whole suspend_enter return and s2idle wait; no added wakeup read changes
    # the source's original decision. This records return, not wake source ID.
    for signature,name,args,stage in [
        ('static int suspend_enter(suspend_state_t state, bool *wakeup)','suspend_enter','state, wakeup',16),
        ('static void s2idle_enter(void)','s2idle_enter','',17)]:
        t=replace_once(t,signature,signature.replace(name,name+'_r5_body'))
        start=t.index(signature.replace(name,name+'_r5_body'));end=t.index('\n}',start)+2
        is_int='static int' in signature
        wrapper='\n'+signature+'\n{\n struct r5obs2_token t=r5obs2_stage_begin('+str(stage)+',0);\n '
        wrapper+=('int ret=' if is_int else '')+name+'_r5_body('+args+');\n '
        if stage==16: wrapper+='r5obs2_wake_note(&t,pm_wakeup_irq(),*wakeup);\n '
        wrapper+='r5obs2_stage_end(&t,'+str(stage)+',0,'+('ret' if is_int else '0')+');\n'
        if is_int: wrapper+=' return ret;\n'
        t=t[:end]+wrapper+'}\n'+t[end:]
    result[p]=t
    return result


def prepare(source, output, semantic=False):
    source = source.resolve(strict=True)
    output = output.absolute()
    if output.exists() or output.is_symlink() or output.parent.resolve() != output.parent:
        raise ValueError('output must be new and its parent must not traverse symlinks')
    if source == output or source in output.parents or output in source.parents:
        raise ValueError('source/output must be disjoint')
    files = {}
    lock = SEMANTIC_LOCK if semantic else LOCK
    for name, expected in lock.items():
        path = source/name
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('source identity mismatch: '+name)
        files[name] = path.read_text()
    changed = metered_overlay(semantic_overlay(files), files) if semantic else overlay(files)
    # Never copy existing signing secrets, build products or a source-tree config.
    shutil.copytree(source, output, symlinks=True,
                    ignore=shutil.ignore_patterns('*.pem', '*.key', '*.p12', '*.pfx',
                                                 '*.o', '*.ko', '.*.cmd', '.config*', '.git'))
    rows = []
    for name, text in changed.items():
        if text == files.get(name):
            continue
        (output/name).write_text(text)
        rows.append({'path': name, 'before': lock.get(name),
                     'after': hashlib.sha256((output/name).read_bytes()).hexdigest()})
    return {'generation': 'r5obs2' if semantic else 'r5obs1', 'changed': rows, 'input': lock,
            'install': 'NOT_RUN', 'PM': 'NOT_RUN', 'R5': 'FAIL'}

def prepare_lookup(output):
    """Emit a userspace surrogate of the exact table/search, never kernel PM."""
    start=SEMANTIC_EMITTER.index('struct r5_object {')
    end=SEMANTIC_EMITTER.index('struct r5_edge',start)
    table=SEMANTIC_EMITTER[start:end]
    start=SEMANTIC_EMITTER.index('static unsigned int r5_find(')
    end=SEMANTIC_EMITTER.index('/* Called only while census',start)
    search=SEMANTIC_EMITTER[start:end]
    code=r'''// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
typedef uint32_t u32;
typedef struct { int counter; } atomic_t;
typedef struct { long counter; } atomic64_t;
#define R5_OBJECTS @OBJECTS@
struct device { unsigned char unused; } devices[R5_OBJECTS+1];
@TABLE@
static struct r5_object r5_objects[R5_OBJECTS];
static unsigned int r5_n;
@SEARCH@
/* Prevent loop hoisting without changing the production search body. */
__attribute__((noinline)) static unsigned int trial(struct device *p) {
    __asm__ volatile("" : "+r"(p) : : "memory");
    return r5_find(p);
}
static uint64_t now(void) {
    struct timespec t; assert(clock_gettime(CLOCK_MONOTONIC_RAW,&t)==0);
    return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;
}
int main(void) {
    _Static_assert(sizeof(struct r5_object)==136,"must match compiled kernel BTF");
    _Static_assert(offsetof(struct r5_object,dev)==0,"search stride/offset");
    _Static_assert(offsetof(struct r5_object,name)==28,"same table layout");
    cpu_set_t allowed,one; CPU_ZERO(&allowed);
    if(sched_getaffinity(0,sizeof(allowed),&allowed))return 2;
    int cpu=-1;for(int i=0;i<CPU_SETSIZE;i++)if(CPU_ISSET(i,&allowed)){cpu=i;break;}
    if(cpu<0)return 2;
    CPU_ZERO(&one);CPU_SET(cpu,&one);
    if(sched_setaffinity(0,sizeof(one),&one))return 2;
    for(unsigned i=0;i<R5_OBJECTS;i++)r5_objects[i].dev=&devices[i];
    printf("n,case,rep,cpu,calls,elapsed_ns,ns_per_lookup,checksum\n");
    unsigned sizes[]={0,1,64,256,512,513,1024,R5_OBJECTS};
    for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++) {
        r5_n=sizes[s];
        for(unsigned c=0;c<6;c++)for(unsigned rep=0;rep<7;rep++) {
            const unsigned calls=20000;uint64_t checksum=0;
            struct device *p=c==0?NULL:c==1?&devices[0]:c==2?&devices[r5_n/2]:
                c==3?&devices[r5_n?r5_n-1:0]:&devices[R5_OBJECTS];
            for(unsigned i=0;i<1024;i++)checksum+=trial(p); /* untimed warm-up */
            checksum=0;uint64_t start=now();
            for(unsigned i=0;i<calls;i++)checksum+=trial(c==5?&devices[i%(R5_OBJECTS+1)]:p);
            uint64_t elapsed=now()-start;
            assert(sched_getcpu()==cpu);
            uint64_t expected=0;
            for(unsigned i=0;i<calls;i++) {
                struct device *want=c==5?&devices[i%(R5_OBJECTS+1)]:p;
                for(unsigned j=0;j<r5_n;j++)if(want==&devices[j]){expected+=j+1;break;}
            }
            assert(checksum==expected);
            printf("%u,%u,%u,%d,%u,%llu,%.3f,%llu\n",r5_n,c,rep,cpu,calls,
                   (unsigned long long)elapsed,(double)elapsed/calls,(unsigned long long)checksum);
        }
    }
    return 0;
}
'''.replace('@TABLE@',table).replace('@SEARCH@',search).replace('@OBJECTS@',str(_analysis.MAX_OBJECTS))
    output.mkdir()
    (output/'lookup.c').write_text(code)
    return dict(source_sha256=hashlib.sha256(code.encode()).hexdigest(),
                search_sha256=hashlib.sha256(search.encode()).hexdigest(),
                table_sha256=hashlib.sha256(table.encode()).hexdigest(),
                cases=['null','first','middle','last','miss','uniform_'+str(_analysis.MAX_OBJECTS+1)],
                boundary='userspace same-layout/search surrogate; not kernel timing or WCET',
                PM='NOT_RUN')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--export-module', action='store_true')
    modes.add_argument('--receiver-script', action='store_true')
    modes.add_argument('--semantic-kernel', action='store_true')
    modes.add_argument('--lookup-benchmark', action='store_true')
    args = parser.parse_args()
    if args.lookup_benchmark:
        result = prepare_lookup(args.output)
    elif args.receiver_script:
        result = prepare_receiver(args.output)
    else:
        if args.source is None:
            parser.error('--source required for source/module preparation')
        operation = prepare_export if args.export_module else prepare
        result = (prepare(args.source, args.output, semantic=True) if args.semantic_kernel
                  else operation(args.source, args.output))
    print(json.dumps(result, indent=2))
