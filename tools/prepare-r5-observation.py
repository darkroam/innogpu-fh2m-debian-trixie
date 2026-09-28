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

#define PAYLOAD 128
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
static struct trace_event_file *semantic_files[2];
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

static bool semantic_profile_ok(void)
{
    unsigned int i;
    if (!cpumask_equal(array->tracing_cpumask, cpu_possible_mask))
        return false;
    for (i = 0; i < 2; i++) {
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
    u64 allocated = 0;
    int ret, cpu;
    unsigned int i;
    BUILD_BUG_ON(sizeof(struct wire_header) != 64);
    BUILD_BUG_ON(PAGE_SIZE != 4096);
    if (strcmp(init_utsname()->release, "6.12.101-r5obs2") ||
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
    for (i = 0; i < 2; i++) {
        semantic_files[i] = trace_get_event_file(instance, "power",
                                i ? "r5_dictionary" : "r5_pair");
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
    if (ring_buffer_subbuf_size_get(buffer) != PAGE_SIZE) {
        ret = -E2BIG;
        goto put;
    }
#ifdef CONFIG_TRACER_MAX_TRACE
    if (array->allocated_snapshot) {
        ret = -E2BIG;
        goto put;
    }
#endif
    for_each_possible_cpu(cpu) {
        if (ring_buffer_size(buffer, cpu) != CPU_BUFFER_BYTES) {
            ret = -E2BIG;
            goto put;
        }
#ifdef CONFIG_TRACER_MAX_TRACE
        if (ring_buffer_subbuf_size_get(array->max_buffer.buffer) != PAGE_SIZE ||
            ring_buffer_size(array->max_buffer.buffer, cpu) != 2 * (PAGE_SIZE - 16)) {
            ret = -E2BIG;
            goto put;
        }
#endif
        allocated += CPU_BUFFER_BYTES / (PAGE_SIZE - 16) * PAGE_SIZE;
    }
    if (!allocated || allocated > TOTAL_PAGE_BYTES) {
        ret = -E2BIG;
        goto put;
    }
    ret = r5obs2_census();
    if (ret)
        goto put;
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
    for (i = 0; i < 2; i++)
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
    for (i = 0; i < 2; i++)
        trace_put_event_file(semantic_files[i]);
    trace_array_put(array);
}
module_init(export_init);
module_exit(export_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("R41 bounded nop/event ftrace consumer; no PM or watchdog");
'''
EXPORT_MODULE = EXPORT_MODULE.replace('@CPU_BUFFER_KIB@', str(_analysis.BUFFER_KIB)).replace(
    '@DATA_PAGE_BYTES@', str(_analysis.memory_budget()['components']['data_pages']))

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
            'kernel_release': '6.12.101-r5obs2',
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
       R5_NOTIFIER, R5_PREPARE, R5_TASK };
enum { R5_PREPARE_PHASE, R5_SUSPEND_PHASE, R5_RESUME_PHASE, R5_COMPLETE_PHASE };
#ifdef CONFIG_PM_SLEEP
void r5obs2_topology_changed(void);
void r5obs2_links_lock(void);
void r5obs2_links_unlock(void);
int r5obs2_census(void);
int r5obs2_emit_dictionary(void);
int r5obs2_status(void);
void r5obs2_stop(void);
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
        __array(char, driver, 24)
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

SEMANTIC_EMITTER = r'''// SPDX-License-Identifier: GPL-2.0-only
/* Included only in the generated observation kernel's PM main.c.
 * Static storage, no allocation/extra device lock in a recorded boundary.
 * Any topology change invalidates this census; stale pointers are compared,
 * never dereferenced after census. No PM outcome or return value is changed.
 */
#include <linux/uaccess.h>
#define R5_OBJECTS 512
#define R5_EDGES 1024
struct r5_object {
	struct device *dev;
	atomic64_t generation;
	atomic_t resetting;
	atomic_t prepares;
	u32 parent;
	char name[48];
	char driver[24];
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

/* ponytail: bounded linear lookup (512); use a preallocated hash if measured latency requires it. */
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
		return -E2BIG;
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
	BUILD_BUG_ON(sizeof(r5_objects) + sizeof(r5_edges) > 1024 * 1024);
	BUILD_BUG_ON(sizeof(struct r5obs2_token) > 96);
	if (!trace_r5_pair_enabled() || !trace_r5_dictionary_enabled())
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
			if (id < 0 || r5_e == R5_EDGES) { ret = -E2BIG; goto unlock; }
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
			if (copied <= 0 || copied >= sizeof(o->name) ||
			    strscpy(o->driver, dev_driver_string(o->dev), sizeof(o->driver)) < 0)
				ret = -E2BIG;
			device_unlock(o->dev);
		}
		put_device(o->dev);
	}
	if (ret || atomic_cmpxchg(&r5_state, 1, 2) != 1) {
		atomic_set(&r5_state, 3);
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
	unsigned int id;
	if (!atomic_read(&r5_exported) || atomic_read(&r5_state) != 2)
		return t;
	id = r5_find(dev);
	if (!id || (peer && !r5_find(peer))) {
		r5obs2_topology_changed();
		return t;
	}
	if (op == R5_PREPARE && atomic_inc_return(&r5_objects[id-1].prepares) > 4) {
		r5obs2_topology_changed();
		return t;
	}
	if (!r5_slot())
		return t;
	t.object_id = id;
	t.object_generation = atomic64_read(&r5_lifetime);
	t.completion_generation = atomic64_read(&r5_objects[id-1].generation);
	t.task_id = (u64)task_pid_nr(current) + 1;
	t.task_generation = current->start_boottime + 1;
	t.call_id = atomic64_inc_return(&r5_calls);
	t.peer_id = r5_find(peer);
	t.operation = op;
	t.pm_phase = phase;
	if (atomic_read(&r5_objects[id-1].resetting) && op != R5_REINIT) {
		t.flags = 1;
		r5obs2_topology_changed();
	}
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


SEMANTIC_LOCK = {
    'drivers/base/power/main.c': 'e84341387dced0c3c64377b714c66af98a7338226ddd1cb7e861ba5a20caff42',
    'include/trace/events/power.h': '62b7be15bf5e70af7cfc20071b50b1016a9a33adc2e184c17e46a265794f86e8',
    'drivers/base/core.c': 'efbd4c9ba455cc068678b440d04554f24fa459c391ff935c9071ec4d811c29b5',
    'Makefile': LOCK['Makefile'],
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
    changed = semantic_overlay(files) if semantic else overlay(files)  # Validate all anchors before making any copy.
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

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--export-module', action='store_true')
    modes.add_argument('--receiver-script', action='store_true')
    modes.add_argument('--semantic-kernel', action='store_true')
    args = parser.parse_args()
    if args.receiver_script:
        result = prepare_receiver(args.output)
    else:
        if args.source is None:
            parser.error('--source required for source/module preparation')
        operation = prepare_export if args.export_module else prepare
        result = (prepare(args.source, args.output, semantic=True) if args.semantic_kernel
                  else operation(args.source, args.output))
    print(json.dumps(result, indent=2))
