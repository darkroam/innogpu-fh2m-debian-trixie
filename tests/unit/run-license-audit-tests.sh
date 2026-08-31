#!/bin/bash
# Unit tests for the three-layer license model: mechanical audit checks,
# artifact allowlists, commit-based archive building, and release gating.
#
# Negative cases: root GPLv3 scope overreach, missing upstream MIT notice,
# project-tools/driver-source allowlist leaks (drivers/confidential/vendor/deb/
# no-license), vendor-binary/NOASSERTION treated as a license, malformed or
# disallowed SPDX, dirty-tree release, allowlist traversal/duplicate/symlink,
# archive content not from HEAD.
# Positive cases: project-tools builds from a clean commit, upstream MIT ships
# with its notice, driver-source with only explicit-license files passes, audit
# runs without local debs/vendor, audit passes with a read-only .git.

set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
AUDITOR="$ROOT/tools/audit-licenses.py"
BUILDER="$ROOT/tools/build-release-archive.py"
TMP="$(mktemp -d /tmp/innogpu-license-tests.XXXXXX)"
WORK_REL=".build/license-audit-tests.$$"
WORK="$ROOT/$WORK_REL"
trap 'rm -rf "$TMP" "$WORK"' EXIT
mkdir -p "$WORK"

tests=0
failures=0
pass() { tests=$((tests + 1)); printf 'license_audit_t%02d=PASS # %s\n' "$tests" "$1"; }
fail() { tests=$((tests + 1)); failures=$((failures + 1)); printf 'license_audit_t%02d=FAIL reason=%s\n' "$tests" "$1"; }

expect() {
    local label=$1 rc=$2 output=$3 pattern=$4
    local expected_rc=0
    if [[ $# -ge 5 ]]; then expected_rc=$5; fi
    if [[ "$rc" -eq "$expected_rc" ]] && grep -Fq "$pattern" "$output"; then
        pass "$label"
    else
        fail "$label:rc=$rc expected=$expected_rc missing=$pattern"
    fi
}

# make_fixture <dir> <source_text> [confidential]
# Creates a committed fixture repo with one drivers/ implementation file
# (classified from source_text), a project doc, license texts, manifest,
# authority doc, and a policy whose hashes/expected counts match the files.
make_fixture() {
    local dir=$1 source_text=$2
    local confidential=${3:-no}
    mkdir -p "$dir/drivers" "$dir/docs/project" "$dir/LICENSES"
    printf '%s\n' "$source_text" > "$dir/drivers/source.c"
    printf '%s\n' 'Project documentation for the fixture drivers tree.' > "$dir/drivers/README.md"
    printf '%s\n' 'GNU GENERAL PUBLIC LICENSE' 'Version 3, 29 June 2007' \
        'GPL-3.0-or-later applies to the original layer only.' \
        'license_scope=original-layer-only' \
        'It does not cover drivers/, upstream MIT content, or local payloads.' > "$dir/LICENSE"
    printf '%s\n' '# innogpu-fh2m fixture readme' > "$dir/README.md"
    printf '%s\n' 'MIT License' 'Copyright (c) 2026 Tim Hant' 'Permission is hereby granted...' \
        > "$dir/LICENSES/MIT.txt"
    printf '%s\n' 'GNU GENERAL PUBLIC LICENSE' 'Version 3, 29 June 2007' > "$dir/LICENSES/GPL-3.0-or-later.txt"
    printf '%s\n' 'GNU GENERAL PUBLIC LICENSE' 'Version 2, June 1991' > "$dir/LICENSES/GPL-2.0-only.txt"
    printf '%s\n' 'BSD 3-Clause License' > "$dir/LICENSES/BSD-3-Clause.txt"
    printf '%s\n' 'GNU LESSER GENERAL PUBLIC LICENSE' 'Version 2.1, February 1999' > "$dir/LICENSES/LGPL-2.1-only.txt"
    printf '%s\n' 'Mozilla Public License Version 2.0' 'MPL-2.0 fixture text' > "$dir/LICENSES/MPL-2.0.txt"
    printf '%s\n' '# License Texts' 'fixture copies of standard texts.' > "$dir/LICENSES/README.md"
    mkdir -p "$dir/patches" "$dir/debs" "$dir/components/fbterm" "$dir/components/picom" "$dir/scripts" "$dir/collab"
    printf '%s\n' 'diff -ruN hal_power.c Kbuild compat_kernel6.h inno_devfreq_gov.c ...' > "$dir/patches/001-test.patch"
    printf '%s\n' '# collab index (user-input content; must stay out of project-tools)' > "$dir/collab/INDEX.md"
    printf '%s\n' '# debs README (project doc inside the local payload dir)' > "$dir/debs/README.md"
    printf '%s\n' 'diff -ruN fbterm source (GPL-2.0-only derived)' > "$dir/components/fbterm/001-fbterm.patch"
    printf '%s\n' 'diff -ruN picom source (MPL-2.0 derived)' > "$dir/components/picom/001-picom.patch"
    printf '%s\n' '# picom configuration' > "$dir/components/picom/picom.conf"
    printf '%s\n' '#!/bin/sh' 'echo install' > "$dir/scripts/install.sh"
    printf '%s\n' '*.deb' '/debs/*' '!/debs/README.md' > "$dir/.gitignore"
    cat > "$dir/THIRD_PARTY_NOTICES.md" <<'EOF'
# Third-Party Notices

## Upstream fork (MIT)
Forked from https://github.com/timhant/innogpu-fh2m-debian-trixie
Copyright (c) 2026 Tim Hant

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## components
components/fbterm: derived patch, Copyright (C) 2008 dragchan, GPL-2.0-only (fbterm upstream)
components/picom: derived patch, picom commit 6d676824c457a933c52e3e92c5a1856466f90545,
Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>, SPDX-License-Identifier: MPL-2.0
EOF
    printf '%s\n' 'license_release_gate=BLOCKED' > "$dir/docs/project/licensing.md"
    printf '%s\n' '{"entries":[{"source_path":"payload.bin","kind":"kernel-black-box","license":"vendor-binary"}]}' \
        > "$dir/binary-manifest.json"
    python3 - "$dir" "$source_text" "$confidential" <<'PY'
import hashlib, json, re, sys
from pathlib import Path
root = Path(sys.argv[1])
text = sys.argv[2]
confidential = sys.argv[3] == "yes"

def sha(p):
    return hashlib.sha256((root / p).read_bytes()).hexdigest()

if "Strictly Confidential" in text:
    cls, spdx = "strict-confidential", "LicenseRef-Strictly-Confidential"
elif ("distributed under BSD 3 clause and LGPL2.1" in text
      and "SPDX License Identifier: BSD-3-Clause" in text
      and "SPDX License Identifier: LGPL-2.1-only" in text):
    cls, spdx = "bsd-lgpl-dual", "BSD-3-Clause OR LGPL-2.1-only"
elif "Dual MIT/GPLv2" in text and "GPL-COPYING" in text and "MIT-COPYING" in text:
    cls, spdx = "dual-mit-gpl", "MIT OR GPL-2.0-only"
else:
    m = re.search(r"SPDX-License-Identifier:\s*([^\s*]+)", text)
    if m:
        cls, spdx = "standard-spdx", m.group(1)
    else:
        cls, spdx = "unclassified", "NOASSERTION"

counts = {k: 0 for k in ("dual_mit_gpl", "strict_confidential", "bsd_lgpl_dual", "standard_spdx", "unclassified")}
counts[{"dual-mit-gpl": "dual_mit_gpl", "strict-confidential": "strict_confidential",
        "bsd-lgpl-dual": "bsd_lgpl_dual", "standard-spdx": "standard_spdx",
        "unclassified": "unclassified"}[cls]] = 1

classifications = {"bsd_lgpl_dual": []}
if confidential:
    classifications["strict_confidential"] = ["drivers/source.c"]
else:
    classifications["strict_confidential"] = []

module_license = {}
for m in re.finditer(r'MODULE_LICENSE\(\s*"([^"]+)"\s*\)', text):
    module_license["drivers/source.c"] = ",".join(dict.fromkeys(m.groups()))

license_texts = [
    {"path": "LICENSES/GPL-3.0-or-later.txt", "sha256": sha("LICENSES/GPL-3.0-or-later.txt"), "spdx": "GPL-3.0-or-later"},
    {"path": "LICENSES/MIT.txt", "sha256": sha("LICENSES/MIT.txt"), "spdx": "MIT"},
    {"path": "LICENSES/GPL-2.0-only.txt", "sha256": sha("LICENSES/GPL-2.0-only.txt"), "spdx": "GPL-2.0-only"},
    {"path": "LICENSES/BSD-3-Clause.txt", "sha256": sha("LICENSES/BSD-3-Clause.txt"), "spdx": "BSD-3-Clause"},
    {"path": "LICENSES/LGPL-2.1-only.txt", "sha256": sha("LICENSES/LGPL-2.1-only.txt"), "spdx": "LGPL-2.1-only"},
    {"path": "LICENSES/MPL-2.0.txt", "sha256": sha("LICENSES/MPL-2.0.txt"), "spdx": "MPL-2.0"},
]

policy = {
    "format_version": 2,
    "release_status": "BLOCKED",
    "layers": {
        "original": {"license": "GPL-3.0-or-later", "scope": "fixture original layer"},
        "upstream_mit": {"license": "MIT", "copyright": "Copyright (c) 2026 Tim Hant", "scope": "fixture upstream"},
        "drivers": {"license": "per-file", "scope": "fixture drivers"},
        "local_payloads": {"license": "not-distributed", "scope": "fixture payloads"},
    },
    "source_root": "drivers",
    "source_origin": "fixture",
    "project_owned_paths": ["drivers/README.md"],
    "classifications": classifications,
    "dual_mit_gpl": {"declaration": "Dual MIT/GPLv2",
                     "required_references": ["GPL-COPYING", "MIT-COPYING"],
                     "normalized_spdx": "MIT OR GPL-2.0-only"},
    "root_license": {"path": "LICENSE", "scope_marker": "license_scope=original-layer-only",
                     "required_markers": ["GNU GENERAL PUBLIC LICENSE", "Version 3", "GPL-3.0-or-later"]},
    "upstream_mit_notice": {"path": "THIRD_PARTY_NOTICES.md",
                            "required_text": ["Copyright (c) 2026 Tim Hant", "MIT License",
                                              "github.com/timhant/innogpu-fh2m-debian-trixie"]},
    "mit_license_text": {"path": "LICENSES/MIT.txt", "required_text": ["Copyright (c) 2026 Tim Hant"]},
    "required_license_texts": license_texts,
    "allowed_spdx_expressions": ["GPL-3.0-or-later", "MIT", "MIT OR GPL-2.0-only",
                                 "BSD-3-Clause OR LGPL-2.1-only", "GPL-2.0-only",
                                 "BSD-3-Clause", "LGPL-2.1-only", "MPL-2.0"],
    "module_license_metadata": module_license,
    "known_declaration_conflicts": [],
    "manifest": {"path": "binary-manifest.json", "unresolved_marker": "vendor-binary"},
    "license_authority_doc": {"path": "docs/project/licensing.md", "status_marker_prefix": "license_release_gate="},
    "non_drivers_licenses": {
        "original_roots": [".github/", "baselines/", "docs/", "scripts/", "tests/", "tools/"],
        "original": "GPL-3.0-or-later",
        "paths": {
            "LICENSE": "GPL-3.0-or-later",
            "LICENSES/README.md": "GPL-3.0-or-later",
            "THIRD_PARTY_NOTICES.md": "GPL-3.0-or-later",
            "binary-manifest.json": "GPL-3.0-or-later",
            "policy.json": "GPL-3.0-or-later",
            "inventory.tsv": "GPL-3.0-or-later",
            "LICENSES/MIT.txt": "MIT",
            "LICENSES/GPL-3.0-or-later.txt": "GPL-3.0-or-later",
            "LICENSES/GPL-2.0-only.txt": "GPL-2.0-only",
            "LICENSES/BSD-3-Clause.txt": "BSD-3-Clause",
            "LICENSES/LGPL-2.1-only.txt": "LGPL-2.1-only",
            "LICENSES/MPL-2.0.txt": "MPL-2.0",
            ".gitignore": "MIT",
            "README.md": "MIT",
            "scripts/install.sh": "MIT",
            "components/fbterm/001-fbterm.patch": "GPL-2.0-only",
            "components/picom/001-picom.patch": "MPL-2.0",
            "components/picom/picom.conf": "GPL-3.0-or-later",
        },
    },
    "notice_gate": {
        "path": "THIRD_PARTY_NOTICES.md",
        "exempt_expression": "GPL-3.0-or-later",
        "standard_text_root": "LICENSES/",
        "entries": [
            {"paths": [".gitignore", "README.md", "scripts/install.sh"],
             "markers": ["Copyright (c) 2026 Tim Hant", "MIT License"]},
            {"paths": ["components/fbterm/001-fbterm.patch"],
             "markers": ["fbterm", "dragchan", "GPL-2.0-only"]},
            {"paths": ["components/picom/001-picom.patch"],
             "markers": ["picom", "Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>", "MPL-2.0",
                         "6d676824c457a933c52e3e92c5a1856466f90545"]},
        ],
    },
    "artifacts": {
        "project-tools": {"status": "CLEARED",
                          "allowlist_file": "docs/project/project-tools-allowlist.txt",
                          "denylist": [r"^drivers/", r"^patches/", r"^debs/", r"^vendor/",
                                       r"^build/", r"^third_party/", r"^collab/",
                                       r"\.deb$", r"\.o_shipped$", r"\.o(\.cmd)?$", r"\.ko$",
                                       r"\.so(\.\d+)*$", r"\.fw$"],
                          "reason": "fixture"},
        "driver-source": {"status": "BLOCKED",
                          "allowlist_file": "docs/project/driver-source-allowlist.txt",
                          "denylist": [r"^vendor/", r"^build/", r"^third_party/", r"\.deb$",
                                       r"\.o_shipped$", r"\.o(\.cmd)?$", r"\.ko$",
                                       r"\.so(\.\d+)*$", r"\.fw$"],
                          "reason": "fixture"},
    },
    "expected_summary": {
        "tracked_paths": 2, "project_documents": 1, "implementation_paths": 1,
        "dual_mit_gpl": counts["dual_mit_gpl"], "strict_confidential": counts["strict_confidential"],
        "bsd_lgpl_dual": counts["bsd_lgpl_dual"], "standard_spdx": counts["standard_spdx"],
        "unclassified": counts["unclassified"], "module_license_files": len(module_license),
        "declaration_conflicts": 0,
    },
}
(root / "policy.json").write_text(json.dumps(policy, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PY
    # Placeholder inventory so the fail-closed path mapping (inventory.tsv is
    # an explicitly mapped root file) can be satisfied before phase 1 writes
    # the real inventory.
    printf '%s\n' '# placeholder inventory' > "$dir/inventory.tsv"
    git -C "$dir" init -q
    git -C "$dir" add -A
    git -C "$dir" -c user.email=t@t -c user.name=t commit -q -m f
    # Phase 1: generate inventory + allowlists, commit them.
    python3 "$AUDITOR" --root "$dir" --policy policy.json --inventory inventory.tsv \
        --write-inventory --write-allowlists >/dev/null 2>&1
    git -C "$dir" add -A
    git -C "$dir" -c user.email=t@t -c user.name=t commit -q -m allowlists
    # Phase 2: regenerate allowlists now that inventory/allowlist files are
    # tracked, so the project-tools allowlist covers the whole tracked set.
    python3 "$AUDITOR" --root "$dir" --policy policy.json --inventory inventory.tsv \
        --write-allowlists >/dev/null 2>&1
    git -C "$dir" add -A
    git -C "$dir" -c user.email=t@t -c user.name=t commit -q -m allowlists-final
}

mutate_fixture() {
    local dir=$1 mode=$2
    python3 - "$dir" "$mode" <<'PY'
import json, os, subprocess, sys
from pathlib import Path
root = Path(sys.argv[1])
mode = sys.argv[2]
policy_path = root / "policy.json"
policy = json.loads(policy_path.read_text(encoding="utf-8"))

def save_policy():
    policy_path.write_text(json.dumps(policy, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

if mode == "root-scope-overreach":
    lic = (root / "LICENSE").read_text(encoding="utf-8")
    (root / "LICENSE").write_text(lic.replace("license_scope=original-layer-only",
                                              "license_scope=full-repository"), encoding="utf-8")
elif mode == "notice-missing":
    n = (root / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
    (root / "THIRD_PARTY_NOTICES.md").write_text(n.replace("Copyright (c) 2026 Tim Hant",
                                                           "Copyright (c) Someone Else"), encoding="utf-8")
elif mode == "pt-leak-drivers":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "drivers/source.c\n", encoding="utf-8")
elif mode == "pt-leak-deb":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "debs/evil.deb\n", encoding="utf-8")
elif mode == "pt-leak-vendor-path":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "payload.bin\n", encoding="utf-8")
elif mode == "pt-leak-patches":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "patches/001-test.patch\n", encoding="utf-8")
elif mode == "pt-leak-collab":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "collab/INDEX.md\n", encoding="utf-8")
elif mode == "pt-leak-debs-dir":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "debs/README.md\n", encoding="utf-8")
elif mode == "notice-gate-missing":
    n = root / "THIRD_PARTY_NOTICES.md"
    n.write_text(n.read_text(encoding="utf-8").replace("Yuxuan Shui", "Yuxuan Shiu"), encoding="utf-8")
elif mode == "components-unclassified":
    (root / "components/new.patch").write_text("diff -ruN some component\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "new-component"], check=True)
elif mode == "unknown-path-fail-closed":
    # A third-party file in an unknown location: fail-closed model must reject
    # it even after a clean allowlist regeneration (no implicit default GPL).
    (root / "external").mkdir(exist_ok=True)
    (root / "external/foo.c").write_text("/* third-party */\nint foo;\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "external"], check=True)
elif mode == "third-party-no-entry":
    # A new third-party path mapped in policy but without a notice-gate entry:
    # the path-bound gate must reject it (an existing picom string must not
    # cover unrelated third-party paths).
    (root / "components/new2.patch").write_text("diff -ruN another component\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    policy["non_drivers_licenses"]["paths"]["components/new2.patch"] = "MIT"
    save_policy()
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "new2"], check=True)
elif mode == "licenses-unmapped":
    # LICENSES/ standard texts must be explicitly classified: removing a
    # mapping fails closed instead of silently defaulting to GPLv3.
    policy["non_drivers_licenses"]["paths"].pop("LICENSES/MIT.txt", None)
    save_policy()
elif mode == "ds-leak-confidential":
    aw = root / "docs/project/driver-source-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "drivers/source.c\n", encoding="utf-8")
elif mode == "ds-leak-unclassified":
    aw = root / "docs/project/driver-source-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "drivers/source.c\n", encoding="utf-8")
elif mode == "manifest-noassertion-license":
    (root / "binary-manifest.json").write_text(
        '{"entries":[{"source_path":"payload.bin","kind":"kernel-black-box","license":"NOASSERTION"}]}\n',
        encoding="utf-8")
elif mode == "manifest-not-allowed-spdx":
    (root / "binary-manifest.json").write_text(
        '{"entries":[{"source_path":"payload.bin","kind":"kernel-black-box","license":"GPL-3.0-only"}]}\n',
        encoding="utf-8")
elif mode == "manifest-missing-license":
    (root / "binary-manifest.json").write_text(
        '{"entries":[{"source_path":"payload.bin","kind":"kernel-black-box","license":""}]}\n',
        encoding="utf-8")
elif mode == "malformed-layer-spdx":
    policy["layers"]["original"]["license"] = "Totally-Made-Up-License"
    save_policy()
elif mode == "driver-source-gpl3":
    policy["layers"]["drivers"]["license"] = "GPL-3.0-or-later"
    save_policy()
elif mode == "allowlist-traversal":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "../evil\n", encoding="utf-8")
elif mode == "allowlist-dup":
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "LICENSE\n", encoding="utf-8")
elif mode == "allowlist-symlink":
    os.symlink("../../LICENSE", str(root / "docs/project/slink"))
    subprocess.run(["git", "-C", str(root), "add", "docs/project/slink"], check=True)
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "docs/project/slink\n", encoding="utf-8")
elif mode == "dirty-tree":
    # Mutate a non-drivers file so the mechanical audit still passes and the
    # release path fails precisely on the dirty-tree guard.
    with open(root / "README.md", "a", encoding="utf-8") as fh:
        fh.write("dirty\n")
elif mode == "archive-not-head":
    # A clean committed tree with an allowlist entry absent from HEAD: the
    # builder must refuse because the archive content would not come from the
    # target commit. (Committed so the dirty-tree guard does not mask the
    # absent-in-commit failure.)
    aw = root / "docs/project/project-tools-allowlist.txt"
    aw.write_text(aw.read_text(encoding="utf-8") + "missing/file.txt\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "not-head"], check=True)
elif mode == "module-metadata-drift":
    policy.get("module_license_metadata", {}).pop("drivers/source.c", None)
    save_policy()
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "drift"], check=True)
elif mode == "positive-driver-source":
    policy["artifacts"]["driver-source"]["status"] = "CLEARED"
    save_policy()
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
                    "commit", "-q", "-m", "cleared"], check=True)
elif mode == "authority-status-mismatch":
    (root / "docs/project/licensing.md").write_text("license_release_gate=CLEARED\n", encoding="utf-8")
PY
}

O="$TMP/output"

# ---- real-repo baseline (audit consistency + artifact gate) ----
python3 "$AUDITOR" --root "$ROOT" > "$O" 2>&1
rc=$?
expect current_audit_consistent "$rc" "$O" 'license_audit_overall=PASS'

python3 "$AUDITOR" --root "$ROOT" > "$O" 2>&1
rc=$?
expect current_classification_counts "$rc" "$O" 'license_unclassified=70'

F="$TMP/scoped-gate-states"
make_fixture "$F" '/* no license declaration */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
if [[ "$rc" -eq 0 ]] \
    && grep -Fq 'license_release_gate=BLOCKED' "$O" \
    && grep -Fq 'license_artifact_project-tools_gate=CLEARED' "$O"; then
    pass isolated_scoped_gate_states
else
    fail "isolated_scoped_gate_states:rc=$rc"
fi

python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/one.tsv" --write-inventory > "$O" 2>&1
rc1=$?
python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/two.tsv" --write-inventory >> "$O" 2>&1
rc2=$?
if [[ "$rc1" -eq 0 && "$rc2" -eq 0 ]] && cmp -s "$WORK/one.tsv" "$WORK/two.tsv"; then
    pass deterministic_inventory
else
    fail "deterministic_inventory:rc=$rc1/$rc2"
fi

cp "$ROOT/docs/project/source-license-inventory.tsv" "$WORK/stale.tsv"
printf '\n' >> "$WORK/stale.tsv"
python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/stale.tsv" > "$O" 2>&1
rc=$?
expect stale_inventory_rejected "$rc" "$O" 'reason=inventory_stale' 1

sed 's#LICENSES/GPL-3.0-or-later.txt#LICENSES/MISSING.txt#' "$ROOT/license-audit-policy.json" > "$WORK/missing-text-policy.json"
python3 "$AUDITOR" --root "$ROOT" --policy "$WORK_REL/missing-text-policy.json" \
    --inventory "$WORK_REL/missing-text.tsv" --write-inventory > "$O" 2>&1
rc=$?
expect missing_license_text_rejected "$rc" "$O" 'reason=required_license_text_missing:LICENSES/MISSING.txt' 1

# ---- negative: root GPLv3 scope overreach ----
F="$TMP/root-scope"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" root-scope-overreach
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect root_scope_overreach_rejected "$rc" "$O" 'reason=root_license_scope_invalid:LICENSE' 1

# ---- negative: upstream MIT notice missing ----
F="$TMP/notice-missing"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" notice-missing
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect upstream_mit_notice_missing_rejected "$rc" "$O" 'reason=upstream_mit_notice_marker_missing:' 1

# ---- negative: project-tools allowlist leaks ----
F="$TMP/pt-leak-drivers"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-drivers
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_drivers_leak_rejected "$rc" "$O" 'reason=allowlist_leak:project-tools:drivers/source.c' 1

F="$TMP/pt-leak-deb"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-deb
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_deb_leak_rejected "$rc" "$O" 'reason=allowlist_leak:project-tools:debs/evil.deb' 1

F="$TMP/pt-leak-vendor"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-vendor-path
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_vendor_path_rejected "$rc" "$O" 'reason=allowlist_untracked:project-tools:payload.bin' 1

# ---- negative: patches/ and debs/ whole-directory leaks (P1/P2) ----
F="$TMP/pt-leak-patches"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-patches
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_patches_leak_rejected "$rc" "$O" 'reason=allowlist_leak:project-tools:patches/001-test.patch' 1

F="$TMP/pt-leak-debs-dir"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-debs-dir
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_debs_dir_rejected "$rc" "$O" 'reason=allowlist_leak:project-tools:debs/README.md' 1

# ---- negative: collab/ (user-input collaboration records) never enters project-tools ----
F="$TMP/pt-leak-collab"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" pt-leak-collab
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect project_tools_collab_leak_rejected "$rc" "$O" 'reason=allowlist_leak:project-tools:collab/INDEX.md' 1

F="$TMP/positive-collab-excluded"
make_fixture "$F" '/* no license declaration */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
if [ "$rc" -eq 0 ] && ! grep -Fq '^collab/' "$F/docs/project/project-tools-allowlist.txt"; then
    pass project_tools_allowlist_excludes_collab
else
    fail "project_tools_allowlist_excludes_collab:rc=$rc"
fi

# ---- negative: NOTICE gate markers (path-bound entries) - P2 ----
F="$TMP/notice-gate"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" notice-gate-missing
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect notice_gate_marker_rejected "$rc" "$O" 'reason=notice_gate_marker_missing:components/picom/001-picom.patch:Copyright (c) Yuxuan Shui' 1

# ---- negative: components/ third-party content must be classified - P1 ----
F="$TMP/components-unclassified"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" components-unclassified
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-allowlists > "$O" 2>&1
rc=$?
expect components_unclassified_rejected "$rc" "$O" 'reason=non_drivers_unclassified:components/new.patch' 1

# ---- negative: unknown third-party path fails closed (no default GPL) - P1 ----
F="$TMP/unknown-path"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" unknown-path-fail-closed
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-allowlists > "$O" 2>&1
rc=$?
expect unknown_path_fail_closed "$rc" "$O" 'reason=non_drivers_unclassified:external/foo.c' 1

# ---- negative: new third-party path without notice-gate entry - P2 ----
F="$TMP/third-party-no-entry"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" third-party-no-entry
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-allowlists > "$O" 2>&1
rc=$?
expect third_party_notice_entry_required "$rc" "$O" 'reason=notice_gate_entry_missing:components/new2.patch' 1

# ---- negative: LICENSES/ standard texts must be explicitly classified - P1 ----
F="$TMP/licenses-unmapped"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" licenses-unmapped
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect licenses_text_fail_closed "$rc" "$O" 'reason=non_drivers_unclassified:LICENSES/MIT.txt' 1

# ---- negative: driver-source allowlist leaks (confidential / unclassified) ----
F="$TMP/ds-leak-confidential"
make_fixture "$F" '/* Strictly Confidential */' yes
mutate_fixture "$F" ds-leak-confidential
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect driver_source_confidential_leak_rejected "$rc" "$O" 'reason=driver_source_restricted_leak:drivers/source.c' 1

F="$TMP/ds-leak-unclassified"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" ds-leak-unclassified
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect driver_source_unclassified_leak_rejected "$rc" "$O" 'reason=driver_source_restricted_leak:drivers/source.c' 1

# ---- negative: vendor-binary / NOASSERTION / disallowed SPDX as license ----
F="$TMP/manifest-noassertion"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" manifest-noassertion-license
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect noassertion_license_rejected "$rc" "$O" 'reason=manifest_license_invalid:payload.bin:NOASSERTION' 1

F="$TMP/manifest-not-allowed"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" manifest-not-allowed-spdx
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect disallowed_spdx_rejected "$rc" "$O" 'reason=manifest_license_invalid:payload.bin:GPL-3.0-only' 1

F="$TMP/manifest-missing"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" manifest-missing-license
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect manifest_missing_license_rejected "$rc" "$O" 'reason=manifest_license_missing:0' 1

F="$TMP/malformed-spdx"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" malformed-layer-spdx
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect malformed_spdx_rejected "$rc" "$O" 'reason=original_layer_license_invalid:Totally-Made-Up-License' 1

F="$TMP/drivers-gpl3"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" driver-source-gpl3
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect drivers_must_not_be_gpl3 "$rc" "$O" 'reason=drivers_layer_must_not_be_gpl3' 1

# ---- negative: allowlist traversal / duplicate / symlink ----
F="$TMP/traversal"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" allowlist-traversal
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect allowlist_traversal_rejected "$rc" "$O" 'reason=unsafe_allowlist_path' 1

F="$TMP/duplicate"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" allowlist-dup
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect allowlist_duplicate_rejected "$rc" "$O" 'reason=allowlist_duplicate:project-tools:LICENSE' 1

F="$TMP/symlink"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" allowlist-symlink
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect allowlist_symlink_rejected "$rc" "$O" 'reason=allowlist_symlink:project-tools:docs/project/slink' 1

# ---- negative: dirty-tree release + archive not from HEAD ----
F="$TMP/dirty"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" dirty-tree
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --artifact project-tools --require-releasable > "$O" 2>&1
rc=$?
expect dirty_tree_release_rejected "$rc" "$O" 'reason=release_tree_not_clean' 2

python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools --draft \
    --out "$TMP/dirty.tar.gz" > "$O" 2>&1
rc=$?
expect dirty_tree_archive_rejected "$rc" "$O" 'reason=working_tree_dirty' 1

F="$TMP/not-head"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" archive-not-head
python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools --draft \
    --out "$TMP/not-head.tar.gz" > "$O" 2>&1
rc=$?
expect archive_not_head_rejected "$rc" "$O" 'reason=allowlist_absent_in_commit:missing/file.txt' 1

# ---- negative: authority doc status drift ----
F="$TMP/authority-drift"
make_fixture "$F" '/* no license declaration */'
mutate_fixture "$F" authority-status-mismatch
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect authority_status_drift_rejected "$rc" "$O" 'reason=authority_doc_status_mismatch:' 1

# ---- positive: project-tools from a clean commit ----
F="$TMP/positive-pt"
make_fixture "$F" '/* no license declaration */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect positive_project_tools_audit "$rc" "$O" 'license_artifact_project-tools_gate=CLEARED'

python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --artifact project-tools --require-releasable > "$O" 2>&1
rc=$?
expect positive_project_tools_release "$rc" "$O" 'license_release_readiness=PASS'

python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools \
    --out "$TMP/p1.tar.gz" > "$O" 2>&1
rc=$?
expect positive_project_tools_archive "$rc" "$O" 'archive_build=PASS'

python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools \
    --out "$TMP/p2.tar.gz" >> "$O" 2>&1
rc=$?
if [[ "$rc" -eq 0 ]] && cmp -s "$TMP/p1.tar.gz" "$TMP/p2.tar.gz"; then
    pass deterministic_archive
else
    fail "deterministic_archive:rc=$rc"
fi

# Upstream MIT content ships with its notice inside the archive.
tar -tzf "$TMP/p1.tar.gz" > "$TMP/p1.list" 2>/dev/null
if grep -q 'LICENSES/MIT.txt' "$TMP/p1.list" && grep -q 'THIRD_PARTY_NOTICES.md' "$TMP/p1.list" \
   && tar -xOzf "$TMP/p1.tar.gz" THIRD_PARTY_NOTICES.md 2>/dev/null | grep -Fq 'Copyright (c) 2026 Tim Hant' \
   && tar -xOzf "$TMP/p1.tar.gz" LICENSES/MIT.txt 2>/dev/null | grep -Fq 'Copyright (c) 2026 Tim Hant'; then
    pass upstream_mit_ships_with_notice
else
    fail "upstream_mit_ships_with_notice"
fi

# ---- positive: driver-source with only explicit-license files ----
F="$TMP/positive-ds"
make_fixture "$F" '/* Dual MIT/GPLv2 GPL-COPYING MIT-COPYING */'
mutate_fixture "$F" positive-driver-source
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect positive_driver_source_audit "$rc" "$O" 'license_artifact_driver-source_gate=CLEARED'

python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --artifact driver-source --require-releasable > "$O" 2>&1
rc=$?
expect positive_driver_source_release "$rc" "$O" 'license_release_readiness=PASS'

python3 "$BUILDER" --root "$F" --policy policy.json --artifact driver-source \
    --out "$TMP/p3.tar.gz" > "$O" 2>&1
rc=$?
expect positive_driver_source_archive "$rc" "$O" 'archive_build=PASS'

# ---- positive: audit runs without local deb/vendor payloads; read-only .git ----
F="$TMP/no-local"
make_fixture "$F" '/* no license declaration */'
if [[ -e "$F/vendor" || -e "$F/build" || -e "$F/third_party" ]]; then
    fail "fixture unexpectedly has local payload dirs"
else
    pass audit_runs_without_local_payloads
fi
# P1/P2: the generated project-tools allowlist excludes patches/ and the whole debs/ dir
if grep -Eq '^(patches/|debs/)' "$F/docs/project/project-tools-allowlist.txt"; then
    fail "project-tools allowlist contains excluded dirs"
else
    pass project_tools_excludes_patches_debs
fi

chmod -R a-w "$F/.git"
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
chmod -R u+w "$F/.git"
expect readonly_git_audit_passes "$rc" "$O" 'license_audit_overall=PASS'

# ---- draft mode never reports release PASS ----
F="$TMP/draft"
make_fixture "$F" '/* no license declaration */'
python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools --draft \
    --out "$TMP/draft.tar.gz" > "$O" 2>&1
rc=$?
if [[ "$rc" -eq 0 ]] && grep -Fq 'archive_draft=OK' "$O" && ! grep -Fq 'archive_build=PASS' "$O"; then
    pass draft_mode_no_release_pass
else
    fail "draft_mode_no_release_pass:rc=$rc"
fi

# ---- archive output inside repo refused + mode preservation ----
F="$TMP/out-inside"
make_fixture "$F" '/* no license declaration */'
python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools --draft \
    --out "$F/out.tar.gz" > "$O" 2>&1
rc=$?
expect archive_output_inside_repo_rejected "$rc" "$O" 'reason=output_inside_repo' 1

F="$TMP/mode"
make_fixture "$F" '/* no license declaration */'
chmod 755 "$F/docs/project/licensing.md"
git -C "$F" add -A
git -C "$F" -c user.email=t@t -c user.name=t commit -q -m mode
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-allowlists >/dev/null 2>&1
git -C "$F" add -A
git -C "$F" -c user.email=t@t -c user.name=t commit -q -m allowlists2
python3 "$BUILDER" --root "$F" --policy policy.json --artifact project-tools --draft \
    --out "$TMP/mode.tar.gz" > "$O" 2>&1
rc=$?
if [[ "$rc" -eq 0 ]] && tar -tvzf "$TMP/mode.tar.gz" | grep 'docs/project/licensing.md' | grep -q -- '-rwxr-xr-x'; then
    pass archive_mode_preserved
else
    fail "archive_mode_preserved:rc=$rc"
fi

# ---- negative: module metadata drift (MODULE_LICENSE is metadata, not license) ----
F="$TMP/module-metadata"
make_fixture "$F" 'MODULE_LICENSE("GPL");'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect module_metadata_recorded_passes "$rc" "$O" 'license_module_license_files=1'
mutate_fixture "$F" module-metadata-drift
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv > "$O" 2>&1
rc=$?
expect module_metadata_drift_rejected "$rc" "$O" 'reason=module_license_metadata_set_changed' 1

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' \
    "$tests" "$((tests - failures))" "$failures"
[[ "$failures" -eq 0 ]]
