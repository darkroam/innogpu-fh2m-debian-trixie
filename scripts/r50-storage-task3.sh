#!/usr/bin/env bash
# R50 task 3: inventory, physical-TTY approval, then exact-target cleanup.
set -euo pipefail
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

SCRIPT=$(readlink -f "$0")
ROOT=$(cd "$(dirname "$SCRIPT")/.." && pwd)
EVIDENCE_ROOT="$ROOT/.build/r50-system-cleanup-20261001-01/task3-storage"

usage() {
    printf 'Usage: %s --inventory | --self-test | --confirm | --apply RECEIPT\n' "$0" >&2
    exit 2
}

emit_candidates() {
    cat <<'EOF'
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/A/build	A build object tree; final packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/A/unpack	extracted package tree; A packages retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/B/build	B build object tree; final packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/B/unpack	extracted package tree; B packages retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/A-failed-rust-toolchain-drift/build	failed intermediate object tree; failure logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/A-failed-rust-toolchain-drift/failed-signing-artifacts	failed duplicate package staging; root packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/failed-output-path-drift/A/build	failed A object tree; failure packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/failed-output-path-drift/A/unfixed-changelog-packages	discarded duplicate packages; root packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/failed-output-path-drift/B/build	failed B object tree; failure packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/failed-output-path-drift/B/unfixed-changelog-packages	discarded duplicate packages; root packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/source	extracted source; input source deb retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/A/build	A build object tree; final packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/A/unpack	extracted package tree; A packages retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/B/build	B build object tree; final packages/logs retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/B/unpack	extracted package tree; B packages retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/B-interrupted-operator-reboot/build	interrupted object tree; interruption log retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/packaging-discarded	explicitly discarded duplicate packaging attempt
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/verification-attempt-1/A-unpack	verification package extraction; hashes/results retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/verification-attempt-1/B-unpack	verification package extraction; hashes/results retained
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/source	extracted source; input source deb retained
.runtime-archive	.runtime-archive/tools/ghidra_12.1.3_PUBLIC	extracted tool; original zip/config/scripts retained
.runtime-archive	.runtime-archive/tools/jdk-overlay	extracted duplicate JDK overlay; original JDK deb retained
.runtime-archive	.runtime-archive/tools/cache	download cache; original archives retained
.build	.build/r48-direct-20260930-01/kernel	R48 kernel object tree; evidence/logs retained
.build	.build/r48-direct-20260930-01/source	R48 extracted kernel source; source deb retained elsewhere
.build	.build/r48-direct-20260930-01/artifacts	R48 copied build artifacts; hashes/evidence retained
.build	.build/r48-direct-20260930-01/driver	R48 copied driver build tree; patch/meta/evidence retained
.build	.build/r48-direct-20260930-01/stage	R48 module staging tree
.build	.build/r48-direct-20260930-01/modules-final	R48 copied final modules; installed/final evidence retained
.build	.build/r48-direct-20260930-01/install-broad-probes-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-private-accessor-fix-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-async-owner-probes-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-quiet-module-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-dvfs-guard-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-dvfs-guard-02	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-r48-coherent-01	closed diagnostic install payload
.build	.build/r48-direct-20260930-01/install-07	closed diagnostic install payload
.build	.build/r36-observation-preparation-20260924-01	superseded preparation builds; R36 trace-transport original retained
.build	.build/r34-observe-20260924-01/kernel-build	R34 object tree; source anchor retained
.build	.build/r34-observe-20260924-01/kernel-build-02	R34 object tree; source anchor retained
.build	.build/r41-stage1-20260928-01	closed staging root
.build	.build/r42-wired-assessment-20260928-01	closed staging root
.build	.build/r43-prerequisites-20260928-01	closed staging root
.build	.build/r44-semantic-budget-20260928-01	closed staging root
.build	.build/r45-emitter-20260928-01	closed build root
.build	.build/r46-coverage-20260928-01	closed staging root
.build	.build/r47-admission-20260930-01	closed staging root
.build	.build/r47-cache-cleanup-20260930-01	closed staging root
.build	.build/r47-capacity-20260930-01	closed staging root
.build	.build/r47-meter-20260928-01	closed R47 attempt root
.build	.build/r47-meter-20260928-02	closed R47 attempt root
.build	.build/r47-meter-20260928-03	closed R47 attempt root
.build	.build/r47-meter-20260930-04	closed R47 attempt root
.build	.build/r47-meter-20260930-05	closed R47 attempt root
.build	.build/r47-meter-20260930-06	closed R47 attempt root
.build	.build/r47-reader-20260930-01	closed staging root
.build	.build/r51-module-deploy-20261002-01/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-01/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-02/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-02/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-03/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-03/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-04/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-04/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-05/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-05/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-06/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-06/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-07/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-07/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-08/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-08/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-09/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-09/kernel-build	R51 module build objects
.build	.build/r51-module-deploy-20261002-10/source	R51 extracted source; deb retained
.build	.build/r51-module-deploy-20261002-10/kernel-build	R51 module build objects
build	build/r15-deb-inspect	extracted package inspection tree; source deb retained
build	build/repro1	duplicate R4 reproducibility output; top-level package/hash record retained
build	build/repro2	duplicate R4 reproducibility output; top-level package/hash record retained
build	build/r11-repro2	duplicate R11 reproducibility output; byte-identical r11-repro1 physical package retained
build	build/r12-b	duplicate R12 reproducibility output; byte-identical r12-a physical package retained
EOF
}

emit_retain() {
    cat <<'EOF'
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/{input,patches,control,A,B}	101 source deb; patch/control; A/B final packages and logs
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/{input,patches,control,A,B}	101 source deb; patch/control; A/B final packages and logs
.runtime-archive	.runtime-archive/runtime-5.0.0-i6/*result* and runtime evidence roots	unique frozen runtime evidence and manifests
.runtime-archive	.runtime-archive/tools/{*.zip,*.deb,jdk-21,config,ghidra_scripts,ghidra-projects,data}	tool source payload, unique complete JDK runtime and user-created state
.build	.build/r36-trace-transport-20260924-01	unique R36 observation chain original
.build	.build/r34-observe-20260924-01/source	R34 kernel source anchor
.build	.build/r26-*; r28-r31; r35; r37; r39; R48 evidence/logs; R49/R50	retained milestone and installation evidence
.build	.build/r51-module-deploy-*/{logs,hashes,modules,results}; -10/runtime	R51 failure history, installed module identity and runtime acceptance
build	build/innogpu-fh2m-trixie_4.0.2-i3.deb	mandatory physical rollback package
build	build/r16-{unpack-D,fantgpu-deb,license-precheck,evidence}; p2-manifest.tsv	active R16 tool inputs/evidence; task 4 will define migration
build	build/r11-repro1; build/r12-a	one physical package from each reproducibility pair
build	build evidence/stage roots and unique top-level packages	frozen historical evidence or unique physical payload
EOF
}

safe_relpath() {
    local p=$1
    [[ $p != /* && $p != *'..'* && $p != '.' && $p != '' ]] || return 1
    case $p in
        .runtime-archive/*|.build/*|build/*) ;;
        *) return 1 ;;
    esac
}

disk_bytes() {
    local path=$1 value
    if value=$(du -B1 -s -- "$path" 2>/dev/null); then
        printf '%s\n' "${value%%[[:space:]]*}"
    else
        sudo du -B1 -s -- "$path" | awk '{print $1}'
    fi
}

metadata_fingerprint() {
    local path=$1 value
    if value=$(find -P "$path" -printf '%y\t%s\t%T@\t%P\t%l\0' 2>/dev/null | \
        LC_ALL=C sort -z | sha256sum); then
        printf '%s\n' "${value%%[[:space:]]*}"
    else
        sudo find -P "$path" -printf '%y\t%s\t%T@\t%P\t%l\0' | \
            LC_ALL=C sort -z | sha256sum | awk '{print $1}'
    fi
}

approval_key() {
    case $1 in
        .runtime-archive) printf 'runtime_archive\n' ;;
        .build) printf 'dot_build\n' ;;
        build) printf 'build\n' ;;
        *) return 1 ;;
    esac
}

write_pointer() {
    local path=$1 value=$2 tmp
    tmp=$(mktemp "${path}.tmp.XXXXXX")
    if ! printf '%s\n' "$value" >"$tmp"; then
        rm -f -- "$tmp"
        return 1
    fi
    chmod 0644 "$tmp"
    mv -f -- "$tmp" "$path"
}

rebuild_source_preflight() {
    local ghidra_zip="$ROOT/.runtime-archive/tools/ghidra_12.1.3_PUBLIC_20260817.zip"
    local jdk_deb="$ROOT/.runtime-archive/tools/openjdk-21-jdk-headless_21.0.12.1+1-1~deb13u1_amd64.deb"
    cmp -s "$ROOT/build/innogpu-fh2m-trixie_4.0.0-i1.deb" \
        "$ROOT/build/repro1/innogpu-fh2m-trixie_4.0.0-i1.deb"
    cmp -s "$ROOT/build/innogpu-fh2m-trixie_4.0.0-i1.deb" \
        "$ROOT/build/repro2/innogpu-fh2m-trixie_4.0.0-i1.deb"
    cmp -s "$ROOT/build/r11-repro1/innogpu-fh2m-trixie_4.0.2-i1.deb" \
        "$ROOT/build/r11-repro2/innogpu-fh2m-trixie_4.0.2-i1.deb"
    cmp -s "$ROOT/build/r12-a/innogpu-fh2m-trixie_4.0.2-i2.deb" \
        "$ROOT/build/r12-b/innogpu-fh2m-trixie_4.0.2-i2.deb"
    unzip -tq "$ghidra_zip" >/dev/null
    [[ $(comm -13 \
        <(unzip -Z1 "$ghidra_zip" | sed 's#^[^/]*/##' | sed '/^$/d; /\/$/d' | LC_ALL=C sort) \
        <(find "$ROOT/.runtime-archive/tools/ghidra_12.1.3_PUBLIC" -type f -printf '%P\n' | LC_ALL=C sort) | \
        wc -l) -eq 0 ]]
    dpkg-deb --ctrl-tarfile "$jdk_deb" | tar -xOf - ./md5sums | \
        (cd "$ROOT/.runtime-archive/tools/jdk-overlay" && md5sum -c --quiet -)
    dpkg-deb --info "$ROOT/debs/linux-source-6.12_6.12.107-1_all.deb" >/dev/null
    dpkg-deb --info "$ROOT/.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/input/linux-source-6.12_6.12.101-1_all.deb" >/dev/null
    dpkg-deb --info "$ROOT/.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/input/linux-source-6.12_6.12.101-1_all.deb" >/dev/null
}

self_test() {
    local -a paths keys
    local area rel reason p q present=0 test_dir run state rebuild
    mapfile -t paths < <(emit_candidates | cut -f2)
    [[ ${#paths[@]} -eq 81 ]]
    [[ $(printf '%s\n' "${paths[@]}" | LC_ALL=C sort -u | wc -l) -eq 81 ]]
    keys=("$(approval_key .runtime-archive)" "$(approval_key .build)" "$(approval_key build)")
    [[ $(printf '%s\n' "${keys[@]}" | LC_ALL=C sort -u | wc -l) -eq 3 ]]
    while IFS=$'\t' read -r area rel reason; do
        [[ -n $reason ]]
        safe_relpath "$rel"
        case $area:$rel in
            .runtime-archive:.runtime-archive/*|.build:.build/*|build:build/*) ;;
            *) return 1 ;;
        esac
        [[ ! -e $ROOT/$rel ]] || ((present += 1))
    done < <(emit_candidates)
    for p in "${paths[@]}"; do
        for q in "${paths[@]}"; do
            [[ $p == "$q" ]] && continue
            [[ $q != "$p/"* ]] || { printf 'FAIL overlapping targets: %s -> %s\n' "$p" "$q" >&2; return 1; }
        done
    done
    test_dir=$(mktemp -d)
    printf 'old\n' >"$test_dir/pointer"
    chmod 0444 "$test_dir/pointer"
    write_pointer "$test_dir/pointer" new
    [[ $(<"$test_dir/pointer") == new ]]
    rm -rf -- "$test_dir"
    if [[ $present -eq 80 ]]; then
        rebuild_source_preflight
        state=pre-clean
        rebuild=PASS
    elif [[ $present -eq 0 ]]; then
        run=$(find "$EVIDENCE_ROOT" -mindepth 1 -maxdepth 1 -type d -name 'cleanup-*' -print | LC_ALL=C sort | tail -n1)
        [[ -n $run && $(<"$run/result.txt") == "R50_TASK3_CLEANUP_PASS evidence=$run" ]]
        [[ $(wc -l <"$run/removal.tsv") -eq 80 ]]
        if grep -qv $'^removed\t' "$run/removal.tsv"; then
            printf 'FAIL: cleanup evidence contains a non-removed row\n' >&2
            return 1
        fi
        cmp -s "$run/retain-before.tsv" "$run/retain-after.tsv"
        (cd "$run" && sha256sum -c manifests.sha256 >/dev/null)
        state=post-clean
        rebuild=RECORDED
    else
        printf 'FAIL: expected 80 pre-clean or 0 post-clean candidates; found %s\n' "$present" >&2
        return 1
    fi
    printf 'R50_TASK3_SELF_TEST_PASS state=%s present_candidates=%s definitions=81 approval_keys=3 overlaps=0 pointer_replace=PASS rebuild_sources=%s\n' \
        "$state" "$present" "$rebuild"
}

make_inventory() {
    local out=$1 area rel reason bytes metadata current candidates remain
    mkdir -p "$out"
    printf 'area\tbytes\tmetadata_sha256\tpath\tclassification\treason\n' >"$out/task3-clean-candidates.tsv"
    while IFS=$'\t' read -r area rel reason; do
        safe_relpath "$rel" || { printf 'FAIL unsafe target: %s\n' "$rel" >&2; exit 1; }
        [[ -e $ROOT/$rel && ! -L $ROOT/$rel ]] || continue
        bytes=$(disk_bytes "$ROOT/$rel")
        metadata=$(metadata_fingerprint "$ROOT/$rel")
        printf '%s\t%s\t%s\t%s\tsafe-to-clean\t%s\n' "$area" "$bytes" "$metadata" "$rel" "$reason" \
            >>"$out/task3-clean-candidates.tsv"
    done < <(emit_candidates)

    printf 'area\tpath_or_set\tretention_basis\n' >"$out/task3-retain.tsv"
    emit_retain >>"$out/task3-retain.tsv"
    printf 'area\tcurrent_bytes\tcandidate_bytes\testimated_remaining_bytes\n' >"$out/task3-summary.tsv"
    for area in .runtime-archive .build build; do
        current=$(disk_bytes "$ROOT/$area")
        candidates=$(awk -F '\t' -v a="$area" 'NR > 1 && $1 == a {s += $2} END {print s + 0}' \
            "$out/task3-clean-candidates.tsv")
        remain=$((current - candidates))
        printf '%s\t%s\t%s\t%s\n' "$area" "$current" "$candidates" "$remain" \
            >>"$out/task3-summary.tsv"
    done
    sha256sum "$out/task3-clean-candidates.tsv" "$out/task3-retain.tsv" "$out/task3-summary.tsv" \
        >"$out/inventory.sha256"
}

show_inventory() {
    local out=$1
    printf '\nR50 task 3 storage inventory (no deletion)\n'
    printf '%-20s %14s %14s %14s\n' AREA CURRENT CLEANABLE REMAINING
    awk -F '\t' 'NR > 1 {printf "%-20s %14.2fG %14.2fG %14.2fG\n", $1,$2/1e9,$3/1e9,$4/1e9}' \
        "$out/task3-summary.tsv"
    printf '\nClean candidates:\n'
    awk -F '\t' 'NR > 1 {printf "[%s] %8.2fG  %s\n    %s\n",$1,$2/1e9,$4,$6}' \
        "$out/task3-clean-candidates.tsv"
    printf '\nRetained sets and reasons:\n'
    awk -F '\t' 'NR > 1 {printf "[%s] %s\n    %s\n",$1,$2,$3}' "$out/task3-retain.tsv"
    printf '\nInventory: %s\nNo files have been deleted.\n' "$out"
}

inventory_mode() {
    local out
    out="$EVIDENCE_ROOT/inventory-$(date +%Y%m%d-%H%M%S)"
    make_inventory "$out"
    write_pointer "$EVIDENCE_ROOT/latest-inventory.txt" "$out"
    show_inventory "$out" | tee "$out/console.txt"
}

confirm_mode() {
    local tty_path out answer receipt area sha
    [[ $(id -u) -ne 0 ]] || { printf 'FAIL: start this script as the normal user, not root\n' >&2; exit 1; }
    tty_path=$(tty)
    [[ $tty_path =~ ^/dev/tty[0-9]+$ ]] || {
        printf 'FAIL: physical TTY required for approval; current=%s\n' "$tty_path" >&2
        exit 1
    }
    out="$EVIDENCE_ROOT/inventory-$(date +%Y%m%d-%H%M%S)"
    make_inventory "$out"
    write_pointer "$EVIDENCE_ROOT/latest-inventory.txt" "$out"
    show_inventory "$out" | tee "$out/console.txt"
    receipt="$out/confirmation.tsv"
    sha=$(sha256sum "$out/task3-clean-candidates.tsv" | awk '{print $1}')
    printf 'key\tvalue\ncreated\t%s\ntty\t%s\nuid\t%s\ninventory\t%s\ncandidates_sha256\t%s\n' \
        "$(date --iso-8601=ns)" "$tty_path" "$(id -u)" "$out" "$sha" >"$receipt"
    for area in .runtime-archive .build build; do
        printf '\nType exactly "DELETE %s" to approve this area, or press Enter to retain it: ' "$area"
        IFS= read -r answer
        if [[ $answer == "DELETE $area" ]]; then
            printf 'approve_%s\tyes\n' "$(approval_key "$area")" >>"$receipt"
        else
            printf 'approve_%s\tno\n' "$(approval_key "$area")" >>"$receipt"
        fi
    done
    sha256sum "$receipt" >"$receipt.sha256"
    write_pointer "$EVIDENCE_ROOT/latest-confirmation.txt" "$receipt"
    printf '\nCONFIRMATION_RECORDED receipt=%s no_deletion=1\n' "$receipt"
}

retain_snapshot() {
    local out=$1 p
    : >"$out"
    for p in \
        build/innogpu-fh2m-trixie_4.0.2-i3.deb \
        build/r11-repro1/innogpu-fh2m-trixie_4.0.2-i1.deb \
        build/r12-a/innogpu-fh2m-trixie_4.0.2-i2.deb \
        debs/linux-source-6.12_6.12.107-1_all.deb \
        .runtime-archive/tools/ghidra_12.1.3_PUBLIC_20260817.zip \
        .runtime-archive/tools/openjdk-21-jdk-headless_21.0.12.1+1-1~deb13u1_amd64.deb \
        .runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/input/linux-source-6.12_6.12.101-1_all.deb \
        .runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/input/linux-source-6.12_6.12.101-1_all.deb \
        .runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/patches/0001-dpm-watchdog-arm-device-prepare-complete-noirq.patch \
        .build/r51-module-deploy-20261002-10/battery.ko \
        .build/r51-module-deploy-20261002-10/xhci-pci.ko \
        .build/r51-module-deploy-20261002-10/result.txt; do
        [[ -f $ROOT/$p ]] || { printf 'FAIL retained file missing: %s\n' "$p" >&2; exit 1; }
        sha256sum "$ROOT/$p" >>"$out"
    done
    for p in \
        .build/r36-trace-transport-20260924-01 \
        .build/r34-observe-20260924-01/source \
        .build/r48-direct-20260930-01/evidence \
        .build/r49-final-acceptance-20261001-01 \
        .build/r50-system-cleanup-20261001-01/task2-grub-audit \
        .runtime-archive/tools/jdk-21; do
        [[ -d $ROOT/$p ]] || { printf 'FAIL retained directory missing: %s\n' "$p" >&2; exit 1; }
        printf 'DIR\t%s\t%s\n' "$(disk_bytes "$ROOT/$p")" "$ROOT/$p" >>"$out"
    done
}

apply_mode() {
    local receipt=$1 inventory expected actual run area rel approved key approved_bytes approved_metadata current_bytes current_metadata
    if [[ $(id -u) -ne 0 ]]; then
        exec sudo -- "$SCRIPT" --apply "$receipt"
    fi
    [[ -f $receipt && -f $receipt.sha256 ]] || { printf 'FAIL: receipt missing\n' >&2; exit 1; }
    sha256sum -c "$receipt.sha256"
    inventory=$(awk -F '\t' '$1 == "inventory" {print $2}' "$receipt")
    expected=$(awk -F '\t' '$1 == "candidates_sha256" {print $2}' "$receipt")
    [[ -d $inventory && -f $inventory/task3-clean-candidates.tsv ]] || {
        printf 'FAIL: bound inventory missing\n' >&2; exit 1;
    }
    actual=$(sha256sum "$inventory/task3-clean-candidates.tsv" | awk '{print $1}')
    [[ $actual == "$expected" ]] || { printf 'FAIL: candidate inventory changed\n' >&2; exit 1; }
    run="$EVIDENCE_ROOT/cleanup-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$run"
    cp "$receipt" "$receipt.sha256" "$inventory/task3-clean-candidates.tsv" \
        "$inventory/task3-retain.tsv" "$inventory/task3-summary.tsv" "$run/"
    rebuild_source_preflight
    printf 'rebuild_source_preflight=PASS\n' >"$run/preflight.txt"
    retain_snapshot "$run/retain-before.tsv"
    printf 'area\tbytes\tmetadata_sha256\tpath\n' >"$run/selected-targets.tsv"
    while IFS=$'\t' read -r area approved_bytes approved_metadata rel _; do
        [[ $area == area ]] && continue
        approved=$(awk -F '\t' -v k="approve_$(approval_key "$area")" '$1 == k {print $2}' "$receipt")
        [[ $approved == yes ]] || continue
        safe_relpath "$rel" || { printf 'FAIL unsafe target: %s\n' "$rel" >&2; exit 1; }
        [[ -e $ROOT/$rel && ! -L $ROOT/$rel ]] || { printf 'FAIL target drift: %s\n' "$rel" >&2; exit 1; }
        current_bytes=$(disk_bytes "$ROOT/$rel")
        current_metadata=$(metadata_fingerprint "$ROOT/$rel")
        [[ $current_bytes == "$approved_bytes" && $current_metadata == "$approved_metadata" ]] || {
            printf 'FAIL target changed after confirmation: %s\n' "$rel" >&2
            exit 1
        }
        printf '%s\t%s\t%s\t%s\n' "$area" "$current_bytes" "$current_metadata" "$rel" \
            >>"$run/selected-targets.tsv"
    done <"$inventory/task3-clean-candidates.tsv"
    [[ $(wc -l <"$run/selected-targets.tsv") -gt 1 ]] || { printf 'FAIL: no area approved\n' >&2; exit 1; }

    printf 'Generating content SHA manifests before deletion; this is intentionally I/O-heavy.\n'
    for area in .runtime-archive .build build; do
        approved=$(awk -F '\t' -v k="approve_$(approval_key "$area")" '$1 == k {print $2}' "$receipt")
        [[ $approved == yes ]] || continue
        key=$(approval_key "$area")
        : >"$run/$key-files.sha256"
        while IFS=$'\t' read -r _ _ _ rel; do
            find -P "$ROOT/$rel" -type f -print0 | LC_ALL=C sort -z | \
                xargs -0r sha256sum >>"$run/$key-files.sha256"
        done < <(awk -F '\t' -v a="$area" 'NR > 1 && $1 == a {print $1 FS $2 FS $3 FS $4}' \
            "$run/selected-targets.tsv")
        sha256sum "$run/$key-files.sha256" >>"$run/manifests.sha256"
    done
    sha256sum "$run/selected-targets.tsv" >>"$run/manifests.sha256"

    while IFS=$'\t' read -r area _ _ rel; do
        [[ $area == area ]] && continue
        rm -rf -- "$ROOT/${rel:?}"
        [[ ! -e $ROOT/$rel ]] || { printf 'FAIL target remains: %s\n' "$rel" >&2; exit 1; }
        printf 'removed\t%s\n' "$rel" >>"$run/removal.tsv"
    done <"$run/selected-targets.tsv"
    retain_snapshot "$run/retain-after.tsv"
    cmp -s "$run/retain-before.tsv" "$run/retain-after.tsv" || {
        printf 'FAIL: retained set changed\n' >&2
        diff -u "$run/retain-before.tsv" "$run/retain-after.tsv" || true
        exit 1
    }
    du -B1 -s "$ROOT/.runtime-archive" "$ROOT/.build" "$ROOT/build" >"$run/size-after.tsv"
    printf 'R50_TASK3_CLEANUP_PASS evidence=%s\n' "$run" | tee "$run/result.txt"
}

case ${1:-} in
    --inventory) inventory_mode ;;
    --self-test) self_test ;;
    --confirm) confirm_mode ;;
    --apply) [[ $# -eq 2 ]] || usage; apply_mode "$(readlink -f "$2")" ;;
    *) usage ;;
esac
