#!/bin/bash
# Unit tests: DRI repair service lifecycle (scripts/install-dri-node-repair-service.sh
# and scripts/uninstall-innogpu.sh) with a STATEFUL fake systemctl - no root,
# no systemd, no /dev access, no real system modification. The installer is run
# with INNOGPU_DRI_TEST_ROOT=<tmp> and the uninstaller with INNOGPU_UNINSTALL_TEST=1;
# both hooks are hard-off in production and fail closed on an unsafe test root.
#
# The fake systemctl keeps an enable-state file per test root, so the tests
# verify enable-state preservation (enabled stays enabled, disabled stays
# disabled, a partially-failed enable leaves the unit disabled again) instead
# of merely grepping for a disable command. TMPDIR is isolated so backup
# residue can be asserted. Counter-examples: PATH injection; foreign files;
# failed enable/start/daemon-reload/unit-write/backup each restore or clean up;
# backup failure leaves the original unit byte-exact; /usr/bin convenience
# link failure only warns; expected-version mismatch has zero side effects;
# package-absent only touches DRI paths; unsafe test roots; hard-coded usernames.

set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALLER="$ROOT/scripts/install-dri-node-repair-service.sh"
UNINSTALL="$ROOT/scripts/uninstall-innogpu.sh"
CHECK_X="$ROOT/scripts/check-soft-xorg-dwm.sh"
HOTLOAD="$ROOT/scripts/try-hotload-patched17.sh"
T="$(mktemp -d "${TMPDIR:-/tmp}/inno-dri-repair-tests.XXXXXX")"
trap 'rm -rf "$T"' EXIT
export TMPDIR="$T/tmp"
mkdir -p "$T/tmp" "$T/bin"

cat > "$T/bin/systemctl" <<'EOF'
#!/bin/bash
echo "$*" >> "${FAKE_SYSTEMCTL_LOG:-/dev/null}"
ST="${FAKE_SYSTEMCTL_STATE:-/nonexistent}"
case "$1" in
    is-enabled)
        if [[ -f "$ST/enabled" ]]; then echo enabled; exit 0; fi
        echo disabled; exit 1 ;;
    enable)
        mkdir -p "$ST" 2>/dev/null || true
        : > "$ST/enabled"
        # simulate real systemd: enable also creates a wants link; a partially
        # failed enable leaves it behind until disable removes it
        mkdir -p "$ST/wants" 2>/dev/null || true
        : > "$ST/wants/innogpu-repair-dri-nodes.service"
        [[ "${FAKE_SYSTEMCTL_ENABLE_FAIL:-0}" == 1 ]] && { echo "fake: systemctl enable failed" >&2; exit 1; } ;;
    disable)
        rm -f "$ST/enabled" 2>/dev/null || true
        rm -f "$ST/wants/innogpu-repair-dri-nodes.service" 2>/dev/null || true ;;
    daemon-reload)
        [[ "${FAKE_SYSTEMCTL_RELOAD_FAIL:-0}" == 1 ]] && { echo "fake: daemon-reload failed" >&2; exit 1; } ;;
    start)
        [[ "${FAKE_SYSTEMCTL_START_FAIL:-0}" == 1 ]] && { echo "fake: systemctl start failed" >&2; exit 1; } ;;
    *) ;;
esac
exit 0
EOF
chmod +x "$T/bin/systemctl"

# fake cp: fails -p backup copies when FAKE_CP_FAIL=1, or the RESTORE copy
# (target ends with .service) when FAKE_CP_FAIL_RESTORE=1; else passthrough
cat > "$T/bin/cp" <<'EOF'
#!/bin/bash
if [[ "${FAKE_CP_FAIL:-0}" == 1 && "$1" == "-p" ]]; then
    echo "fake: cp failed" >&2; exit 1
fi
if [[ "${FAKE_CP_FAIL_RESTORE:-0}" == 1 && "$1" == "-p" ]]; then
    case "${@: -1}" in
        *.service) echo "fake: restore cp failed" >&2; exit 1 ;;
    esac
fi
exec /usr/bin/cp "$@"
EOF
chmod +x "$T/bin/cp"

# PATH includes a fake "innogpu-repair-dri-nodes" on a writable dir: the
# installer must NEVER trust an arbitrary PATH hit.
mkdir -p "$T/evil"
printf '%s\n' '#!/bin/sh' 'echo NOT-THE-PROJECT-HELPER' > "$T/evil/innogpu-repair-dri-nodes"
chmod +x "$T/evil/innogpu-repair-dri-nodes"
export PATH="$T/evil:$T/bin:$PATH"

passed=0; failed=0; t=0
pass() { t=$((t+1)); passed=$((passed+1)); printf 'dri_repair_t%02d=PASS # %s\n' "$t" "$1"; }
fail() { t=$((t+1)); failed=$((failed+1)); printf 'dri_repair_t%02d=FAIL reason=%s\n' "$t" "$2"; }

install_run() {
    local root="$1"; shift
    env INNOGPU_DRI_TEST_ROOT="$root" FAKE_SYSTEMCTL_LOG="$T/sysctl.log" \
        FAKE_SYSTEMCTL_STATE="$root/state" "$@" bash "$INSTALLER" > "$T/out" 2>&1
}
uninstall_run() {
    local root="$1"; shift
    env INNOGPU_UNINSTALL_TEST=1 INNOGPU_UNINSTALL_TEST_ROOT="$root" \
        INNOGPU_UNINSTALL_TEST_VERSION=4.0.0-i1 FAKE_SYSTEMCTL_LOG="$T/sysctl-un.log" \
        FAKE_SYSTEMCTL_STATE="$root/state" "$@" bash "$UNINSTALL" > "$T/un.out" 2>&1
}
helper_of() { printf '%s/usr/local/sbin/innogpu-repair-dri-nodes' "$1"; }
unit_of()  { printf '%s/etc/systemd/system/innogpu-repair-dri-nodes.service' "$1"; }
enabled_of() { if [[ -f "$1/state/enabled" ]]; then echo enabled; else echo disabled; fi; }
hash_of() { sha256sum "$1" | cut -d' ' -f1; }
mode_of() { stat -c %a "$1"; }
backup_residue() { find "$T/tmp" -name 'innogpu-unit.*' 2>/dev/null; }

# ---- A: fresh install + partially-failed enable (wants link left) ----
A="$T/a"
install_run "$A" env FAKE_SYSTEMCTL_ENABLE_FAIL=1
rc=$?
if [ "$rc" -ne 0 ] && [ ! -e "$(helper_of "$A")" ] && [ ! -e "$(unit_of "$A")" ] \
   && [ "$(enabled_of "$A")" = disabled ] \
   && [ ! -e "$A/state/wants/innogpu-repair-dri-nodes.service" ]; then
    pass fresh_enable_failure_rolls_back_created
else
    fail fresh_enable_failure_rolls_back_created "rc=$rc enabled=$(enabled_of "$A")"
fi

# ---- B: existing ENABLED install survives failed reinstall, unchanged ----
B="$T/b"
install_run "$B"
rc=$?
if [ "$rc" -ne 0 ]; then fail setup_install_b "rc=$rc"; else
    ub="$(hash_of "$(unit_of "$B")")"; mb="$(mode_of "$(unit_of "$B")")"
    install_run "$B" env FAKE_SYSTEMCTL_START_FAIL=1
    rc=$?
    if [ "$rc" -ne 0 ] && [ -e "$(helper_of "$B")" ] && [ -e "$(unit_of "$B")" ] \
       && [ "$(hash_of "$(unit_of "$B")")" = "$ub" ] && [ "$(mode_of "$(unit_of "$B")")" = "$mb" ] \
       && [ "$(enabled_of "$B")" = enabled ]; then
        pass existing_install_survives_start_failure_unchanged
    else
        fail existing_install_survives_start_failure_unchanged "rc=$rc enabled=$(enabled_of "$B")"
    fi
    install_run "$B" env FAKE_SYSTEMCTL_ENABLE_FAIL=1
    rc=$?
    if [ "$rc" -ne 0 ] && [ -e "$(unit_of "$B")" ] \
       && [ "$(hash_of "$(unit_of "$B")")" = "$ub" ] && [ "$(mode_of "$(unit_of "$B")")" = "$mb" ] \
       && [ "$(enabled_of "$B")" = enabled ]; then
        pass existing_install_survives_enable_failure_unchanged
    else
        fail existing_install_survives_enable_failure_unchanged "rc=$rc enabled=$(enabled_of "$B")"
    fi
    install_run "$B"
    rc=$?
    if [ "$rc" -eq 0 ] && grep -Fq "ExecStart=$(helper_of "$B")" "$(unit_of "$B")"; then
        pass reinstall_idempotent_and_consistent
    else
        fail reinstall_idempotent_and_consistent "rc=$rc"
    fi
    if grep -Fq "ExecStart=$T/evil/innogpu-repair-dri-nodes" "$(unit_of "$B")"; then
        fail path_injection_ignored
    else
        pass path_injection_ignored
    fi
fi

# ---- Bd: existing DISABLED install stays disabled after a failed reinstall ----
Bd="$T/bd"
install_run "$Bd"
env FAKE_SYSTEMCTL_STATE="$Bd/state" FAKE_SYSTEMCTL_LOG="$T/sd.log" bash "$T/bin/systemctl" disable innogpu-repair-dri-nodes.service
install_run "$Bd" env FAKE_SYSTEMCTL_START_FAIL=1
rc=$?
if [ "$rc" -ne 0 ] && [ "$(enabled_of "$Bd")" = disabled ]; then
    pass disabled_existing_install_stays_disabled
else
    fail disabled_existing_install_stays_disabled "rc=$rc enabled=$(enabled_of "$Bd")"
fi

# ---- B2: daemon-reload failure restores a pre-existing install byte-exact ----
B2="$T/b2"
install_run "$B2"
rc=$?
if [ "$rc" -ne 0 ]; then fail setup_install_b2 "rc=$rc"; else
    ub="$(hash_of "$(unit_of "$B2")")"; mb="$(mode_of "$(unit_of "$B2")")"
    install_run "$B2" env FAKE_SYSTEMCTL_RELOAD_FAIL=1
    rc=$?
    if [ "$rc" -ne 0 ] && [ -e "$(unit_of "$B2")" ] \
       && [ "$(hash_of "$(unit_of "$B2")")" = "$ub" ] && [ "$(mode_of "$(unit_of "$B2")")" = "$mb" ]; then
        pass daemon_reload_failure_restores_existing_install
    else
        fail daemon_reload_failure_restores_existing_install "rc=$rc"
    fi
fi

# ---- B3: unit WRITE failure on a fresh root rolls back created files ----
B3="$T/b3"
mkdir -p "$B3/etc/systemd/system"
chmod 555 "$B3/etc/systemd/system"
install_run "$B3"
rc=$?
chmod 755 "$B3/etc/systemd/system"
if [ "$rc" -ne 0 ] && [ ! -e "$(helper_of "$B3")" ] && [ ! -e "$(unit_of "$B3")" ]; then
    pass unit_write_failure_rolls_back_created
else
    fail unit_write_failure_rolls_back_created "rc=$rc"
fi

# ---- B4: backup failure aborts BEFORE the unit is touched ----
B4="$T/b4"
install_run "$B4"
rc=$?
if [ "$rc" -ne 0 ]; then fail setup_install_b4 "rc=$rc"; else
    ub="$(hash_of "$(unit_of "$B4")")"; mb="$(mode_of "$(unit_of "$B4")")"
    install_run "$B4" env FAKE_CP_FAIL=1
    rc=$?
    if [ "$rc" -ne 0 ] && [ "$(hash_of "$(unit_of "$B4")")" = "$ub" ] \
       && [ "$(mode_of "$(unit_of "$B4")")" = "$mb" ] && [ -e "$(helper_of "$B4")" ] \
       && [ "$(enabled_of "$B4")" = enabled ]; then
        pass backup_failure_preserves_original_unit
    else
        fail backup_failure_preserves_original_unit "rc=$rc enabled=$(enabled_of "$B4")"
    fi
fi

# ---- backup residue: neither success nor failure leaves innogpu-unit.* ----
if [ -z "$(backup_residue)" ]; then pass no_backup_residue_after_runs; else
    fail no_backup_residue_after_runs "$(backup_residue)"
fi

# ---- B5: RESTORE copy failure keeps the backup and reports it ----
B5="$T/b5"
install_run "$B5"
rc=$?
if [ "$rc" -ne 0 ]; then fail setup_install_b5 "rc=$rc"; else
    install_run "$B5" env FAKE_CP_FAIL_RESTORE=1 FAKE_SYSTEMCTL_START_FAIL=1
    rc=$?
    if [ "$rc" -ne 0 ] \
       && [ "$(ls "$T/tmp"/innogpu-unit.* 2>/dev/null | wc -l)" -eq 1 ] \
       && grep -Fq 'backup is preserved' "$T/out" \
       && ! grep -Fq 'changes made by this run were rolled back' "$T/out"; then
        pass restore_failure_preserves_backup
    else
        fail restore_failure_preserves_backup "rc=$rc backups=$(ls "$T/tmp"/innogpu-unit.* 2>/dev/null | wc -l) msg=$(grep -o 'backup is preserved.*' "$T/out" | head -1)"
    fi
fi

# ---- C/D: foreign files at the fallback path are never overwritten ----
C="$T/c"
mkdir -p "$C/usr/local/sbin"
printf 'foreign-file\n' > "$(helper_of "$C")"
install_run "$C"
rc=$?
if [ "$rc" -ne 0 ] && [ -f "$(helper_of "$C")" ] && grep -q foreign-file "$(helper_of "$C")"; then
    pass foreign_regular_file_not_overwritten
else
    fail foreign_regular_file_not_overwritten "rc=$rc"
fi
D="$T/d"
mkdir -p "$D/usr/local/sbin"
ln -s /bin/true "$(helper_of "$D")"
install_run "$D"
rc=$?
if [ "$rc" -ne 0 ] && [ -L "$(helper_of "$D")" ] && [ "$(readlink -f "$(helper_of "$D")")" = "$(readlink -f /bin/true)" ]; then
    pass foreign_symlink_not_overwritten
else
    fail foreign_symlink_not_overwritten "rc=$rc"
fi

# ---- E: packaged /usr/sbin helper branch (positive) ----
mkpkg() { # <root>
    local root="$1"
    mkdir -p "$root/usr/share/innogpu-fh2m-trixie" "$root/usr/sbin" "$root/usr/bin"
    cp "$ROOT/scripts/repair-dri-nodes.sh" "$root/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"
    chmod +x "$root/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"
    ln -sf ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh "$root/usr/sbin/innogpu-repair-dri-nodes"
}
E="$T/e"; mkpkg "$E"
install_run "$E"
rc=$?
if [ "$rc" -eq 0 ] && grep -Fq "ExecStart=$E/usr/sbin/innogpu-repair-dri-nodes" "$(unit_of "$E")" \
   && [ -L "$E/usr/bin/innogpu-repair-dri-nodes" ]; then
    pass packaged_helper_branch
else
    fail packaged_helper_branch "rc=$rc usrbin=$(test -e "$E/usr/bin/innogpu-repair-dri-nodes" && echo present || echo gone)"
fi

# ---- E2: packaged-branch failure rolls back this run's /usr/bin link ----
E2="$T/e2"; mkpkg "$E2"
install_run "$E2" env FAKE_SYSTEMCTL_START_FAIL=1
rc=$?
if [ "$rc" -ne 0 ] && [ ! -e "$E2/usr/bin/innogpu-repair-dri-nodes" ] \
   && [ ! -e "$(unit_of "$E2")" ] && [ -e "$E2/usr/sbin/innogpu-repair-dri-nodes" ]; then
    pass packaged_branch_failure_rolls_back_usrbin_link
else
    fail packaged_branch_failure_rolls_back_usrbin_link "rc=$rc"
fi

# ---- E3: /usr/bin convenience link creation failure only warns ----
E3="$T/e3"; mkpkg "$E3"
chmod 555 "$E3/usr/bin"
install_run "$E3"
rc=$?
chmod 755 "$E3/usr/bin"
if [ "$rc" -eq 0 ] && ! grep -Fq 'WARNING' "$T/out" && [ -e "$(unit_of "$E3")" ]; then
    fail usrbin_link_failure_warns_only "no warning emitted"
elif [ "$rc" -eq 0 ] && grep -Fq 'WARNING' "$T/out" && [ -e "$(unit_of "$E3")" ] \
   && [ ! -e "$E3/usr/bin/innogpu-repair-dri-nodes" ]; then
    pass usrbin_link_failure_warns_only
else
    fail usrbin_link_failure_warns_only "rc=$rc warn=$(grep -c WARNING "$T/out") usrbin=$(test -e "$E3/usr/bin/innogpu-repair-dri-nodes" && echo present || echo gone)"
fi

# ---- F: uninstall with EXPECTED-VERSION mismatch aborts with ZERO changes ----
F="$T/f"
install_run "$F"
rc=$?
mkdir -p "$F/etc/modules-load.d"
printf 'innogpu\n' > "$F/etc/modules-load.d/innogpu.conf"
: > "$T/sysctl-un.log"
env INNOGPU_UNINSTALL_TEST=1 INNOGPU_UNINSTALL_TEST_ROOT="$F" \
    INNOGPU_UNINSTALL_TEST_VERSION=4.0.0-i1 FAKE_SYSTEMCTL_LOG="$T/sysctl-un.log" \
    FAKE_SYSTEMCTL_STATE="$F/state" bash "$UNINSTALL" 3.3.3.42-patched-17 > "$T/un.out" 2>&1
rc=$?
if [ "$rc" -ne 0 ] && [ -e "$(helper_of "$F")" ] && [ -e "$(unit_of "$F")" ] \
   && [ -e "$F/etc/modules-load.d/innogpu.conf" ] && [ "$(enabled_of "$F")" = enabled ]; then
    pass version_mismatch_zero_changes
else
    fail version_mismatch_zero_changes "rc=$rc enabled=$(enabled_of "$F")"
fi
env INNOGPU_UNINSTALL_TEST=1 INNOGPU_UNINSTALL_TEST_ROOT="$F" \
    INNOGPU_UNINSTALL_TEST_VERSION=4.0.0-i1 FAKE_SYSTEMCTL_LOG="$T/sysctl-un2.log" \
    FAKE_SYSTEMCTL_STATE="$F/state" FORCE=1 bash "$UNINSTALL" 3.3.3.42-patched-17 > "$T/un2.out" 2>&1
rc=$?
if [ "$rc" -eq 0 ] && [ ! -e "$(helper_of "$F")" ] && [ ! -e "$(unit_of "$F")" ]; then
    pass force_overrides_version_check
else
    fail force_overrides_version_check "rc=$rc"
fi

# ---- G: package-absent uninstall only touches DRI-owned paths ----
G="$T/g"
install_run "$G"
mkdir -p "$G/etc/modules-load.d"
printf 'innogpu\n' > "$G/etc/modules-load.d/innogpu.conf"
uninstall_run "$G" env INNOGPU_UNINSTALL_TEST_VERSION=
rc=$?
if [ "$rc" -eq 0 ] && [ ! -e "$(helper_of "$G")" ] && [ ! -e "$(unit_of "$G")" ] \
   && [ -e "$G/etc/modules-load.d/innogpu.conf" ]; then
    pass package_absent_cleans_only_dri_paths
else
    fail package_absent_cleans_only_dri_paths "rc=$rc"
fi

# ---- H: uninstall removes owned helper/unit; foreign preserved; idempotent ----
H="$T/h"
install_run "$H"
uninstall_run "$H"
rc=$?
if [ "$rc" -eq 0 ] && [ ! -e "$(helper_of "$H")" ] && [ ! -e "$(unit_of "$H")" ] \
   && [ "$(enabled_of "$H")" = disabled ]; then
    pass uninstall_removes_owned_helper_and_unit
else
    fail uninstall_removes_owned_helper_and_unit "rc=$rc enabled=$(enabled_of "$H")"
fi
uninstall_run "$H"
rc=$?
if [ "$rc" -eq 0 ]; then pass uninstall_idempotent; else fail uninstall_idempotent "rc=$rc"; fi

mkdir -p "$H/other/scripts"
printf 'other-repo-script\n' > "$H/other/scripts/repair-dri-nodes.sh"
chmod +x "$H/other/scripts/repair-dri-nodes.sh"
ln -sf "$H/other/scripts/repair-dri-nodes.sh" "$(helper_of "$H")"
uninstall_run "$H"
rc=$?
if [ "$rc" -eq 0 ] && [ -L "$(helper_of "$H")" ]; then pass foreign_repo_symlink_preserved;
else fail foreign_repo_symlink_preserved "rc=$rc"; fi
rm -f "$(helper_of "$H")"
printf 'not-ours\n' > "$(helper_of "$H")"
uninstall_run "$H"
rc=$?
if [ "$rc" -eq 0 ] && [ -f "$(helper_of "$H")" ]; then pass foreign_regular_file_preserved;
else fail foreign_regular_file_preserved "rc=$rc"; fi

# ---- I: foreign same-named unit preserved by install and uninstall ----
I="$T/i"
mkdir -p "$I/etc/systemd/system"
printf '[Unit]\nDescription=Something Else\n' > "$(unit_of "$I")"
ub="$(hash_of "$(unit_of "$I")")"
install_run "$I"
rc=$?
if [ "$rc" -ne 0 ] && [ "$(hash_of "$(unit_of "$I")")" = "$ub" ] && [ ! -e "$(helper_of "$I")" ]; then
    pass foreign_unit_not_overwritten_by_install
else
    fail foreign_unit_not_overwritten_by_install "rc=$rc helper=$(test -e "$(helper_of "$I")" && echo present || echo gone)"
fi
: > "$T/sysctl-un.log"
uninstall_run "$I"
rc=$?
if [ "$rc" -eq 0 ] && [ -e "$(unit_of "$I")" ]; then
    pass foreign_unit_preserved_by_uninstall
else
    fail foreign_unit_preserved_by_uninstall "rc=$rc"
fi
: > "$T/sysctl-un.log"
uninstall_run "$I" env INNOGPU_UNINSTALL_TEST_VERSION=
rc=$?
if [ "$rc" -eq 0 ] && [ -e "$(unit_of "$I")" ]; then
    pass foreign_unit_preserved_package_absent
else
    fail foreign_unit_preserved_package_absent "rc=$rc"
fi

# ---- unsafe test roots fail closed ----
env INNOGPU_DRI_TEST_ROOT=/ FAKE_SYSTEMCTL_LOG="$T/bad.log" bash "$INSTALLER" > "$T/bad.out" 2>&1
rc=$?
if [ "$rc" -ne 0 ] && grep -q 'must be a non-empty absolute path' "$T/bad.out"; then
    pass test_root_slash_rejected
else
    fail test_root_slash_rejected "rc=$rc"
fi
env INNOGPU_DRI_TEST_ROOT=relative FAKE_SYSTEMCTL_LOG="$T/bad2.log" bash "$INSTALLER" > "$T/bad2.out" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then pass test_root_relative_rejected; else fail test_root_relative_rejected "rc=$rc"; fi
env INNOGPU_UNINSTALL_TEST=1 bash "$UNINSTALL" > "$T/unbad.out" 2>&1
rc=$?
if [ "$rc" -ne 0 ] && grep -q 'INNOGPU_UNINSTALL_TEST_ROOT' "$T/unbad.out"; then
    pass uninstall_test_root_required
else
    fail uninstall_test_root_required "rc=$rc"
fi

# ---- static counter-examples ----
if grep -Eq 'USER_NAME=.*(ok|[A-Za-z])|:[[:space:]]*ok([^A-Za-z]|$)' "$CHECK_X"; then
    fail no_hardcoded_user_in_check_x
else pass no_hardcoded_user_in_check_x; fi
if grep -Fq 'log in as ok' "$HOTLOAD"; then fail no_hardcoded_user_in_hotload;
else pass no_hardcoded_user_in_hotload; fi
if grep -Fq '"${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"' "$CHECK_X"; then
    pass user_resolution_order_present
else fail user_resolution_order_present; fi
if grep -Eq 'USER_HOME=\$"\{USER_HOME:-\$HOME\}"|:-\$HOME' "$CHECK_X"; then
    fail no_root_home_fallback
else pass no_root_home_fallback; fi
if grep -Fq 'su - "$X_USER"' "$CHECK_X" && ! grep -Fq 'su - "$USER_NAME"' "$CHECK_X"; then
    pass run_x_uses_resolved_user
else fail run_x_uses_resolved_user; fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[[ "$failed" -eq 0 ]]
