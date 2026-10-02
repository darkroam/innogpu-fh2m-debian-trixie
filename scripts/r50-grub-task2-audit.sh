#!/usr/bin/env bash
# shellcheck disable=SC2016
# Read-only R50 task 2 audit of the active UEFI/GRUB configuration chain.
set -u -o pipefail
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

SCRIPT=$(readlink -f "$0")
ROOT=$(cd "$(dirname "$SCRIPT")/.." && pwd)
EVIDENCE_ROOT="$ROOT/.build/r50-system-cleanup-20261001-01/task2-grub-audit"
LOGDIR="$EVIDENCE_ROOT/run-$(date +%Y%m%d-%H%M%S)"
APPLY=0

case ${1:-} in
    '') ;;
    --apply) APPLY=1 ;;
    *) printf 'Usage: %s [--apply]\n' "$0" >&2; exit 2 ;;
esac

if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$EVIDENCE_ROOT"
    printf '[%s] uid=%s tty=%s sudo_exec=1\n' "$(date --iso-8601=ns)" \
        "$(id -u)" "$(tty 2>/dev/null || printf unknown)" >>"$EVIDENCE_ROOT/launcher.log"
    exec sudo -- "$SCRIPT" "$@"
fi

mkdir -p "$LOGDIR"
printf '%s\n' "$LOGDIR" >"$EVIDENCE_ROOT/latest-run.txt"
exec > >(tee "$LOGDIR/console.log") 2>&1

run() {
    local name=$1
    shift
    printf '\n=== %s ===\n' "$name"
    "$@" >"$LOGDIR/$name.txt" 2>&1
    local rc=$?
    cat "$LOGDIR/$name.txt"
    printf '%s_rc=%s\n' "$name" "$rc" | tee -a "$LOGDIR/result.txt"
    return 0
}

if ((APPLY)); then
    MODE=cleanup
else
    MODE=read_only
fi
printf 'mode=%s\nstarted=%s\nlogdir=%s\n' \
    "$MODE" "$(date --iso-8601=ns)" "$LOGDIR" >"$LOGDIR/result.txt"

run identity bash -c 'date --iso-8601=ns; id; tty || true; uname -a; cat /proc/cmdline'
run mounts findmnt -rn -o TARGET,SOURCE,FSTYPE,OPTIONS
run block-devices lsblk -o NAME,PATH,SIZE,FSTYPE,FSVER,LABEL,UUID,PARTUUID,MOUNTPOINTS
run efibootmgr efibootmgr -v
run grub-default bash -c 'cat /etc/default/grub; grub-editenv /boot/grub/grubenv list'
run grub-generators bash -c 'ls -l /etc/grub.d; for f in /etc/grub.d/40_custom /etc/grub.d/41_custom; do echo "--- $f"; test -r "$f" && cat "$f" || true; done'
run config-files bash -c 'find / -xdev -type f \( -name grub.cfg -o -name custom.cfg \) -print; find /boot/efi -xdev -type f \( -name grub.cfg -o -name custom.cfg \) -print 2>/dev/null'
run config-metadata bash -c 'while IFS= read -r f; do stat -c "%A %U:%G %s %y %n" "$f"; sha256sum "$f"; done < <(find / -xdev -type f \( -name grub.cfg -o -name custom.cfg \) -print; find /boot/efi -xdev -type f \( -name grub.cfg -o -name custom.cfg \) -print 2>/dev/null)'
run grub-cfg bash -c 'cat /boot/grub/grub.cfg'
run custom-cfg bash -c 'cat /boot/grub/custom.cfg 2>/dev/null || true'
run menu-summary bash -c 'for f in /boot/grub/grub.cfg /boot/grub/custom.cfg; do test -r "$f" || continue; echo "--- $f"; grep -Ec "^[[:space:]]*(menuentry|submenu) " "$f"; grep -En "^[[:space:]]*(menuentry|submenu) |^[[:space:]]*(linux|initrd)[[:space:]]" "$f"; done'
run custom-load-path bash -c 'grep -En "custom\\.cfg|configfile|source" /boot/grub/grub.cfg /etc/grub.d/* 2>/dev/null || true'
run boot-files bash -c 'find /boot -maxdepth 1 -type f -printf "%f %s\n" | sort'
run esp-tree bash -c 'find /boot/efi -xdev -printf "%y %p %s\n" 2>/dev/null | sort'
run esp-hashes bash -c 'find /boot/efi -xdev -type f -print0 2>/dev/null | sort -z | xargs -0r sha256sum'
run grub-probe bash -c 'for target in device fs fs_uuid partmap abstraction; do printf "%s=" "$target"; grub-probe --target="$target" /boot/grub || true; done'

if ((APPLY)); then
    CUSTOM=/boot/grub/custom.cfg
    EXPECTED_SHA=4dc256c2594cfbd4eea8aa0876ca8f32773f7d6a4b5b140bd2ed03562261a8c2
    EXPECTED_RELEASES=(
        6.12.101-r5obs2-r47b
        6.12.101-r5obs2-r47c
        6.12.101-r5obs2-r47d
        6.12.101-r5obs2-r47e
        6.12.101-r5obs2-r47f
        6.12.101-r5obs2-r48
    )
    RETAINED_RELEASES=(
        6.12.107+deb13-amd64
        6.12.101+deb13-amd64
        6.12.101-r5dpm1
        6.12.101-r5dpm2
    )

    die() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
    [[ -f $CUSTOM ]] || die 'custom.cfg missing'
    [[ $(sha256sum "$CUSTOM" | awk '{print $1}') == "$EXPECTED_SHA" ]] || \
        die 'custom.cfg hash changed; refusing cleanup'
    [[ $(grep -Ec '^menuentry ' "$CUSTOM") -eq 6 ]] || \
        die 'custom.cfg no longer contains exactly six entries'
    mapfile -t actual_releases < <(sed -n 's#.*linux[[:space:]]\+/boot/vmlinuz-\([^[:space:]]*\).*#\1#p' "$CUSTOM" | sort -u)
    [[ ${actual_releases[*]} == "${EXPECTED_RELEASES[*]}" ]] || \
        die 'custom.cfg release set changed; refusing cleanup'

    for release in "${EXPECTED_RELEASES[@]}"; do
        [[ ! -e /boot/vmlinuz-$release && ! -e /boot/initrd.img-$release ]] || \
            die "stale target still has boot files: $release"
    done
    for release in "${RETAINED_RELEASES[@]}"; do
        for artifact in vmlinuz initrd.img System.map config; do
            [[ -f /boot/$artifact-$release ]] || die "retained boot file missing: $artifact-$release"
        done
    done

    sha256sum /etc/default/grub /boot/grub/grub.cfg /boot/grub/grubenv "$CUSTOM" \
        >"$LOGDIR/cleanup-before.sha256"
    DEFAULT_BEFORE=$(sha256sum /etc/default/grub | awk '{print $1}')
    GRUBENV_BEFORE=$(sha256sum /boot/grub/grubenv | awk '{print $1}')
    find /boot/efi -xdev -type f -print0 | sort -z | xargs -0r sha256sum \
        >"$LOGDIR/esp-before.sha256"
    cp -a "$CUSTOM" "$LOGDIR/custom.cfg.before"
    [[ $(sha256sum "$LOGDIR/custom.cfg.before" | awk '{print $1}') == "$EXPECTED_SHA" ]] || \
        die 'custom.cfg evidence backup hash mismatch'

    MUTATED=0
    rollback_on_exit() {
        local rc=$?
        trap - EXIT
        if ((rc != 0 && MUTATED)); then
            cp -a "$LOGDIR/custom.cfg.before" "$CUSTOM"
            printf 'CLEANUP_FAIL_ROLLED_BACK rc=%s\n' "$rc" >&2
        fi
        exit "$rc"
    }
    trap rollback_on_exit EXIT

    MUTATED=1
    rm -- "$CUSTOM"
    update-grub >"$LOGDIR/update-grub.log" 2>&1 || die 'update-grub failed'
    cat "$LOGDIR/update-grub.log"

    [[ ! -e $CUSTOM ]] || die 'custom.cfg still exists'
    ! grep -Eq 'r5obs2-r47[b-f]|r5obs2-r48' /boot/grub/grub.cfg || \
        die 'stale r5obs2 entry remains in grub.cfg'
    for release in "${RETAINED_RELEASES[@]}"; do
        grep -Fq "/boot/vmlinuz-$release" /boot/grub/grub.cfg || \
            die "retained GRUB entry missing: $release"
    done
    [[ $(awk -F= '$1 == "GRUB_DEFAULT" {print $2}' /etc/default/grub) == 0 ]] || \
        die 'GRUB_DEFAULT changed'
    [[ $(sha256sum /etc/default/grub | awk '{print $1}') == "$DEFAULT_BEFORE" ]] || \
        die '/etc/default/grub changed unexpectedly'
    [[ $(sha256sum /boot/grub/grubenv | awk '{print $1}') == "$GRUBENV_BEFORE" ]] || \
        die 'grubenv changed unexpectedly'
    find /boot/efi -xdev -type f -print0 | sort -z | xargs -0r sha256sum \
        >"$LOGDIR/esp-after.sha256"
    cmp -s "$LOGDIR/esp-before.sha256" "$LOGDIR/esp-after.sha256" || \
        die 'ESP content changed unexpectedly'
    efibootmgr -v >"$LOGDIR/efibootmgr-after.txt"
    cmp -s "$LOGDIR/efibootmgr.txt" "$LOGDIR/efibootmgr-after.txt" || \
        die 'EFI NVRAM boot configuration changed unexpectedly'

    sha256sum /etc/default/grub /boot/grub/grub.cfg /boot/grub/grubenv \
        >"$LOGDIR/cleanup-after.sha256"
    grep -En "^[[:space:]]*(menuentry|submenu) |^[[:space:]]*(linux|initrd)[[:space:]]" \
        /boot/grub/grub.cfg >"$LOGDIR/menu-after.txt"
    trap - EXIT
    printf 'cleanup=PASS\nremoved=%s\nbackup=%s\n' \
        "$CUSTOM" "$LOGDIR/custom.cfg.before" | tee -a "$LOGDIR/result.txt"
    printf 'R50_GRUB_CLEANUP_PASS no_reboot=1 evidence=%s\n' "$LOGDIR"
else
    printf 'audit_rc=0\n' | tee -a "$LOGDIR/result.txt"
    printf 'R50_GRUB_AUDIT_PASS no_host_mutation=1 evidence=%s\n' "$LOGDIR"
fi

printf 'finished=%s\n' "$(date --iso-8601=ns)" | tee -a "$LOGDIR/result.txt"
