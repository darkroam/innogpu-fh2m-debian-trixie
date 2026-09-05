#!/usr/bin/env python3
"""Build the canonical-path → BC mapping (P5 6th-round · qoder · 2026-09-04).

Reads the P2 manifest and emits build/r16-evidence/canon-to-bc.tsv
with each of the 435 differs/F-only canonical paths assigned to exactly
one of 23 BCs (BC-01..BC-21 + BC-22a + BC-22b).

Production cardinality invariant (fail-closed):
  - raw manifest rows: 432 differs + 3 F-only = 435 paths
  - assigned == 435 paths, exactly once each (no duplicate canonical paths)
  - input != 435 ⇒ abort without overwriting output

Priority order (top-down, first match wins):
  1. F-only: BC-22a (fant_stackprotector.{c,h}), BC-22b (common_ri_bridge.h)
  2. srvkm/include/:
     a. BC-08 — 4 fixed files (ftsrv.h, device.h, osfunc.h, ftx_fwif_km.h)
     b. BC-06 — paths under powervr/ or km/
     c. BC-02 — common_*_bridge.h
     d. BC-03 — ft_, ftsrv_, ftmodule.h, ftversion.h
     e. BC-04 — ftx* (must precede BC-05 to avoid fant* prefix)
     f. BC-05 — fant_* (must precede BC-07 to avoid pdp0_ misroute)
     g. BC-07 — pdp* (pdp0_, pdp_)
     h. BC-01 — default
  3. srvkm/ (not include/):
     a. BC-09 — dpu_*, pdp0_*, gpu_drm.c, g3_*, gen_g3_*
     b. BC-10 — ft_*.c
     c. BC-11 — audio_*.c
     d. BC-12 — event.c, trace_events.c, fant_trace.c, fant_pmr.c,
                 fant_physmem_dmabuf_helper.c, fant_ft_spec.c,
                 fant_debugfs.c, fant_apphint.c
  4. gpu/: BC-13 (hal.h, hal_*, kernel_compat.*, ion_lma_heap.c),
           BC-14 (fant_*), BC-15 (gpu.h, gpu_pci_drv.*, gpu_ion.h,
                                  fixup_pcie.h, fixup_alignment.h)
  5. dma/: BC-16
  6. vpu/: BC-18
  7. smmu/: BC-19
  8. pmbus/: BC-20
  9. power/: BC-17
 10. tools/gpu_info/: BC-21

Failure modes (fail-closed, all non-zero exit, no partial output):
  - manifest missing / malformed (col < 6)
  - manifest contains unknown statuses (not in VALID_STATUSES whitelist)
  - manifest contains duplicate canonical paths
  - empty differs/F-only set (no rows to classify)
  - assigned != 435 (under/over production cardinality)
  - classify() raises ValueError on unhandled path
  - atomic write failure

Exit codes:
  0 — success
  1 — failure (stderr FATAL)
"""
import csv
import locale
import os
import sys

locale.setlocale(locale.LC_ALL, 'C')

MANIFEST = os.environ.get("R16_MANIFEST", "build/p2-manifest.tsv")
OUT = os.environ.get("R16_BC_MAP_OUT", "build/r16-evidence/canon-to-bc.tsv")

BC_08_FILES = {
    "srvkm/include/ftsrv.h",
    "srvkm/include/device.h",
    "srvkm/include/osfunc.h",
    "srvkm/include/ftx_fwif_km.h",
}

BC_13_BASENAMES = {
    "hal.h", "hal_dma.c", "hal_dma_errors.h", "hal_power.c",
    "hal_interface.h", "hal_interface.c", "hal_bmc.h",
    "hal_bind_numa.c", "kernel_compat.h", "kernel_compat.c",
    "ion_lma_heap.c",
}

BC_15_BASENAMES = {
    "gpu.h", "gpu_pci_drv.c", "gpu_pci_drv.h", "gpu_ion.h",
    "fixup_pcie.h", "fixup_alignment.h",
}

BC_12_BASENAMES = {
    "event.c", "trace_events.c", "fant_trace.c", "fant_pmr.c",
    "fant_physmem_dmabuf_helper.c", "fant_ft_spec.c",
    "fant_debugfs.c", "fant_apphint.c",
}

BC_11_PREFIXES = ("audio_",)
BC_10_PREFIXES = ("ft_",)
BC_09_PREFIXES = ("dpu_", "pdp0_", "g3_", "gen_g3_")
BC_09_BASENAMES = {"gpu_drm.c"}
BC_03_PREFIXES = ("ft_", "ftsrv_")
BC_03_BASENAMES = {"ftmodule.h", "ftversion.h"}
BC_05_PREFIXES = ("fant_",)
BC_07_PREFIXES = ("pdp0_", "pdp_")


def classify(canon_path):
    """Return BC label for canonical path; raises ValueError on unhandled."""
    parts = canon_path.split('/', 1)
    top = parts[0]
    rest = parts[1] if len(parts) > 1 else ''
    base = canon_path.rsplit('/', 1)[-1]

    # F-only handled at top-level caller; srvkm/include path
    if top == "srvkm" and rest.startswith("include/"):
        if canon_path in BC_08_FILES:
            return "BC-08"
        if "/powervr/" in canon_path or "/km/" in canon_path:
            return "BC-06"
        if base.startswith("common_") and base.endswith("_bridge.h"):
            return "BC-02"
        if (any(base.startswith(p) for p in BC_03_PREFIXES) or
                base in BC_03_BASENAMES):
            return "BC-03"
        if base.startswith("ftx"):
            return "BC-04"
        if any(base.startswith(p) for p in BC_05_PREFIXES):
            return "BC-05"
        if any(base.startswith(p) for p in BC_07_PREFIXES):
            return "BC-07"
        return "BC-01"

    if top == "srvkm":
        if (any(base.startswith(p) for p in BC_09_PREFIXES) or
                base in BC_09_BASENAMES):
            return "BC-09"
        if (any(base.startswith(p) for p in BC_10_PREFIXES) and
                base.endswith(".c")):
            return "BC-10"
        if (any(base.startswith(p) for p in BC_11_PREFIXES) and
                base.endswith(".c")):
            return "BC-11"
        if base in BC_12_BASENAMES:
            return "BC-12"
        raise ValueError(f"unhandled srvkm root: {canon_path}")

    if top == "gpu":
        if base in BC_13_BASENAMES:
            return "BC-13"
        if base.startswith("fant_"):
            return "BC-14"
        if base in BC_15_BASENAMES:
            return "BC-15"
        raise ValueError(f"unhandled gpu: {canon_path}")

    if top == "dma":
        return "BC-16"
    if top == "vpu":
        return "BC-18"
    if top == "smmu":
        return "BC-19"
    if top == "pmbus":
        return "BC-20"
    if top == "power":
        return "BC-17"
    if top == "tools":
        return "BC-21"
    raise ValueError(f"unhandled top-level: {canon_path}")


VALID_STATUSES = {'differs', 'identical', 'D-only', 'F-only', 'F-only-deferred'}


def atomic_write_text(path, content):
    """Write content to path atomically via tmp+rename; never partially overwrites."""
    tmp = f"{path}.tmp.{os.getpid()}"
    try:
        with open(tmp, 'w') as f:
            f.write(content)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except Exception:
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass
        raise


def main():
    if not os.path.isfile(MANIFEST):
        print(f"FATAL: {MANIFEST} missing", file=sys.stderr)
        return 1

    rows = []
    unknown_statuses = set()
    with open(MANIFEST) as f:
        next(f)
        for line in f:
            cols = line.rstrip('\n').split('\t')
            if len(cols) < 6:
                print(f"FATAL: manifest row has < 6 columns: {line!r}", file=sys.stderr)
                return 1
            if cols[5] not in VALID_STATUSES:
                unknown_statuses.add(cols[5])
            rows.append({
                'canon': cols[0],
                'd_rel': cols[1],
                'f_rel': cols[2],
                'status': cols[5],
            })
    if unknown_statuses:
        print(f"FATAL: manifest contains unknown statuses: {sorted(unknown_statuses)}",
              file=sys.stderr)
        return 1

    # Duplicate canonical-path detection (fail-closed: reject any dup at raw
    # manifest row level before downstream dict-keyed folds silently drop it).
    from collections import Counter
    canon_counter = Counter(r['canon'] for r in rows)
    duplicates = {p: n for p, n in canon_counter.items() if n > 1}
    if duplicates:
        sample = sorted(duplicates.items())[:10]
        print(f"FATAL: manifest contains duplicate canonical paths "
              f"(unique={len(canon_counter)} rows={len(rows)}); "
              f"sample: {sample}", file=sys.stderr)
        return 1

    assigned = {}
    differs_count = 0
    fonly_count = 0
    for row in rows:
        canon = row['canon']
        status = row['status']
        if status not in ('differs', 'F-only'):
            continue
        if status == 'differs':
            differs_count += 1
        else:
            fonly_count += 1
        # F-only: handle before general classifier
        if status == 'F-only':
            if canon == 'gpu/fant_stackprotector.c' or canon == 'gpu/fant_stackprotector.h':
                assigned[canon] = 'BC-22a'
                continue
            if canon == 'srvkm/include/common_ri_bridge.h':
                assigned[canon] = 'BC-22b'
                continue
            raise ValueError(f"unknown F-only: {canon}")
        # Differs: classify by canonical path
        assigned[canon] = classify(canon)

    # Production cardinality invariants (fail-closed): must be exactly
    # 432 differs + 3 F-only = 435, NOT "any input summing to 435".
    if not assigned:
        print("FATAL: no differs/F-only rows in manifest; cannot produce BC map",
              file=sys.stderr)
        return 1
    if differs_count != 432:
        print(f"FATAL: differs={differs_count} != 432 "
              f"(must be exactly 432 differs); refusing to write output",
              file=sys.stderr)
        return 1
    if fonly_count != 3:
        print(f"FATAL: F-only={fonly_count} != 3 "
              f"(must be exactly 3 F-only); refusing to write output",
              file=sys.stderr)
        return 1
    if len(assigned) != 435:
        print(f"FATAL: assigned {len(assigned)} != 435 "
              f"(differs+F-only cardinality mismatch); "
              f"refusing to write output", file=sys.stderr)
        return 1

    # All gates passed. Build output content in memory, then atomic write
    # (no partial overwrite). Count check now is informational only.
    out_lines = ["canon_path\tbc_id\n"]
    for p in sorted(assigned):
        out_lines.append(f"{p}\t{assigned[p]}\n")
    content = ''.join(out_lines)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    try:
        atomic_write_text(OUT, content)
    except OSError as e:
        print(f"FATAL: failed to write {OUT}: {e}", file=sys.stderr)
        return 1

    # Per-BC count
    bc_count = Counter(assigned.values())
    total = len(assigned)
    print(f"Total: {total}")
    print(f"Output: {OUT}")
    for bc in sorted(bc_count):
        print(f"  {bc}: {bc_count[bc]}")
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except ValueError as e:
        print(f"FATAL: {e}", file=sys.stderr)
        sys.exit(1)
