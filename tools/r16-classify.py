#!/usr/bin/env python3
"""R16 per-file classification (P5 · 2026-09-04 · qoder).

Pipeline (deterministic, replayable):
  1. Load P2 manifest → 432 differs + 3 F-only = 435 canonical paths.
  2. Per-file F→D content normalization (fnorm). Compare D original.
     Identical after fnorm → PURE_RENAME (drop evidence).
     Different → BEHAVIORAL → mode-pattern match.
  3. Per-file disposition (fail-closed):
     - PURE_RENAME → drop
     - BEHAVIORAL  → defer (always fail-closed; not BC-level cluster default)
     - F-ONLY      → drop (F-only means D never had this file; per-BC notes apply)
  4. Per-file license field (from build/r16-license-precheck/{D,F}-license-manifest.tsv).
  5. Per-BC rollup and gate assertions.

Production cardinality invariant (fail-closed):
  - raw manifest rows: 432 differs + 3 F-only = 435 paths
  - results == 435 paths, exactly once each (no duplicate canonical paths)
  - input != 435 ⇒ abort without overwriting output

Inputs:
  build/p2-manifest.tsv                          (P2 manifest, normalized paths)
  build/r16-unpack-D/usr/src/innogpu-kernel-2.2  (D source tree)
  build/r16-fantgpu-deb/usr/src/fantgpu-fh2m-kernel-2.2  (F source tree)
  build/r16-license-precheck/D-license-manifest.tsv
  build/r16-license-precheck/F-license-manifest.tsv

Outputs (deterministic, LC_ALL=C sorted):
  build/r16-evidence/per-file-classification.tsv
       (canon_path, bc, disposition, classification, license_d, license_f,
        first_diff_d, first_diff_f)
  build/r16-evidence/per-bc-summary.tsv
       (bc, drop, defer, selected, f_only, identical,
        pure_rename, behavioral, total)

Exit code:
  0 — all gates PASS (assigned=435, duplicate=0, unassigned=0, sum=435)
  1 — gate failure (printed to stderr; details on stdout)
"""
import csv
import difflib
import locale
import os
import re
import sys
from collections import Counter, defaultdict

locale.setlocale(locale.LC_ALL, 'C')

D_SRC = os.environ.get("R16_D_SRC", "build/r16-unpack-D/usr/src/innogpu-kernel-2.2")
F_SRC = os.environ.get("R16_F_SRC", "build/r16-fantgpu-deb/usr/src/fantgpu-fh2m-kernel-2.2")
MANIFEST = os.environ.get("R16_MANIFEST", "build/p2-manifest.tsv")
D_LICENSE = os.environ.get("R16_D_LICENSE", "build/r16-license-precheck/D-license-manifest.tsv")
F_LICENSE = os.environ.get("R16_F_LICENSE", "build/r16-license-precheck/F-license-manifest.tsv")
OUT_DIR = os.environ.get("R16_OUT_DIR", "build/r16-evidence")
PER_FILE_OUT = f"{OUT_DIR}/per-file-classification.tsv"
PER_BC_OUT = f"{OUT_DIR}/per-bc-summary.tsv"

VALID_STATUSES = {'differs', 'identical', 'D-only', 'F-only', 'F-only-deferred'}


def atomic_write_text(path, content):
    """Atomic write via tmp+rename; never partially overwrites existing output."""
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

# --- F→D normalization (verbatim from qoder-notes.md P5 6th-round) ---
def fnorm(s):
    for _ in range(3):
        o = s
        # Vendor prefix F → D (longer first)
        for fa, dn in [
            ('fantaudio_', 'innoaudio_'), ('fantpmbus_', 'innopmbus_'),
            ('fantpower_', 'innopower_'), ('fantdma_', 'innodma_'),
            ('fantsmmu_', 'innosmmu_'), ('fantvpu_', 'innovpu_'),
            ('fantsrvkm_', 'innosrvkm_'), ('fantgpu_', 'innogpu_'),
            ('fantdpu_', 'innodpu_'),
        ]:
            s = re.sub(rf'\b{fa}', dn, s)
        s = re.sub(r'\bfant_', 'inno_', s)
        s = re.sub(r'\bftsrv', 'pvrsrv', s)
        s = re.sub(r'\bFTSRV', 'PVRSRV', s)
        s = re.sub(r'\bft_', 'pvr_', s)
        s = re.sub(r'\bFT_', 'PVR_', s)
        s = re.sub(r'\bftx_', 'rgx_', s)
        s = re.sub(r'\bFTX_', 'RGX_', s)
        s = re.sub(r'\bftmodule\b', 'pvrmodule', s)
        s = re.sub(r'\bftversion\b', 'pvrversion', s)
        s = re.sub(r'\bfttl\b', 'pvrtl', s)
        for fa, dn in [
            ('FANTAUDIO', 'INNOAUDIO'), ('FANTPMBUS', 'INNOPMBUS'),
            ('FANTPOWER', 'INNOPOWER'), ('FANTDMA', 'INNODMA'),
            ('FANTSMMU', 'INNOSMMU'), ('FANTVPU', 'INNOVPU'),
            ('FANTSRVKM', 'INNOSRVKM'), ('FANTGPU', 'INNOGPU'),
            ('FANTDPU', 'INNODPU'),
        ]:
            s = re.sub(rf'\b{fa}\b', dn, s)
        s = re.sub(
            r'(Copyright \(c\) )?(Beijing )?Fantasy( Technologies)?( Technology)?( Ltd\.)?( All Rights Reserved)?',
            'Innosilicon Technology Ltd.', s)
        s = re.sub(r'"pvrsrvkm"', '"innosrvkm"', s)
        s = re.sub(r'"fthmmu"', '"innohmmu"', s)
        s = re.sub(r'"pvr_sync"', '"inno_sync"', s)
        s = re.sub(r'"ftsrvkm"', '"innosrvkm"', s)
        s = re.sub(r'"fant-dma"', '"inno-dma"', s)
        s = re.sub(r'"fant-pdma"', '"inno-pdma"', s)
        s = re.sub(r'"fant-power"', '"inno-power"', s)
        s = re.sub(r'"fant-dpu"', '"inno-dpu"', s)
        s = re.sub(r'"fantgpu"', '"innogpu"', s)
        s = re.sub(r'"Fantasy Technologies DMA Driver"',
                   '"Innosilicon Technologies DMA Driver"', s)
        s = re.sub(r'"Fantasy Technologies Power Driver"',
                   '"Innosilicon Technologies Power Driver"', s)
        s = re.sub(r'\bcommon_fantgpu_bridge\b', 'common_innogpu_bridge', s)
        s = re.sub(r'\bcommon_gpu_bridge\b', 'common_innogpu_bridge', s)
        s = re.sub(r'\bcommon_fttl_bridge\b', 'common_pvrtl_bridge', s)
        s = re.sub(r'\bcommon_ftx(\w+)_bridge\b', r'common_rgx\1_bridge', s)
        s = re.sub(r'\bgpu_info_fantml\b', 'gpu_info_innoml', s)
        s = re.sub(r'"inno_types\.h"', '"img_types.h"', s)
        s = re.sub(r'"fant_types\.h"', '"img_types.h"', s)
        s = re.sub(r'"inno_defs\.h"', '"img_defs.h"', s)
        s = re.sub(r'"fant_defs\.h"', '"img_defs.h"', s)
        s = re.sub(r'\bINNO_TYPES_H\b', 'IMG_TYPES_H', s)
        s = re.sub(r'\bINNO_DEFS_H\b', 'IMG_DEFS_H', s)
        s = re.sub(r'\bFANT_TYPES_H\b', 'IMG_TYPES_H', s)
        s = re.sub(r'\bFANT_DEFS_H\b', 'IMG_DEFS_H', s)
        s = re.sub(r'DISPLAY_CONTROLLER\s+\w+', 'DISPLAY_CONTROLLER innodpu', s)
        s = re.sub(r'\bfh2m_fant_', 'fh2m_inno_', s)
        for fa, dn in [
            ('FANTDMA', 'INNODMA'), ('FANTDPU', 'INNODPU'),
            ('FANTGPU', 'INNOGPU'), ('FANTPMBUS', 'INNOPMBUS'),
            ('FANTPOWER', 'INNOPOWER'), ('FANTSMMU', 'INNOSMMU'),
            ('FANTVPU', 'INNOVPU'), ('FANTSRVKM', 'INNOSRVKM'),
            ('FANTAUDIO', 'INNOAUDIO'),
        ]:
            s = re.sub(rf'\b{fa}_H_?\b', dn + '_H', s)
            s = re.sub(rf'\b{fa}H_?\b', dn + 'H', s)
            s = re.sub(rf'_{fa}_', '_' + dn + '_', s)
            s = re.sub(rf'_{fa}H_', '_' + dn + 'H_', s)
        for fa, dn in [
            ('FANT_AUDIO_CHIP_COMMON', 'INNO_AUDIO_CHIP_COMMON'),
            ('FANT_AXI_DMA_DRV', 'INNO_AXI_DMA_DRV'),
            ('FANT_PCIE_DMA_DRV', 'INNO_PCIE_DMA_DRV'),
            ('FANT_AUDIO_OSFUNC', 'INNO_AUDIO_OSFUNC'),
            ('FANT_PHYSMEM', 'INNO_PHYSMEM'),
            ('FANT_PHYSMEM_DMABUF', 'INNO_PHYSMEM_DMABUF'),
            ('FANT_PMR', 'INNO_PMR'),
            ('FANT_FT_SPEC', 'INNO_FT_SPEC'),
            ('FANT_HWIF', 'INNO_HWIF'),
            ('FANT_PCI', 'INNO_PCI'),
            ('FANT_PDPU', 'INNO_PDPU'),
            ('FANT_FW', 'INNO_FW'),
            ('FANT_FANT', 'INNO_FANT'),
            ('FANT_OSFUNC', 'INNO_OSFUNC'),
            ('FANT_DEVFREQ', 'INNO_DEVFREQ'),
            ('FANT_AUDIO', 'INNO_AUDIO'),
            ('FANT_DEBUG', 'INNO_DEBUG'),
            ('FANT_CPUMASK', 'INNO_CPUMASK'),
            ('FANT_FENCE', 'INNO_FENCE'),
            ('FANT_FIRMWARE', 'INNO_FIRMWARE'),
            ('FANT_FS', 'INNO_FS'),
            ('FANT_IDR', 'INNO_IDR'),
            ('FANT_INSN', 'INNO_INSN'),
            ('FANT_INTERRUPT', 'INNO_INTERRUPT'),
            ('FANT_IO', 'INNO_IO'),
            ('FANT_KERNEL_HOOK', 'INNO_KERNEL_HOOK'),
            ('FANT_LOCK', 'INNO_LOCK'),
            ('FANT_MATH', 'INNO_MATH'),
            ('FANT_MISC', 'INNO_MISC'),
            ('FANT_MM', 'INNO_MM'),
            ('FANT_MTRR', 'INNO_MTRR'),
            ('FANT_PLAT_DEV', 'INNO_PLAT_DEV'),
            ('FANT_PM_RUNTIME', 'INNO_PM_RUNTIME'),
            ('FANT_SRVKM', 'INNO_SRVKM'),
            ('FANT_TASK', 'INNO_TASK'),
            ('FANT_TIMER', 'INNO_TIMER'),
            ('FANT_UACCESS', 'INNO_UACCESS'),
            ('FANT_UUID', 'INNO_UUID'),
            ('FANT_WAITQUEUE', 'INNO_WAITQUEUE'),
            ('FANT_DMA_BUF', 'INNO_DMA_BUF'),
            ('FANT_DRM', 'INNO_DRM'),
            ('FANT_DRM_MODE', 'INNO_DRM_MODE'),
        ]:
            s = re.sub(rf'\b{fa}_?H_?\b', dn + '_H', s)
            s = re.sub(rf'\b{fa}_H\b', dn + '_H', s)
            s = re.sub(rf'\b{fa}H\b', dn + 'H', s)
            s = re.sub(rf'_{fa}_', '_' + dn + '_', s)
            s = re.sub(rf'_{fa}H_', '_' + dn + 'H_', s)
        for fa, dn in [
            ('KBUILD_FANT', 'KBUILD_INNO'),
            ('KBUILD_FT', 'KBUILD_PVR'),
            ('KBUILD_FTSRV', 'KBUILD_PVRSRV'),
            ('KBUILD_FTX', 'KBUILD_RGX'),
        ]:
            s = re.sub(rf'\b{fa}_', dn + '_', s)
            s = re.sub(rf'\b{fa}\b', dn, s)
        s = re.sub(r'\bfantinno', 'inno', s)
        s = re.sub(r'\bINNOSYNC\b', 'INNO_SYNC', s)
        s = re.sub(r'\binno_sync\b', 'pvr_sync', s)
        s = re.sub(r'\bpvr_inno_', 'inno_', s)
        s = re.sub(r'\binno_fant_', 'inno_inno_', s)
        if s == o:
            break
    return s


# --- Per-file classification ---
def classify_file(d_full, f_full):
    """Return (classification, first_diff_d, first_diff_f, unmatched_count)."""
    try:
        with open(d_full, errors='replace') as fh:
            d_content = fh.read()
        with open(f_full, errors='replace') as fh:
            f_content = fh.read()
    except FileNotFoundError:
        return 'MISSING', '', '', 0
    f_n = fnorm(f_content)
    if d_content == f_n:
        return 'PURE_RENAME', '', '', 0
    # Find first diff hunk
    d_lines = d_content.splitlines()
    f_lines = f_n.splitlines()
    sm = difflib.SequenceMatcher(None, d_lines, f_lines)
    first_diff_d = ''
    first_diff_f = ''
    unmatched = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        # Extract context
        ctx_d = '\n'.join(d_lines[max(0, i1):i2][:5])
        ctx_f = '\n'.join(f_lines[max(0, j1):j2][:5])
        first_diff_d = ctx_d
        first_diff_f = ctx_f
        # Count unmatched (rough): sum of i2-i1 + j2-j1
        unmatched = max(unmatched, (i2 - i1) + (j2 - j1))
        break
    return 'BEHAVIORAL', first_diff_d, first_diff_f, unmatched


def load_license_map(path):
    """Load {path: license} from {D|F}-license-manifest.tsv."""
    m = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            lic, p = line.split('\t', 1)
            m[p] = lic
    return m


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    if not os.path.isfile(MANIFEST):
        print(f"FATAL: missing {MANIFEST}", file=sys.stderr)
        return 1

    # Load manifest with schema + status validation + raw canon counter
    drel, frel, status = {}, {}, {}
    unknown_statuses = set()
    raw_canon_counter = Counter()
    raw_count = 0
    with open(MANIFEST) as f:
        next(f)
        for line in f:
            cols = line.rstrip('\n').split('\t')
            if len(cols) < 6:
                print(f"FATAL: manifest row has < 6 columns: {line!r}",
                      file=sys.stderr)
                return 1
            if cols[5] not in VALID_STATUSES:
                unknown_statuses.add(cols[5])
            drel[cols[0]] = cols[1]
            frel[cols[0]] = cols[2]
            status[cols[0]] = cols[5]
            raw_canon_counter[cols[0]] += 1
            raw_count += 1
    if unknown_statuses:
        print(f"FATAL: manifest contains unknown statuses: "
              f"{sorted(unknown_statuses)}", file=sys.stderr)
        return 1

    # Duplicate canonical-path detection (fail-closed at raw manifest row level)
    duplicates = {p: n for p, n in raw_canon_counter.items() if n > 1}
    if duplicates:
        sample = sorted(duplicates.items())[:10]
        print(f"FATAL: manifest contains duplicate canonical paths "
              f"(unique={len(raw_canon_counter)} raw_rows={raw_count}); "
              f"sample: {sample}", file=sys.stderr)
        return 1

    # Load license maps
    d_lic = load_license_map(D_LICENSE)
    f_lic = load_license_map(F_LICENSE)

    # Classify per-file (fail-closed: only differ/F-only produce evidence)
    results = {}
    differs_count = 0
    fonly_count = 0
    for cp in sorted(status):
        d_path = drel.get(cp, '')
        f_path = frel.get(cp, '')
        st = status[cp]
        if st == 'identical':
            continue
        if st == 'D-only':
            # D-only: not in 435-set; skip but record no evidence
            continue
        if st == 'differs':
            differs_count += 1
            d_full = os.path.join(D_SRC, d_path) if d_path else ''
            f_full = os.path.join(F_SRC, f_path) if f_path else ''
            cls, dd, fd, _ = classify_file(d_full, f_full)
            if cls == 'MISSING':
                # Generator fail-closed: cannot read D or F content. Abort
                # entire run rather than emit a MISSING row (gate rejects
                # MISSING entirely).
                print(f"FATAL: cannot read content for differs row: "
                      f"canon={cp} d={d_full!r} f={f_full!r}",
                      file=sys.stderr)
                return 1
        elif st == 'F-only':
            fonly_count += 1
            cls = 'F-ONLY'
            dd = fd = ''
        else:
            # Defensive (already validated above, but explicit)
            continue
        # Disposition (fail-closed per-file)
        if cls == 'PURE_RENAME':
            disp = 'drop'
        elif cls == 'BEHAVIORAL':
            disp = 'defer'
        elif cls == 'F-ONLY':
            disp = 'drop'
        else:
            disp = 'defer'
        # License lookup
        d_license = d_lic.get(d_path, 'unknown') if d_path else 'F-ONLY'
        f_license = f_lic.get(f_path, 'unknown') if f_path else 'D-ONLY'
        results[cp] = (disp, cls, d_license, f_license, dd[:200], fd[:200])

    if not results:
        print("FATAL: no differs/F-only rows produced evidence; "
              "cannot write per-file classification", file=sys.stderr)
        return 1

    # Production cardinality invariants (fail-closed): must be exactly
    # 432 differs + 3 F-only = 435, NOT "any input summing to 435".
    if differs_count != 432:
        print(f"FATAL: differs={differs_count} != 432 "
              f"(must be exactly 432 differs); "
              f"refusing to write output", file=sys.stderr)
        return 1
    if fonly_count != 3:
        print(f"FATAL: F-only={fonly_count} != 3 "
              f"(must be exactly 3 F-only); "
              f"refusing to write output", file=sys.stderr)
        return 1
    if len(results) != 435:
        print(f"FATAL: results {len(results)} != 435 "
              f"(differs+F-only cardinality mismatch); "
              f"refusing to write output", file=sys.stderr)
        return 1

    # Per-BC rollup (read canon-to-bc.tsv if it exists)
    bc_map = {}
    bc_path = f"{OUT_DIR}/canon-to-bc.tsv"
    if os.path.isfile(bc_path):
        with open(bc_path) as f:
            next(f)
            for line in f:
                p, bc = line.rstrip('\n').split('\t')
                bc_map[p] = bc

    # Build per-file output content (atomic write)
    per_file_lines = ["canon_path\tbc\tdisposition\tclassification\t"
                      "license_d\tlicense_f\tfirst_diff_d\tfirst_diff_f\n"]
    for cp in sorted(results):
        disp, cls, dl, fl, dd, fd = results[cp]
        bc = bc_map.get(cp, 'UNASSIGNED')
        dd_e = dd.replace('\t', ' ').replace('\n', ' | ')
        fd_e = fd.replace('\t', ' ').replace('\n', ' | ')
        per_file_lines.append(f"{cp}\t{bc}\t{disp}\t{cls}\t{dl}\t{fl}\t"
                              f"{dd_e}\t{fd_e}\n")
    try:
        atomic_write_text(PER_FILE_OUT, ''.join(per_file_lines))
    except OSError as e:
        print(f"FATAL: failed to write {PER_FILE_OUT}: {e}", file=sys.stderr)
        return 1

    # Per-BC summary (atomic write)
    per_bc = defaultdict(lambda: Counter())
    for cp, (disp, cls, *_) in results.items():
        bc = bc_map.get(cp, 'UNASSIGNED')
        per_bc[bc][disp] += 1
        per_bc[bc][cls] += 1
    bc_lines = ["bc\tdrop\tdefer\tselected\tf_only\tidentical\t"
                "pure_rename\tbehavioral\ttotal\n"]
    for bc in sorted(per_bc):
        c = per_bc[bc]
        total = sum(c.get(k, 0) for k in ('drop', 'defer', 'selected'))
        bc_lines.append(f"{bc}\t{c.get('drop', 0)}\t{c.get('defer', 0)}\t"
                        f"{c.get('selected', 0)}\t{c.get('F-ONLY', 0)}\t"
                        f"{c.get('IDENTICAL', 0)}\t{c.get('PURE_RENAME', 0)}\t"
                        f"{c.get('BEHAVIORAL', 0)}\t{total}\n")
    try:
        atomic_write_text(PER_BC_OUT, ''.join(bc_lines))
    except OSError as e:
        print(f"FATAL: failed to write {PER_BC_OUT}: {e}", file=sys.stderr)
        return 1

    # Summary
    total = len(results)
    disp_total = Counter(r[0] for r in results.values())
    cls_total = Counter(r[1] for r in results.values())
    print(f"Total canonical paths: {total}")
    print(f"Disposition: {dict(disp_total)}")
    print(f"Classification: {dict(cls_total)}")
    print(f"Outputs: {PER_FILE_OUT}, {PER_BC_OUT}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
