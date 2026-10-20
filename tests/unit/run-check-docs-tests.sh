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

printf 'check_docs_total=%s check_docs_failed=0\n' "$checks"
