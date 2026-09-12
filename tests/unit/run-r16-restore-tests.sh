#!/usr/bin/env bash
# tests/unit/run-r16-restore-tests.sh — tools/r16-f-payload-integrity.py restore 子命令单元验证
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §一（v12）：
# 排他锁（O_NOFOLLOW + flock NB + 永不删除）、journal 写前状态机
# （staged/moving_old/moved_old/moving_new/committed）、逐状态真值表
# （仅可产生组合；不可产生组合 fail-closed 保留现场）、journal 字段安全、
# 内容复核（SHA/mode/target + 目录 0755）、DEBIAN/ 剔除、故障注入。
# 全合成夹具（FPI_RESTORE_FIXTURE=1：小载荷 + 合成 deb + 合成 manifest），
# 只写 $TMP，不触碰真实 debs/vendor/build。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/r16-f-payload-integrity.py"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }
command -v dpkg-deb >/dev/null 2>&1 || { echo "FATAL: dpkg-deb required" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/restore-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

cat > "$TMP/mkfixture.py" <<'PY'
#!/usr/bin/env python3
"""构造合成小载荷 deb + manifest；输出 shell 赋值行。"""
import hashlib, json, os, subprocess, sys

def build(out, mutate="", link_target="a.txt", file_mode="0644"):
    payload = os.path.join(out, "payload")
    debroot = os.path.join(out, "debroot")
    os.makedirs(os.path.join(payload, "usr", "sub"), exist_ok=True)
    os.makedirs(os.path.join(debroot, "usr", "sub"), exist_ok=True)
    os.makedirs(os.path.join(debroot, "DEBIAN"), exist_ok=True)
    os.chmod(os.path.join(debroot, "usr"), 0o755)
    os.chmod(os.path.join(debroot, "usr", "sub"), 0o755)

    content_a = b"A-CONTENT\n"
    content_b = b"B-CONTENT\n"
    deb_a = content_a
    if mutate == "content-a":
        deb_a = b"DRIFTED\n"   # 仅 deb 漂移；manifest 仍按规范内容生成
    with open(os.path.join(payload, "usr", "a.txt"), "wb") as fh:
        fh.write(content_a)
    with open(os.path.join(payload, "usr", "sub", "b.txt"), "wb") as fh:
        fh.write(content_b)
    os.symlink(link_target, os.path.join(payload, "usr", "link.txt"))
    for rel in ("usr/a.txt", "usr/sub/b.txt"):
        os.chmod(os.path.join(payload, rel), 0o644)  # umask 无关
    os.chmod(os.path.join(payload, "usr"), 0o755)
    os.chmod(os.path.join(payload, "usr", "sub"), 0o755)
    if mutate == "extra-file":
        with open(os.path.join(payload, "usr", "extra.txt"), "wb") as fh:
            fh.write(b"EXTRA\n")

    # debroot：先复制常规文件 + 指定 mode，再 symlink（deb 内也带链接）
    deb_link = link_target
    if mutate == "link-target-drift":
        deb_link = "b.txt"
    with open(os.path.join(debroot, "usr", "a.txt"), "wb") as fh:
        fh.write(deb_a)
    with open(os.path.join(debroot, "usr", "sub", "b.txt"), "wb") as fh:
        fh.write(content_b)
    os.symlink(deb_link, os.path.join(debroot, "usr", "link.txt"))
    for rel in ("usr/a.txt", "usr/sub/b.txt"):
        os.chmod(os.path.join(debroot, rel), int(file_mode, 8))
    if mutate == "extra-file":
        with open(os.path.join(debroot, "usr", "extra.txt"), "wb") as fh:
            fh.write(b"EXTRA\n")
    with open(os.path.join(debroot, "DEBIAN", "control"), "w") as fh:
        fh.write("Package: fixture-restore\nVersion: 1.0\n"
                 "Architecture: all\nMaintainer: fixture\n"
                 "Description: fixture\n")
    deb = os.path.join(out, "fixture.deb")
    subprocess.run(["dpkg-deb", "--root-owner-group", "-b", debroot, deb],
                   check=True, capture_output=True)

    def sha(p):
        return hashlib.sha256(open(p, "rb").read()).hexdigest()
    # manifest 恒按规范 0644/0777 记录；file_mode 只影响 deb 内 chmod（t09）
    entries = [
        {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
         "sha256": sha(os.path.join(payload, "usr/a.txt")), "size": 9,
         "source_path": "usr/a.txt", "vendor_path": "fantgpu/usr/a.txt",
         "mode": "0644", "variant": None, "materialize": "direct"},
        {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
         "sha256": sha(os.path.join(payload, "usr/sub/b.txt")), "size": 9,
         "source_path": "usr/sub/b.txt",
         "vendor_path": "fantgpu/usr/sub/b.txt",
         "mode": "0644", "variant": None, "materialize": "direct"},
        {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
         "sha256": None, "size": None, "source_path": "usr/link.txt",
         "vendor_path": "fantgpu/usr/link.txt", "mode": "0777",
         "target": link_target, "variant": None, "materialize": "direct"},
    ]
    if mutate != "extra-file":
        obj = {"format_version": 1, "source_package": "fixture",
               "source_version": "1", "source_deb_sha256": "0" * 64,
               "input_entries": 3, "dir_entries": 2, "file_entries": 2,
               "symlink_entries": 1, "entries": entries}
    else:
        # extra-file：manifest 不含 extra → 双射失败；但 file_entries 不变
        obj = {"format_version": 1, "source_package": "fixture",
               "source_version": "1", "source_deb_sha256": "0" * 64,
               "input_entries": 3, "dir_entries": 2, "file_entries": 2,
               "symlink_entries": 1, "entries": entries}
    manifest = os.path.join(out, "manifest.json")
    with open(manifest, "w") as fh:
        json.dump(obj, fh, indent=2, sort_keys=True)
    print("FIX=%s" % out)
    print("DEB=%s" % deb)
    print("MANIFEST=%s" % manifest)
    print("PAYLOAD=%s" % payload)

if __name__ == "__main__":
    out = sys.argv[1]
    spec = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    os.makedirs(out, exist_ok=True)
    build(out, spec.get("mutate", ""), spec.get("link_target", "a.txt"),
          spec.get("file_mode", "0644"))
PY

mkfix() {  # mkfix <name> [spec-json]
    local name="$1" spec="${2-}"
    [ -n "$spec" ] || spec='{}'
    rm -rf "$TMP/$name"
    python3 "$TMP/mkfixture.py" "$TMP/$name" "$spec"
}

# run_restore <vendor-dir-abs> [env...]：restore 到指定 vendor 目录
RESTORE_RC=0; RESTORE_ERR=""; RESTORE_OUT=""
run_restore() {
    local vendor="$1"; shift
    set +e
    FPI_RESTORE_FIXTURE=1 "$@" python3 "$TOOL" restore \
        --root "$TMP" --deb "$DEB" --vendor-dir "$vendor" \
        --manifest "$MANIFEST" > "$TMP/r.out" 2> "$TMP/r.err"
    RESTORE_RC=$?
    set -e
    RESTORE_ERR="$(cat "$TMP/r.err")"
    RESTORE_OUT="$(cat "$TMP/r.out")"
}

make_gen() {  # 用 PAYLOAD 复制一份目录树（供 C/T/O 场景）
    local dst="$1"
    rm -rf "$dst"
    cp -r "$PAYLOAD" "$dst"
}

vendor_of() { echo "$TMP/$1/vendor/fantgpu"; }      # C
parent_of() { echo "$TMP/$1/vendor"; }              # parent（锁/journal）
txn_of()    { echo "$TMP/$1/vendor/.fantgpu-txn.$2"; }
old_of()    { echo "$TMP/$1/vendor/.fantgpu-old.$2"; }
journal_of(){ echo "$TMP/$1/vendor/.fantgpu-restore.journal"; }
lock_of()   { echo "$TMP/$1/vendor/.fantgpu-restore.lock"; }

mkdir_ok() { mkdir -p "$1"; chmod 0755 "$(dirname "$1")" 2>/dev/null || true; }

# ------------------------------------------------ t01 首次落库正向
eval "$(mkfix t01)"
mkdir -p "$TMP/t01/vendor"
run_restore "$TMP/t01/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 0 ] \
   && [ -f "$TMP/t01/vendor/fantgpu/usr/a.txt" ] \
   && [ ! -e "$(journal_of t01)" ] \
   && [ -z "$(find "$TMP/t01/vendor" -maxdepth 1 -name '.fantgpu-txn.*' \
        -o -name '.fantgpu-old.*')" ]; then
    ok t01
else
    bad t01 "rc=$RESTORE_RC"
fi

# ------------------------------------------------ t02 已有 C 重落库（pre=T 路径）
run_restore "$TMP/t01/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 0 ] \
   && [ -f "$TMP/t01/vendor/fantgpu/usr/a.txt" ] \
   && [ ! -e "$(journal_of t01)" ]; then
    ok t02
else
    bad t02 "rc=$RESTORE_RC"
fi

# ------------------------------------------------ t03 锁文件复用（永不删除）
if [ -f "$(lock_of t01)" ]; then ok t03; else bad t03 "lock file missing"; fi

# ------------------------------------------------ t04 deb 缺失 → rc=2
eval "$(mkfix t04)"
mkdir -p "$TMP/t04/vendor"
set +e
FPI_RESTORE_FIXTURE=1 python3 "$TOOL" restore --root "$TMP" \
    --deb "$TMP/nope.deb" --vendor-dir "$TMP/t04/vendor/fantgpu" \
    --manifest "$MANIFEST" >/dev/null 2>&1
RC4=$?
set -e
if [ "$RC4" -eq 2 ]; then ok t04; else bad t04 "rc=$RC4 (want 2)"; fi

# ------------------------------------------------ t05 manifest 缺失 → rc=2
eval "$(mkfix t05)"
mkdir -p "$TMP/t05/vendor"
set +e
FPI_RESTORE_FIXTURE=1 python3 "$TOOL" restore --root "$TMP" \
    --deb "$DEB" --vendor-dir "$TMP/t05/vendor/fantgpu" \
    --manifest "$TMP/nope.json" >/dev/null 2>&1
RC5=$?
set -e
if [ "$RC5" -eq 2 ]; then ok t05; else bad t05 "rc=$RC5 (want 2)"; fi

# ------------------------------------------------ t06 双射失败（extra file）→ 首代零残留
eval "$(mkfix t06 '{"mutate":"extra-file"}')"
mkdir -p "$TMP/t06/vendor"
run_restore "$TMP/t06/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && [ ! -e "$TMP/t06/vendor/fantgpu" ] \
   && [ -z "$(find "$TMP/t06/vendor" -maxdepth 1 -name '.fantgpu-*' \
        | grep -v lock)" ]; then
    ok t06
else
    bad t06 "rc=$RESTORE_RC"
fi

# ------------------------------------------------ t07 DEBIAN/ 未剔除 → rc=2
eval "$(mkfix t07)"
mkdir -p "$TMP/t07/vendor"
set +e
FPI_RESTORE_FIXTURE=1 FPI_RESTORE_KEEP_DEBIAN=1 python3 "$TOOL" restore \
    --root "$TMP" --deb "$DEB" --vendor-dir "$TMP/t07/vendor/fantgpu" \
    --manifest "$MANIFEST" >/dev/null 2>"$TMP/t07.err"
RC7=$?
set -e
if [ "$RC7" -eq 2 ] && [ ! -e "$TMP/t07/vendor/fantgpu" ]; then
    ok t07
else
    bad t07 "rc=$RC7 (want 2)"
fi

# ------------------------------------------------ t08 内容漂移（deb 与 manifest 不符）→ rc=2
eval "$(mkfix t08 '{"mutate":"content-a"}')"
mkdir -p "$TMP/t08/vendor"
run_restore "$TMP/t08/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && [ ! -e "$TMP/t08/vendor/fantgpu" ]; then
    ok t08
else
    bad t08 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t09 mode 漂移 → rc=2
eval "$(mkfix t09 '{"file_mode":"0600"}')"
mkdir -p "$TMP/t09/vendor"
run_restore "$TMP/t09/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "mode drift" "$TMP/r.err"; then
    ok t09
else
    bad t09 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t10 链接目标漂移 → rc=2
eval "$(mkfix t10 '{"mutate":"link-target-drift"}')"
mkdir -p "$TMP/t10/vendor"
run_restore "$TMP/t10/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "symlink target drift" "$TMP/r.err"; then
    ok t10
else
    bad t10 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t11 锁文件 symlink → rc=2
eval "$(mkfix t11)"
mkdir -p "$TMP/t11/vendor"
ln -s /tmp/victim "$TMP/t11/vendor/.fantgpu-restore.lock"
run_restore "$TMP/t11/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "symlink" "$TMP/r.err"; then
    ok t11
else
    bad t11 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t12 并发竞争：锁被持有 → rc=2
eval "$(mkfix t12)"
mkdir -p "$TMP/t12/vendor"
python3 - "$TMP/t12/vendor" <<'PY' &
import fcntl, os, sys, time
path = os.path.join(sys.argv[1], ".fantgpu-restore.lock")
fd = os.open(path, os.O_CREAT | os.O_RDWR, 0o644)
fcntl.flock(fd, fcntl.LOCK_EX)
time.sleep(4)
PY
HOLDER=$!
sleep 0.5
run_restore "$TMP/t12/vendor/fantgpu"
wait $HOLDER
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "already in progress" "$TMP/r.err"; then
    ok t12
else
    bad t12 "rc=$RESTORE_RC"
fi

# ------------------------------------------------ t13 journal 为 symlink → rc=2
eval "$(mkfix t13)"
mkdir -p "$TMP/t13/vendor"
ln -s /tmp/victim "$(journal_of t13)"
run_restore "$TMP/t13/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "symlink" "$TMP/r.err"; then
    ok t13
else
    bad t13 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t14 journal 字段非法（路径逃逸）→ rc=2
eval "$(mkfix t14)"
mkdir -p "$TMP/t14/vendor"
printf '{"state": "staged", "pre_existing": true, "txn": "../escape", "old": ".fantgpu-old.abcdef01"}\n' > "$(journal_of t14)"
run_restore "$TMP/t14/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "not tool-generated" "$TMP/r.err"; then
    ok t14
else
    bad t14 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t15 journal 字段篡改（pre 非布尔）→ rc=2
eval "$(mkfix t15)"
mkdir -p "$TMP/t15/vendor"
printf '{"state": "staged", "pre_existing": "yes", "txn": ".fantgpu-txn.abcdef01", "old": ".fantgpu-old.abcdef01"}\n' > "$(journal_of t15)"
run_restore "$TMP/t15/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ]; then ok t15; else bad t15 "rc=$RESTORE_RC (want 2)"; fi

# ------------------------------------------------ t16 无 journal 的 .old → fail-closed
eval "$(mkfix t16)"
mkdir -p "$TMP/t16/vendor"
make_gen "$(old_of t16 deadbeef)"
run_restore "$TMP/t16/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] && grep -Fq "without journal" "$TMP/r.err" \
   && [ -e "$(old_of t16 deadbeef)" ]; then
    ok t16
else
    bad t16 "rc=$RESTORE_RC (want 2)"
fi

# ------------------------------------------------ t17 无 journal 的 txn → 安全清理后继续
eval "$(mkfix t17)"
mkdir -p "$TMP/t17/vendor"
make_gen "$(txn_of t17 cafebabe)"
run_restore "$TMP/t17/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 0 ] \
   && [ ! -e "$(txn_of t17 cafebabe)" ] \
   && [ -f "$TMP/t17/vendor/fantgpu/usr/a.txt" ]; then
    ok t17
else
    bad t17 "rc=$RESTORE_RC"
fi

# ---------------- 真值表场景（C/T/O 用 PAYLOAD 副本；content 与 manifest 一致）
craft() {  # craft <name> <state> <pre(T/F)> <C:0/1> <T:0/1> <O:0/1>
    local name="$1" state="$2" pre="$3" c="$4" t="$5" o="$6"
    local prejson
    case "$pre" in T) prejson=true ;; F) prejson=false ;; esac
    mkdir -p "$TMP/$name/vendor"
    [ "$c" = 1 ] && make_gen "$(vendor_of "$name")"
    [ "$t" = 1 ] && make_gen "$(txn_of "$name" aabbcc01)"
    [ "$o" = 1 ] && make_gen "$(old_of "$name" aabbcc01)"
    printf '{"state": "%s", "pre_existing": %s, "txn": ".fantgpu-txn.aabbcc01", "old": ".fantgpu-old.aabbcc01"}\n' "$state" "$prejson" > "$(journal_of "$name")"
}

truth_ok() {  # 可产生组合：restore 成功且零残留
    local name="$1"
    run_restore "$TMP/$name/vendor/fantgpu"
    local resid
    resid="$(find "$TMP/$name/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
        -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) \
        | wc -l)"
    if [ "$RESTORE_RC" -eq 0 ] && [ "$resid" -eq 0 ] \
       && [ -f "$TMP/$name/vendor/fantgpu/usr/a.txt" ]; then
        ok "$name"
    else
        bad "$name" "rc=$RESTORE_RC resid=$resid"
    fi
}

truth_fail() {  # 不可产生组合：rc=2 且现场保留
    local name="$1"
    run_restore "$TMP/$name/vendor/fantgpu"
    local journal_present=0
    [ -e "$(journal_of "$name")" ] && journal_present=1
    if [ "$RESTORE_RC" -eq 2 ] && [ "$journal_present" -eq 1 ] \
       && grep -Fq "not producible" "$TMP/r.err"; then
        ok "$name"
    else
        bad "$name" "rc=$RESTORE_RC journal=$journal_present"
    fi
}

eval "$(mkfix ttr)"
for combo in "t18/staged/T/1/1/0" "t19/staged/T/1/0/0" "t20/staged/F/0/1/0" \
             "t21/staged/F/0/0/0" "t22/moving_old/T/1/1/0" \
             "t23/moving_old/T/0/1/1" "t24/moving_old/T/1/0/0" \
             "t25/moved_old/T/0/1/1" "t26/moved_old/T/1/1/0" \
             "t27/moved_old/T/1/0/0" "t28/moving_new/T/0/1/1" \
             "t29/moving_new/T/1/0/1" "t30/moving_new/T/1/1/0" \
             "t31/moving_new/T/1/0/0" "t32/moving_new/F/0/1/0" \
             "t33/moving_new/F/0/0/0" "t34/moving_new/F/1/0/0" \
             "t35/committed/T/1/0/1" "t36/committed/T/1/0/0" \
             "t37/committed/F/1/0/0"; do
    IFS='/' read -r label state pre c t o <<< "$combo"
    craft "$label" "$state" "$pre" "$c" "$t" "$o"
    truth_ok "$label"
done
for combo in "t38/moving_old/F/0/1/0" "t39/moved_old/F/0/1/1" \
             "t40/moving_new/F/1/0/1" "t41/moved_old/T/0/1/0" \
             "t42/committed/T/1/1/0" "t43/moving_new/T/1/1/1"; do
    IFS='/' read -r label state pre c t o <<< "$combo"
    craft "$label" "$state" "$pre" "$c" "$t" "$o"
    truth_fail "$label"
done

# ---------------- t44 journal 写入失败（moving_old）→ 旧代零改动 rc=2
eval "$(mkfix t44)"
mkdir -p "$TMP/t44/vendor"
make_gen "$TMP/t44/vendor/fantgpu"
before="$(sha256sum "$TMP/t44/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-journal.moving_old \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t44/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>&1
RC44=$?
set -e
after="$(sha256sum "$TMP/t44/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
resid="$(find "$TMP/t44/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC44" -eq 2 ] && [ "$before" = "$after" ] && [ "$resid" -eq 0 ]; then
    ok t44
else
    bad t44 "rc=$RC44 resid=$resid"
fi

# ---------------- t45 journal 写入失败（moved_old，old-mv 已成功）→ 恢复
eval "$(mkfix t45)"
mkdir -p "$TMP/t45/vendor"
make_gen "$TMP/t45/vendor/fantgpu"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-journal.moved_old \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t45/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>"$TMP/t45.err"
RC45=$?
set -e
resid="$(find "$TMP/t45/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC45" -eq 2 ] && [ -f "$TMP/t45/vendor/fantgpu/usr/a.txt" ] \
   && [ "$resid" -eq 0 ]; then
    ok t45
else
    bad t45 "rc=$RC45 resid=$resid"
fi

# ---------------- t46 journal 写入失败（committed，new-mv 已成功）→ 按 committed 清理
eval "$(mkfix t46)"
mkdir -p "$TMP/t46/vendor"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-journal.committed \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t46/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>"$TMP/t46.err"
RC46=$?
set -e
resid="$(find "$TMP/t46/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC46" -eq 2 ] && [ -f "$TMP/t46/vendor/fantgpu/usr/a.txt" ] \
   && [ "$resid" -eq 0 ]; then
    ok t46
else
    bad t46 "rc=$RC46 resid=$resid"
fi

# ---------------- t47 mv.old 失败 → 旧代零改动 rc=2
eval "$(mkfix t47)"
mkdir -p "$TMP/t47/vendor"
make_gen "$TMP/t47/vendor/fantgpu"
before="$(sha256sum "$TMP/t47/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-mv.old \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t47/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>&1
RC47=$?
set -e
after="$(sha256sum "$TMP/t47/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
if [ "$RC47" -eq 2 ] && [ "$before" = "$after" ]; then
    ok t47
else
    bad t47 "rc=$RC47"
fi

# ---------------- t48 mv.new 失败（pre=T）→ 旧代恢复 rc=2
eval "$(mkfix t48)"
mkdir -p "$TMP/t48/vendor"
make_gen "$TMP/t48/vendor/fantgpu"
before="$(sha256sum "$TMP/t48/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-mv.new \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t48/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>&1
RC48=$?
set -e
after="$(sha256sum "$TMP/t48/vendor/fantgpu/usr/a.txt" | cut -d' ' -f1)"
resid="$(find "$TMP/t48/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC48" -eq 2 ] && [ "$before" = "$after" ] && [ "$resid" -eq 0 ]; then
    ok t48
else
    bad t48 "rc=$RC48 resid=$resid"
fi

# ---------------- t49 mv.new 失败（pre=F 首次落库）→ 无当前代、零残留 rc=2
eval "$(mkfix t49)"
mkdir -p "$TMP/t49/vendor"
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-mv.new \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t49/vendor/fantgpu" --manifest "$MANIFEST" \
    >/dev/null 2>&1
RC49=$?
set -e
resid="$(find "$TMP/t49/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC49" -eq 2 ] && [ ! -e "$TMP/t49/vendor/fantgpu" ] \
   && [ "$resid" -eq 0 ]; then
    ok t49
else
    bad t49 "rc=$RC49 resid=$resid"
fi

# ---------------- t50 清理失败 → rc=0 + WARNING + 残留；再次 restore 清零
eval "$(mkfix t50)"
mkdir -p "$TMP/t50/vendor"
make_gen "$TMP/t50/vendor/fantgpu"     # pre=T：old 真实存在，残留可观察
set +e
FPI_RESTORE_FIXTURE=1 FPI_FAIL_INJECT=restore-cleanup.1 \
    python3 "$TOOL" restore --root "$TMP" --deb "$DEB" \
    --vendor-dir "$TMP/t50/vendor/fantgpu" --manifest "$MANIFEST" \
    > "$TMP/t50.out" 2> "$TMP/t50.err"
RC50=$?
set -e
resid1="$(find "$TMP/t50/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
jstate1="$(python3 -c "
import json
print(json.load(open('$TMP/t50/vendor/.fantgpu-restore.journal'))['state'])" \
    2>/dev/null || echo missing)"
run_restore "$TMP/t50/vendor/fantgpu"
resid2="$(find "$TMP/t50/vendor" -maxdepth 1 \( -name '.fantgpu-txn.*' \
    -o -name '.fantgpu-old.*' -o -name '.fantgpu-restore.journal' \) | wc -l)"
if [ "$RC50" -eq 0 ] && grep -Fq "WARNING" "$TMP/t50.err" \
   && [ "$resid1" -gt 0 ] && [ "$jstate1" = "committed" ] \
   && [ "$RESTORE_RC" -eq 0 ] && [ "$resid2" -eq 0 ]; then
    ok t50
else
    bad t50 "rc=$RC50/$RESTORE_RC resid=$resid1/$resid2 jstate=$jstate1"
fi

# ---------------- t51 有 journal 时未引用 txn 残留 → fail-closed 保留现场
eval "$(mkfix t51)"
mkdir -p "$TMP/t51/vendor"
make_gen "$TMP/t51/vendor/.fantgpu-txn.aabbcc01"   # journal 指名对象
make_gen "$TMP/t51/vendor/.fantgpu-txn.99dd99dd"   # 未引用残留
printf '{"state": "staged", "pre_existing": false, "txn": ".fantgpu-txn.aabbcc01", "old": ".fantgpu-old.aabbcc01"}\n' \
    > "$(journal_of t51)"
run_restore "$TMP/t51/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Fq "unreferenced transaction candidates" "$TMP/r.err" \
   && [ -e "$TMP/t51/vendor/.fantgpu-txn.99dd99dd" ] \
   && [ -e "$(journal_of t51)" ]; then
    ok t51
else
    bad t51 "rc=$RESTORE_RC"
fi

# ---------------- t52 有 journal 时未引用 old 残留 → fail-closed 保留现场
eval "$(mkfix t52)"
mkdir -p "$TMP/t52/vendor"
make_gen "$TMP/t52/vendor/.fantgpu-txn.aabbcc01"
make_gen "$TMP/t52/vendor/.fantgpu-old.99dd99dd"
printf '{"state": "staged", "pre_existing": false, "txn": ".fantgpu-txn.aabbcc01", "old": ".fantgpu-old.aabbcc01"}\n' \
    > "$(journal_of t52)"
run_restore "$TMP/t52/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Fq "unreferenced transaction candidates" "$TMP/r.err" \
   && [ -e "$TMP/t52/vendor/.fantgpu-old.99dd99dd" ]; then
    ok t52
else
    bad t52 "rc=$RESTORE_RC"
fi

# ---------------- t53 无 journal 的 txn 为普通文件 → 安全删除后继续
eval "$(mkfix t53)"
mkdir -p "$TMP/t53/vendor"
printf 'junk\n' > "$TMP/t53/vendor/.fantgpu-txn.file0001"
run_restore "$TMP/t53/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 0 ] \
   && [ ! -e "$TMP/t53/vendor/.fantgpu-txn.file0001" ] \
   && [ -f "$TMP/t53/vendor/fantgpu/usr/a.txt" ]; then
    ok t53
else
    bad t53 "rc=$RESTORE_RC"
fi

# ---------------- t54 无 journal 的 txn 为 symlink → 安全 unlink 后继续
eval "$(mkfix t54)"
mkdir -p "$TMP/t54/vendor"
ln -s /tmp/victim "$TMP/t54/vendor/.fantgpu-txn.link0001"
run_restore "$TMP/t54/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 0 ] \
   && [ ! -e "$TMP/t54/vendor/.fantgpu-txn.link0001" ] \
   && [ -f "$TMP/t54/vendor/fantgpu/usr/a.txt" ]; then
    ok t54
else
    bad t54 "rc=$RESTORE_RC"
fi

# ---------------- t55 首次 restore 时 C 为 symlink → rc=2 保留现场
eval "$(mkfix t55)"
mkdir -p "$TMP/t55/vendor"
ln -s /tmp/victim "$TMP/t55/vendor/fantgpu"
run_restore "$TMP/t55/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Fq "not a real directory" "$TMP/r.err" \
   && [ -L "$TMP/t55/vendor/fantgpu" ]; then
    ok t55
else
    bad t55 "rc=$RESTORE_RC"
fi

# ---------------- t56 首次 restore 时 C 为普通文件 → rc=2 保留现场
eval "$(mkfix t56)"
mkdir -p "$TMP/t56/vendor"
printf 'junk\n' > "$TMP/t56/vendor/fantgpu"
run_restore "$TMP/t56/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Fq "not a real directory" "$TMP/r.err" \
   && [ -f "$TMP/t56/vendor/fantgpu" ]; then
    ok t56
else
    bad t56 "rc=$RESTORE_RC"
fi

# ---------------- t57 symlink target 绝对路径 → rc=2（载荷根边界）
eval "$(mkfix t57 '{"link_target":"/tmp/victim"}')"
mkdir -p "$TMP/t57/vendor"
run_restore "$TMP/t57/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Fq "symlink target is absolute" "$TMP/r.err" \
   && [ ! -e "$TMP/t57/vendor/fantgpu" ]; then
    ok t57
else
    bad t57 "rc=$RESTORE_RC"
fi

# ---------------- t58 symlink target ../ 逃逸（解析后出根）→ rc=2
eval "$(mkfix t58 '{"link_target":"../../escape.txt"}')"
mkdir -p "$TMP/t58/vendor"
run_restore "$TMP/t58/vendor/fantgpu"
if [ "$RESTORE_RC" -eq 2 ] \
   && grep -Eq "symlink target is absolute|escapes payload root" "$TMP/r.err" \
   && [ ! -e "$TMP/t58/vendor/fantgpu" ]; then
    ok t58
else
    bad t58 "rc=$RESTORE_RC"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
