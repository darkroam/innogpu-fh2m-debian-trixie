#!/bin/bash
# Fixture for tools/debian/install-hygon-hda-audio.sh. No root, no systemd, no
# real home or /etc writes. INNOGPU_HDA_TEST_ROOT is refused unless it is an
# absolute directory other than /.
set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALLER="$ROOT/tools/debian/install-hygon-hda-audio.sh"
T="$(mktemp -d "${TMPDIR:-/tmp}/hygon-hda-tests.XXXXXX")"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/bin" "$T/tmp"
export TMPDIR="$T/tmp"
export PATH="$T/bin:$PATH"
LOG="$T/log"

cat > "$T/bin/systemctl" << 'END_SYSTEMCTL'
#!/bin/bash
printf '%s\n' "$*" >> "${FAKE_SYSTEMCTL_LOG:?}"
if [[ "${FAKE_SYSTEMCTL_USER_FAIL:-0}" == 1 && "$1" == "--user" && "$2" == "enable" ]]; then
    echo "fake: user enable failed" >&2
    exit 1
fi
exit 0
END_SYSTEMCTL
cat > "$T/bin/systemd-analyze" << 'END_ANALYZE'
#!/bin/bash
printf '%s\n' "$*" >> "${FAKE_SYSTEMCTL_LOG:?}"
if [[ "${FAKE_ANALYZE_FAIL:-0}" == 1 && "$1" == "verify" ]]; then
    echo "fake: verify failed" >&2
    exit 1
fi
exit 0
END_ANALYZE
cat > "$T/bin/cp" << 'END_CP'
#!/bin/bash
if [[ "${FAKE_CP_FAIL:-0}" == 1 ]]; then
    echo "fake: cp failed" >&2
    exit 1
fi
exec /bin/cp "$@"
END_CP
chmod +x "$T/bin/systemctl" "$T/bin/systemd-analyze" "$T/bin/cp"

pass=0
fail=0
ok() { pass=$((pass + 1)); printf 'hda_t=PASS # %s\n' "$1"; }
bad() { fail=$((fail + 1)); printf 'hda_t=FAIL # %s %s\n' "$1" "$2"; }

run_install() {
    local root="$1"
    shift
    env FAKE_SYSTEMCTL_LOG="$LOG" INNOGPU_HDA_TEST_ROOT="$root" \
        INNOGPU_X_USER="${USER:-testuser}" INNOGPU_X_HOME="$root/hda-user" \
        "$@" bash "$INSTALLER"
}

fresh() {
    local root="$1"
    mkdir -p "$root/hda-user"
    : > "$LOG"
}

if bash "$INSTALLER" --help >/dev/null; then ok help; else bad help rc; fi
if INNOGPU_HDA_TEST_ROOT=relative INNOGPU_X_HOME="$T/nope" bash "$INSTALLER" >"$T/rel.out" 2>&1; then
    bad relative_root "rc=0"
else
    ok relative_root
fi
if INNOGPU_HDA_TEST_ROOT=/ INNOGPU_X_HOME=/hda-user bash "$INSTALLER" >"$T/slash.out" 2>&1; then
    bad slash_root "rc=0"
else
    grep -q 'absolute path' "$T/slash.out" && ok slash_root || bad slash_root "$(cat "$T/slash.out")"
fi

R="$T/fresh"
fresh "$R"
if run_install "$R" >"$R/out" 2>"$R/err"; then
    if grep -q 'hygon_hda_user_service=SKIP reason=no_session' "$R/out" \
        && grep -q 'hygon_hda_install=PASS' "$R/out" \
        && grep -q 'hygon_hda_verify=PASS' "$R/out" \
        && [[ -f "$R/etc/systemd/system/hygon-hda-audio.service" ]] \
        && [[ -f "$R/hda-user/.local/bin/hygon-hda-audio-user-apply" ]] \
        && ! grep -q -- '--user enable' "$LOG"; then
        verify_line="$(grep -n 'verify ' "$LOG" | head -1 | cut -d: -f1)"
        enable_line="$(grep -n '^enable ' "$LOG" | head -1 | cut -d: -f1)"
        if [[ -n "$verify_line" && -n "$enable_line" && "$verify_line" -lt "$enable_line" ]]; then
            ok fresh_skip_user
        else
            bad fresh_skip_user "order verify=$verify_line enable=$enable_line log=$(tr '\n' ';' < "$LOG")"
        fi
    else
        bad fresh_skip_user "out=$(tr '\n' ';' < "$R/out") log=$(tr '\n' ';' < "$LOG") err=$(tr '\n' ';' < "$R/err")"
    fi
else
    bad fresh_skip_user "rc != 0 err=$(cat "$R/err") out=$(cat "$R/out")"
fi

baks="$(find "$R" -name '*.bak-hygon-hda-*' | wc -l)"
if run_install "$R" >"$R/out2" 2>"$R/err2" && [[ "$(find "$R" -name '*.bak-hygon-hda-*' | wc -l)" == "$baks" ]]; then
    ok reinstall_identical
else
    bad reinstall_identical "baks=$baks err=$(cat "$R/err2" 2>/dev/null)"
fi

C="$T/conflict"
fresh "$C"
mkdir -p "$C/etc/modules-load.d"
printf 'foreign-module\n' > "$C/etc/modules-load.d/hygon-hda-audio.conf"
before="$(cat "$C/etc/modules-load.d/hygon-hda-audio.conf")"
if run_install "$C" >"$C/out" 2>"$C/err"; then
    bak="$(find "$C/etc/modules-load.d" -name 'hygon-hda-audio.conf.bak-hygon-hda-*' | head -1)"
    if [[ -n "$bak" && "$(cat "$bak")" == "$before" ]] && grep -qx snd_hda_intel "$C/etc/modules-load.d/hygon-hda-audio.conf"; then
        ok conflict_backup
    else
        bad conflict_backup "bak=$bak err=$(cat "$C/err")"
    fi
else
    bad conflict_backup "rc != 0 $(cat "$C/err")"
fi

B="$T/bakfail"
fresh "$B"
mkdir -p "$B/etc/modules-load.d"
printf 'keep-me\n' > "$B/etc/modules-load.d/hygon-hda-audio.conf"
if FAKE_CP_FAIL=1 run_install "$B" >"$B/out" 2>"$B/err"; then
    bad backup_fail "rc=0"
else
    if [[ "$(cat "$B/etc/modules-load.d/hygon-hda-audio.conf")" == "keep-me" ]] && ! grep -q '^enable ' "$LOG"; then
        ok backup_fail
    else
        bad backup_fail "file=$(cat "$B/etc/modules-load.d/hygon-hda-audio.conf") log=$(cat "$LOG") err=$(cat "$B/err")"
    fi
fi

V="$T/verify"
fresh "$V"
mkdir -p "$V/etc/systemd/system"
printf 'foreign unit bytes\n' > "$V/etc/systemd/system/hygon-hda-audio.service"
if FAKE_ANALYZE_FAIL=1 run_install "$V" >"$V/out" 2>"$V/err"; then
    bad verify_fail "rc=0"
else
    if [[ "$(cat "$V/etc/systemd/system/hygon-hda-audio.service")" == "foreign unit bytes" ]] \
        && ! grep -q '^enable ' "$LOG" \
        && ! [[ -e "$V/etc/modules-load.d/hygon-hda-audio.conf" ]]; then
        ok verify_rollback
    else
        bad verify_rollback "unit=$(cat "$V/etc/systemd/system/hygon-hda-audio.service") log=$(cat "$LOG") err=$(cat "$V/err")"
    fi
fi

U="$T/userfail"
fresh "$U"
if FAKE_SYSTEMCTL_USER_FAIL=1 INNOGPU_HDA_ASSUME_USER_BUS=1 run_install "$U" >"$U/out" 2>"$U/err"; then
    bad user_fail "rc=0"
else
    if [[ ! -e "$U/etc/systemd/system/hygon-hda-audio.service" ]] && grep -q '^disable ' "$LOG"; then
        ok user_fail_rolls_back
    else
        bad user_fail_rolls_back "log=$(cat "$LOG") err=$(cat "$U/err") out=$(cat "$U/out")"
    fi
fi

Q="$T/uninstall"
fresh "$Q"
if run_install "$Q" >"$Q/out" 2>"$Q/err" \
    && env FAKE_SYSTEMCTL_LOG="$LOG" INNOGPU_HDA_TEST_ROOT="$Q" \
        INNOGPU_X_USER="${USER:-testuser}" INNOGPU_X_HOME="$Q/hda-user" \
        bash "$INSTALLER" --uninstall >"$Q/un" 2>"$Q/unerr"; then
    if grep -q 'hygon_hda_uninstall=PASS' "$Q/un" \
        && [[ ! -e "$Q/etc/systemd/system/hygon-hda-audio.service" ]] \
        && [[ ! -e "$Q/hda-user/.local/bin/hygon-hda-audio-user-apply" ]] \
        && [[ ! -e "$Q/hda-user/.asoundrc" ]] \
        && grep -q '^disable ' "$LOG"; then
        ok uninstall
    else
        bad uninstall "un=$(cat "$Q/un") log=$(cat "$LOG")"
    fi
else
    bad uninstall "rc err=$(cat "$Q/unerr" 2>/dev/null) installerr=$(cat "$Q/err" 2>/dev/null)"
fi

F="$T/foreign"
fresh "$F"
mkdir -p "$F/etc/systemd/system" "$F/hda-user"
printf 'Description=Bind Hygon HDA audio and enable internal speakers\n' > "$F/etc/systemd/system/hygon-hda-audio.service"
printf 'not our asoundrc\n' > "$F/hda-user/.asoundrc"
: > "$LOG"
if env FAKE_SYSTEMCTL_LOG="$LOG" INNOGPU_HDA_TEST_ROOT="$F" \
    INNOGPU_X_USER="${USER:-testuser}" INNOGPU_X_HOME="$F/hda-user" \
    bash "$INSTALLER" --uninstall >"$F/un" 2>"$F/unerr"; then
    bad foreign_uninstall "rc=0"
else
    if grep -q 'reason=foreign:' "$F/unerr" \
        && grep -q 'Bind Hygon HDA' "$F/etc/systemd/system/hygon-hda-audio.service" \
        && [[ ! -s "$LOG" ]]; then
        ok foreign_uninstall
    else
        bad foreign_uninstall "err=$(cat "$F/unerr") log=$(cat "$LOG")"
    fi
fi

printf 'hda_summary pass=%d fail=%d\n' "$pass" "$fail"
[[ "$fail" -eq 0 ]]
