#!/usr/bin/env python3
"""R16 per-file mapping gate (P5 · 2026-09-05 · qoder, 11th-round hardening).

Asserts (each failure is fatal — sys.exit(1) — with non-zero exit and a
specific FAIL line printed to stderr; a misleading PASS is never produced):

  G0 — production cardinality invariant (fail-closed):
       - differs count == 432 (not "any count summing to 435")
       - F-only count == 3
       - differs+F-only total == 435
       - no duplicate canonical paths (raw-row Counter)
       - all manifest statuses ∈ VALID_STATUSES (no unknown status)
       Any violation rejects the input before any other gate runs.
  G1 — canonical path → BC mapping covers exactly the 432 differs + 3 F-only
       paths in P2 manifest. Unassigned, extra, missing, count drift all FAIL.
  G2 — BC uniqueness: each canonical path maps to exactly one BC. Real
       duplicate detection via Counter on the parsed stream (not the broken
       `len(assigned) - len(set(assigned))`); any path appearing twice fails.
  G3 — BC coverage: the set of BC labels in canon-to-bc.tsv must equal the
       23-BC required set {BC-01..BC-21, BC-22a, BC-22b}. Missing or extra BCs
       both FAIL.
  G4 — per-file disposition coverage: every differs/F-only path appears in
       per-file-classification.tsv with its BC tag identical to canon-to-bc.tsv
       (consistency check). Unassigned/extra/missing/tampered all FAIL.
  G5 — per-file schema: disposition ∈ {drop, defer, selected}; classification
       ∈ {PURE_RENAME, BEHAVIORAL, F-ONLY, MISSING}; license_d / license_f
       ∈ LICENSE_WHITELIST. Unknown values FAIL. Total must equal 435.
  G6 — fail-closed discipline (full table, not just BEHAVIORAL→drop):
       - BEHAVIORAL + disposition=drop    → FAIL
       - PURE_RENAME + disposition=defer  → FAIL
       - F-ONLY + disposition=defer       → FAIL (only F-ONLY with notes may defer)
       - F-ONLY + disposition=selected    → FAIL
       - MISSING + disposition=drop       → FAIL (defensive default)

Reads:
  build/p2-manifest.tsv                          (P2 manifest)
  build/r16-evidence/canon-to-bc.tsv             (built by r16-build-bc-map.py)
  build/r16-evidence/per-file-classification.tsv (built by r16-classify.py)

Exit codes:
  0 — all gates PASS
  1 — gate failure (printed to stderr; gate-level summary printed to stdout)
  2 — usage / input missing
"""
import csv
import hashlib
import os
import sys
from collections import Counter, defaultdict

MANIFEST = os.environ.get("R16_MANIFEST", "build/p2-manifest.tsv")
BC_MAP = os.environ.get("R16_BC_MAP", "build/r16-evidence/canon-to-bc.tsv")
PER_FILE = os.environ.get("R16_PER_FILE", "build/r16-evidence/per-file-classification.tsv")

REQUIRED_BCS = {
    'BC-01', 'BC-02', 'BC-03', 'BC-04', 'BC-05', 'BC-06', 'BC-07',
    'BC-08', 'BC-09', 'BC-10', 'BC-11', 'BC-12', 'BC-13', 'BC-14',
    'BC-15', 'BC-16', 'BC-17', 'BC-18', 'BC-19', 'BC-20', 'BC-21',
    'BC-22a', 'BC-22b',
}

# Production cardinality invariants: 432 differs + 3 F-only = 435 paths.
# Gate fails closed if input has a different cardinality, regardless of which
# downstream generator or fixture produced it.
REQUIRED_PATHS = 435
REQUIRED_DIFFERS = 432
REQUIRED_FONLY = 3
VALID_STATUSES = {'differs', 'identical', 'D-only', 'F-only', 'F-only-deferred'}

VALID_DISPOSITIONS = {'drop', 'defer', 'selected'}
# MISSING is intentionally NOT in VALID_CLASSIFICATIONS: a MISSING row means
# the generator could not read D or F content, so it must be treated as
# fail-closed (defer with explicit unassigned marker), not silently passed.
VALID_CLASSIFICATIONS = {'PURE_RENAME', 'BEHAVIORAL', 'F-ONLY'}
LICENSE_WHITELIST = {
    'mit-or-gpl-2.0-only',
    'mit-only',
    'bsd-3-clause-or-lgpl-2.1-only',
    'strictly-confidential',
    'unclassified',
    # F-only rows have no D-side path; the D license is recorded as F-ONLY.
    'F-ONLY',
}


def fatal(msg, code=1):
    print(f"FATAL: {msg}", file=sys.stderr)
    sys.exit(code)


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def load_manifest():
    """Return (status_map, raw_rows, manifest_duplicates, unknown_statuses).

    status_map: {path: status} for all paths in P2 manifest (last-wins).
    raw_rows: count of raw data rows (excluding header).
    manifest_duplicates: list of (path, count) for paths appearing more than
        once in raw rows — fail-closed at this gate layer too.
    unknown_statuses: list of status strings not in VALID_STATUSES — fail-closed.
    """
    status = {}
    raw_count = 0
    canon_counter = Counter()
    seen_statuses = set()
    unknown_statuses = []
    with open(MANIFEST) as f:
        next(f)
        for line in f:
            cols = line.rstrip('\n').split('\t')
            if len(cols) < 6:
                fatal(f"manifest row has < 6 columns: {line!r}")
            if cols[5] not in VALID_STATUSES:
                if cols[5] not in seen_statuses:
                    unknown_statuses.append(cols[5])
                    seen_statuses.add(cols[5])
            status[cols[0]] = cols[5]
            canon_counter[cols[0]] += 1
            raw_count += 1
    manifest_duplicates = sorted(
        [(p, n) for p, n in canon_counter.items() if n > 1])
    return status, raw_count, manifest_duplicates, unknown_statuses


def load_bc_map():
    """Return {path: bc_id}. Detect duplicate path rows."""
    m = {}
    seen_paths = []
    with open(BC_MAP) as f:
        next(f)
        for line in f:
            cols = line.rstrip('\n').split('\t')
            if len(cols) != 2:
                fatal(f"canon-to-bc.tsv row has != 2 columns: {line!r}")
            p, bc = cols
            if bc not in REQUIRED_BCS:
                fatal(f"canon-to-bc.tsv has unknown BC label '{bc}' for {p}")
            if p in m:
                seen_paths.append((p, m[p], bc))
            m[p] = bc
    return m, seen_paths


def load_per_file():
    """Return list of dict rows from per-file-classification.tsv.

    Also return the raw header to detect tampering.
    """
    rows = []
    with open(PER_FILE) as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            rows.append(row)
    return rows


def main():
    # --- Input presence ---
    for path in (MANIFEST, BC_MAP, PER_FILE):
        if not os.path.isfile(path):
            fatal(f"required input missing: {path}", code=2)

    failures = []

    # --- Load ---
    status, raw_rows, manifest_dups, unknown_statuses = load_manifest()
    bc_map, dup_in_bc_map = load_bc_map()
    per_file = load_per_file()

    # --- G0: Production cardinality invariant (fail-closed) ---
    # Gate must require EXACTLY 432 differs + 3 F-only = 435 paths, not just
    # "the input sums to 435". An under/over-sized input in any of the three
    # sub-counts (raw rows / differs / F-only) all fail. Also rejects any
    # unknown status string in the manifest.
    differs_count = sum(1 for s in status.values() if s == 'differs')
    fonly_count = sum(1 for s in status.values() if s == 'F-only')
    differs_fonly_count = differs_count + fonly_count

    print(f"=== Gate 0: production cardinality ===")
    print(f"  raw manifest rows     = {raw_rows}")
    print(f"  differs paths         = {differs_count} (required {REQUIRED_DIFFERS})")
    print(f"  F-only paths          = {fonly_count} (required {REQUIRED_FONLY})")
    print(f"  differs+F-only total  = {differs_fonly_count} (required {REQUIRED_PATHS})")
    if differs_count != REQUIRED_DIFFERS:
        failures.append(f"G0: differs={differs_count} != {REQUIRED_DIFFERS} "
                        f"(must be exactly 432)")
    if fonly_count != REQUIRED_FONLY:
        failures.append(f"G0: F-only={fonly_count} != {REQUIRED_FONLY} "
                        f"(must be exactly 3)")
    if differs_fonly_count != REQUIRED_PATHS:
        failures.append(f"G0: differs+F-only={differs_fonly_count} != "
                        f"{REQUIRED_PATHS} (refusing to validate)")
    if manifest_dups:
        failures.append(f"G0: {len(manifest_dups)} duplicate canonical paths "
                        f"in manifest (fail-closed)")
        for p, n in manifest_dups[:10]:
            print(f"    {p}: {n}x")
    if unknown_statuses:
        failures.append(f"G0: {len(unknown_statuses)} unknown manifest status(es) "
                        f"in {sorted(unknown_statuses)} (fail-closed; valid: "
                        f"{sorted(VALID_STATUSES)})")

    # --- G1: BC mapping covers exactly differs+F-only ---
    differs_fonly = {p for p, s in status.items() if s in ('differs', 'F-only')}
    g1_assigned = set(bc_map.keys())
    g1_unassigned = differs_fonly - g1_assigned
    g1_extra = g1_assigned - differs_fonly
    print(f"\n=== Gate 1: canonical path → BC mapping ===")
    print(f"  assigned = {len(g1_assigned)}")
    print(f"  expected = {len(differs_fonly)} ({differs_count} differs + "
          f"{fonly_count} F-only)")
    if len(g1_assigned) != len(differs_fonly):
        failures.append(f"G1: assigned != expected ({len(g1_assigned)} vs {len(differs_fonly)})")
    if g1_unassigned:
        failures.append(f"G1: {len(g1_unassigned)} unassigned paths")
        for p in sorted(g1_unassigned)[:10]:
            print(f"    unassigned: {p}")
    if g1_extra:
        failures.append(f"G1: {len(g1_extra)} extra paths in canon-to-bc.tsv")
        for p in sorted(g1_extra)[:10]:
            print(f"    extra: {p}")
    if dup_in_bc_map:
        failures.append(f"G1: {len(dup_in_bc_map)} duplicate path rows in canon-to-bc.tsv")
        for p, b1, b2 in dup_in_bc_map[:5]:
            print(f"    {p}: {b1} vs {b2}")

    # --- G2: BC uniqueness (real duplicate detection via per-line parse) ---
    path_to_bcs = defaultdict(list)
    with open(BC_MAP) as f:
        next(f)
        for line in f:
            p, bc = line.rstrip('\n').split('\t')
            path_to_bcs[p].append(bc)
    dup_paths = {p: bcs for p, bcs in path_to_bcs.items() if len(bcs) > 1}
    print(f"\n=== Gate 2: BC mapping uniqueness ===")
    print(f"  total entries = {sum(len(bcs) for bcs in path_to_bcs.values())}")
    print(f"  unique paths  = {len(path_to_bcs)}")
    print(f"  duplicates (paths in multiple BCs) = {len(dup_paths)}")
    if dup_paths:
        failures.append(f"G2: {len(dup_paths)} paths mapped to multiple BCs")
        for p, bcs in sorted(dup_paths.items())[:5]:
            print(f"    {p} → {bcs}")

    # --- G3: BC coverage ---
    bcs_used = set(bc_map.values())
    missing_bcs = REQUIRED_BCS - bcs_used
    extra_bcs = bcs_used - REQUIRED_BCS
    print(f"\n=== Gate 3: BC coverage ===")
    print(f"  BCs used = {len(bcs_used)} ({sorted(bcs_used)})")
    if missing_bcs:
        failures.append(f"G3: missing BCs: {sorted(missing_bcs)}")
    if extra_bcs:
        failures.append(f"G3: extra BCs: {sorted(extra_bcs)}")

    # --- G4: per-file coverage + BC consistency with canon-to-bc.tsv ---
    pf_paths = {r['canon_path'] for r in per_file}
    pf_unassigned = differs_fonly - pf_paths
    pf_extra = pf_paths - differs_fonly
    bc_mismatch = []
    for row in per_file:
        cp = row['canon_path']
        bc_pf = row.get('bc', '')
        bc_map_v = bc_map.get(cp, 'UNASSIGNED')
        if bc_pf != bc_map_v:
            bc_mismatch.append((cp, bc_pf, bc_map_v))
    print(f"\n=== Gate 4: per-file disposition coverage ===")
    print(f"  per-file paths = {len(pf_paths)}")
    print(f"  expected = {REQUIRED_PATHS}")
    print(f"  bc consistency (per-file vs canon-to-bc.tsv) = "
          f"{'PASS' if not bc_mismatch else 'FAIL'}")
    if len(pf_paths) != REQUIRED_PATHS:
        failures.append(f"G4: per-file != {REQUIRED_PATHS} ({len(pf_paths)} vs {REQUIRED_PATHS})")
    if pf_unassigned:
        failures.append(f"G4: per-file unassigned: {len(pf_unassigned)}")
    if pf_extra:
        failures.append(f"G4: per-file extra: {len(pf_extra)}")
    if bc_mismatch:
        failures.append(f"G4: {len(bc_mismatch)} per-file BC tags differ from canon-to-bc.tsv")
        for cp, b1, b2 in bc_mismatch[:5]:
            print(f"    {cp}: per-file={b1} vs map={b2}")

    # --- G5: per-file schema validation ---
    bad_disps = []
    bad_classifications = []
    bad_license_d = []
    bad_license_f = []
    for r in per_file:
        d = r.get('disposition', '')
        c = r.get('classification', '')
        ld = r.get('license_d', '')
        lf = r.get('license_f', '')
        if d not in VALID_DISPOSITIONS:
            bad_disps.append((r['canon_path'], d))
        if c not in VALID_CLASSIFICATIONS:
            bad_classifications.append((r['canon_path'], c))
        if ld not in LICENSE_WHITELIST:
            bad_license_d.append((r['canon_path'], ld))
        if lf not in LICENSE_WHITELIST:
            bad_license_f.append((r['canon_path'], lf))
    disp_counts = Counter(r['disposition'] for r in per_file)
    cls_counts = Counter(r['classification'] for r in per_file)
    print(f"\n=== Gate 5: per-file schema validation ===")
    print(f"  dispositions = {dict(disp_counts)}, sum = {sum(disp_counts.values())}")
    print(f"  classifications = {dict(cls_counts)}")
    print(f"  bad_dispositions = {len(bad_disps)}")
    print(f"  bad_classifications = {len(bad_classifications)}")
    print(f"  bad_license_d = {len(bad_license_d)}")
    print(f"  bad_license_f = {len(bad_license_f)}")
    if bad_disps:
        failures.append(f"G5: {len(bad_disps)} invalid dispositions (not in {sorted(VALID_DISPOSITIONS)})")
        for cp, d in bad_disps[:5]:
            print(f"    {cp}: disposition='{d}'")
    if bad_classifications:
        failures.append(f"G5: {len(bad_classifications)} invalid classifications")
        for cp, c in bad_classifications[:5]:
            print(f"    {cp}: classification='{c}'")
    if bad_license_d:
        failures.append(f"G5: {len(bad_license_d)} invalid license_d values")
        for cp, l in bad_license_d[:5]:
            print(f"    {cp}: license_d='{l}'")
    if bad_license_f:
        failures.append(f"G5: {len(bad_license_f)} invalid license_f values")
        for cp, l in bad_license_f[:5]:
            print(f"    {cp}: license_f='{l}'")
    total_disp = sum(disp_counts.values())
    if total_disp != REQUIRED_PATHS:
        failures.append(f"G5: disposition sum != {REQUIRED_PATHS} "
                        f"({total_disp} vs {REQUIRED_PATHS})")

    # --- G6: fail-closed discipline (full table) ---
    g6_violations = []
    for r in per_file:
        c = r['classification']
        d = r['disposition']
        cp = r['canon_path']
        if c == 'BEHAVIORAL' and d == 'drop':
            g6_violations.append((cp, 'BEHAVIORAL+drop (must fail-closed to defer)'))
        elif c == 'PURE_RENAME' and d == 'defer':
            g6_violations.append((cp, 'PURE_RENAME+defer (should drop, not defer)'))
        elif c == 'F-ONLY' and d == 'defer':
            g6_violations.append((cp, 'F-ONLY+defer (default drop; defer requires per-BC notes)'))
        elif c == 'F-ONLY' and d == 'selected':
            g6_violations.append((cp, 'F-ONLY+selected (no D-side candidate; cannot port)'))
        elif c == 'MISSING' and d == 'drop':
            g6_violations.append((cp, 'MISSING+drop (defensive default should defer)'))
        elif c == 'MISSING':
            g6_violations.append((cp, f'MISSING+{d} (MISSING classification must not appear in per-file output)'))
    print(f"\n=== Gate 6: fail-closed discipline (full table) ===")
    print(f"  violations = {len(g6_violations)}")
    if g6_violations:
        failures.append(f"G6: {len(g6_violations)} fail-closed violations")
        for cp, msg in g6_violations[:5]:
            print(f"    {cp}: {msg}")

    # --- Summary ---
    print(f"\n=== Summary ===")
    if not failures:
        print("ALL GATES PASS")
        print(f"  Gate 0: cardinality={len(differs_fonly)}/{REQUIRED_PATHS}, "
              f"manifest_dups={len(manifest_dups)} ✓")
        print(f"  Gate 1: assigned={len(g1_assigned)}, "
              f"unassigned={len(g1_unassigned)}, extra={len(g1_extra)} ✓")
        print(f"  Gate 2: duplicates={len(dup_paths)} ✓")
        print(f"  Gate 3: BC coverage={len(bcs_used)}/23 ✓")
        print(f"  Gate 4: per-file={len(pf_paths)}, bc_mismatch={len(bc_mismatch)} ✓")
        print(f"  Gate 5: dispositions={dict(disp_counts)}, sum={total_disp} ✓")
        print(f"  Gate 6: fail-closed={len(g6_violations)} violations ✓")
        return 0

    print(f"FAIL ({len(failures)} gate violations):", file=sys.stderr)
    # Also echo to stdout so test fixtures can grep assertion patterns like
    # "BC tags differ" / "fail-closed violations" without parsing stderr.
    print(f"FAIL ({len(failures)} gate violations):")
    for f in failures:
        print(f"  - {f}", file=sys.stderr)
        print(f"  - {f}")
    return 1


if __name__ == '__main__':
    sys.exit(main())