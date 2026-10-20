#!/bin/bash
# Exercise the real documentation gate against an isolated working-tree copy.
set -euo pipefail
export LC_ALL=C

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
fixture=$(mktemp -d)
trap '[[ -f $fixture/matrix.before ]] && cp "$fixture/matrix.before" "$fixture/docs/patches/system-kernel-matrix.json" || true; rm -rf "$fixture"' EXIT

# Include pending additions/renames without staging anything in the source repo.
cd "$ROOT"
while IFS= read -r -d '' path; do
    [[ -e "$path" || -L "$path" ]] || continue
    cp -a --parents -- "$path" "$fixture/"
done < <(git ls-files --cached --others --exclude-standard -z)
cd "$fixture"
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE INNOGPU_ROOT
git init -q
git add -f --all

checks=0
check() {
    local expected=$1 marker=$2 rc=0
    bash tools/check-docs.sh > "$fixture/check.log" 2>&1 || rc=$?
    if [[ "$rc" != "$expected" ]] || ! grep -Fq "$marker" "$fixture/check.log"; then
        cat "$fixture/check.log"
        printf 'check_docs_test=FAIL expected_rc=%s actual_rc=%s marker=%s\n' "$expected" "$rc" "$marker"
        exit 1
    fi
    checks=$((checks + 1))
    printf 'check_docs_t%s=PASS\n' "$checks"
}

# Historical references remain allowed at both migrated destinations.
printf '\nscripts/%s.sh\n' xdisplay >> docs/history/history.md
printf '\nscripts/%s.sh\n' xdisplay >> docs/history/display-integration.md
check 0 'RESULT: PASS_DOCS'

cp docs/state/current-work.md "$fixture/current-work.before"
printf '\ntools/%s.sh\n' xdisplay >> docs/state/current-work.md
check 1 'current documentation references a removed xdisplay implementation'
cp "$fixture/current-work.before" docs/state/current-work.md

cp docs/history/todo.md "$fixture/todo.before"
printf '\n- [ ] fixture active task\n' >> docs/history/todo.md
check 1 'completed-work timeline contains an active task'
cp "$fixture/todo.before" docs/history/todo.md

sed -i -E 's/[0-9]+ PASS \/ [0-9]+ SKIP \/ [0-9]+ UNVERIFIED/fixture missing runtime summary/g' docs/state/current-work.md
check 1 'docs/state/current-work.md is missing required current-state text'
cp "$fixture/current-work.before" docs/state/current-work.md

for path in docs/design/source-tree-migration.md docs/design/phase5-retirement-design.md; do
    cp "$path" "$fixture/design.before"
    printf '\n18/9/8\n' >> "$path"
    check 1 'current documentation contains a stale architecture, capability, or runtime assertion'
    cp "$fixture/design.before" "$path"
done

for path in docs/history/display-integration.md docs/design/source-tree-migration.md docs/design/phase5-retirement-design.md; do
    mv "$path" "$fixture/missing-document"
    check 1 "required document is missing: $path"
    mv "$fixture/missing-document" "$path"
done

cp docs/patches/system-kernel-matrix.json "$fixture/matrix.before"
python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
data["current"][0]["install"].append("tools/debian/missing-matrix-fixture.sh")
path.write_text(json.dumps(data))
PY
check 1 'system-kernel matrix'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
gentoo = next(row for row in data["current"] if row["system"] == "gentoo")
del gentoo["linux_patches"]["006-vmap-pfn-prompt"]
path.write_text(json.dumps(data))
PY
check 1 'linux_patches must match gentoo'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
debian = next(row for row in data["current"] if row["system"] == "debian")
debian["linux_patches"]["005-drm-display-hdmi-helper-prompt"] = {"state": "applied", "sha256": "unused"}
path.write_text(json.dumps(data))
PY
check 1 'linux_patches must match debian'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

cp tools/internal/derive-fantgpu-source.sh "$fixture/derive.before"
cp tools/gentoo/fh2m-lib.sh "$fixture/lib.before"

write_derive() {
    local extra="$1"
    local tree="$2"
    cat > tools/internal/derive-fantgpu-source.sh << EOF
need_repo() {
    [[ -f \$ROOT/patches/m-099-not-applied.patch ]] || true
}
patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "\$1" -p1 \\
    < "\$ROOT/patches/m-035-private-accessor.patch"
${extra}
EOF
    if [[ -n "$tree" ]]; then
        printf 'expected = "%s"\n' "$tree" >> tools/internal/derive-fantgpu-source.sh
    fi
}
write_cmd() {
    local extra="$1"
    cat > tools/gentoo/fh2m-lib.sh << EOF
need_repo() {
    [[ -f \$ROOT/patches/m-099-not-applied.patch ]] || true
}
cmd_build() {
    [[ -f \$ROOT/patches/m-098-not-applied.patch ]] || true
${extra}}
EOF
}
base_cmd='    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/m-036-bf-shf-rename.patch"
    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/m-051-hwinfo-connector-fallback.patch"
'
write_derive '' '9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee'
write_cmd "$base_cmd"
check 0 'RESULT: PASS_DOCS'

write_cmd '    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/m-036-bf-shf-rename.patch"
'
check 1 'missing=m-051-hwinfo-connector-fallback'
write_cmd "$base_cmd"

python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
gentoo = next(row for row in data["current"] if row["system"] == "gentoo")
for item in gentoo["m_base"]:
    if item["id"] == "m-036-bf-shf-rename":
        item["in_i12_builder"] = True
path.write_text(json.dumps(data))
PY
check 1 'flag=m-036-bf-shf-rename'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
debian = next(row for row in data["current"] if row["system"] == "debian")
for item in debian["m_base"]:
    if item["id"] == "i12-builder-through-m-035":
        item["in_i12_builder"] = False
path.write_text(json.dumps(data))
PY
check 1 'missing=i12-builder-through-m-035'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

write_derive 'patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$1" -p1 \
    < "$ROOT/patches/m-036-bf-shf-rename.patch"
' '9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee'
check 1 'debian 6.12.111+deb13-amd64 builder missing=- extra=- flag=m-036-bf-shf-rename'
write_derive '' '9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee'

python3 - << 'PY'
import json
from pathlib import Path
path = Path("docs/patches/system-kernel-matrix.json")
data = json.loads(path.read_text())
gentoo = next(row for row in data["current"] if row["system"] == "gentoo")
gentoo["g_set"] = ["g-001-not-built"]
path.write_text(json.dumps(data))
PY
check 1 'extra=g-001-not-built'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json

write_derive '' ''
check 1 'tree=missing'

write_derive '' '9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee'
g_cmd="$base_cmd"'    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/g-037-module-import-ns.patch"
'
write_cmd "$g_cmd"
python3 -c 'import json; from pathlib import Path; p=Path("docs/patches/system-kernel-matrix.json"); d=json.loads(p.read_text()); g=next(r for r in d["current"] if r["system"]=="gentoo"); g["g_set"]=["g-037-module-import-ns"]; p.write_text(json.dumps(d))'
check 0 'RESULT: PASS_DOCS'
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json
write_cmd "$base_cmd"
python3 -c 'import json; from pathlib import Path; p=Path("docs/patches/system-kernel-matrix.json"); d=json.loads(p.read_text()); row=next(r for r in d["current"] if r["system"]=="debian"); row["install"]=[i for i in row["install"] if i!="tools/debian/build-driver.sh"]; p.write_text(json.dumps(d))'
printf '%s\n' 'need_repo() { [[ -f $ROOT/patches/m-099-not-applied.patch ]] || true; }' 'expected = "9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee"' > tools/internal/derive-fantgpu-source.sh
check 1 'gentoo 6.12.111-gentoo-x86_64 builder missing=m-035-private-accessor'
if grep -q 'debian 6.12.111+deb13-amd64 builder' "$fixture/check.log"; then
    echo "gentoo assertion still depended on the debian builder check" >&2
    exit 1
fi
cp "$fixture/matrix.before" docs/patches/system-kernel-matrix.json
cp "$fixture/derive.before" tools/internal/derive-fantgpu-source.sh
cp "$fixture/lib.before" tools/gentoo/fh2m-lib.sh

printf 'check_docs_total=%s check_docs_failed=0\n' "$checks"
