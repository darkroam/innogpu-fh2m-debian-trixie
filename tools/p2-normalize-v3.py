#!/usr/bin/env python3
"""P2 manifest v3: handle inno_pvr_ → fant_ft_ correctly.

Usage: p2-normalize-v3.py [OUTPUT_PATH]

If OUTPUT_PATH is not provided, writes to build/p2-manifest.tsv.
Uses LC_ALL=C for deterministic sorting.
"""
import os, hashlib, sys, locale

# Force C locale for deterministic sorting
locale.setlocale(locale.LC_ALL, 'C')

D_SRC = os.environ.get("P2_NORM_D_SRC", "build/r16-unpack-D/usr/src/innogpu-kernel-2.2")
F_SRC = os.environ.get("P2_NORM_F_SRC", "build/r16-fantgpu-deb/usr/src/fantgpu-fh2m-kernel-2.2")

MOD_MAP = {
    'innogpu': 'gpu', 'fantgpu': 'gpu',
    'innodma': 'dma', 'fantdma': 'dma',
    'innopmbus': 'pmbus', 'fantpmbus': 'pmbus',
    'innopower': 'power', 'fantpower': 'power',
    'innosmmu': 'smmu', 'fantsmmu': 'smmu',
    'innosrvkm': 'srvkm', 'fantsrvkm': 'srvkm',
    'innovpu': 'vpu', 'fantvpu': 'vpu',
    'tools': 'tools',
}

def normalize_basename(name):
    # Step 1: Strip module-name-in-filename (innogpu_→gpu_, fantgpu_→gpu_, etc.)
    for d_p, f_p, c in [
        ('innogpu_','fantgpu_','gpu_'), ('innodma_','fantdma_','dma_'),
        ('innopmbus_','fantpmbus_','pmbus_'), ('innopower_','fantpower_','power_'),
        ('innosmmu_','fantsmmu_','smmu_'), ('innovpu_','fantvpu_','vpu_'),
    ]:
        if name.startswith(d_p): name = c + name[len(d_p):]; break
        if name.startswith(f_p): name = c + name[len(f_p):]; break
    else:
        for d_p, f_p, c in [
            ('innogpu.','fantgpu.','gpu.'), ('innodma.','fantdma.','dma.'),
            ('innopmbus.','fantpmbus.','pmbus.'), ('innopower.','fantpower.','power.'),
            ('innosmmu.','fantsmmu.','smmu.'), ('innovpu.','fantvpu.','vpu.'),
        ]:
            if name.startswith(d_p): name = c + name[len(d_p):]; break
            if name.startswith(f_p): name = c + name[len(f_p):]; break

    # Step 2: innodpu_/fantdpu_ → dpu_
    if name.startswith('innodpu_'): name = 'dpu_' + name[8:]
    elif name.startswith('fantdpu_'): name = 'dpu_' + name[8:]

    # Step 3: innoaudio_/fantaudio_ → audio_
    if name.startswith('innoaudio_'): name = 'audio_' + name[10:]
    elif name.startswith('fantaudio_'): name = 'audio_' + name[10:]

    # Step 4: innoversion/fantversion → version
    if name.startswith('innoversion'): name = 'version' + name[11:]
    elif name.startswith('fantversion'): name = 'version' + name[11:]

    # Step 5: inno_ → fant_ (D vendor prefix to canonical F prefix)
    if name.startswith('inno_'):
        name = 'fant_' + name[5:]
    # F's fant_ stays

    # Step 6: NOW apply internal vendor replacements on the full name
    # These handle cases like fant_pvr_spec → fant_ft_spec
    # pvrsrv_ → ftsrv_
    name = name.replace('pvrsrv_', 'ftsrv_')
    # pvr_ → ft_ (but not if already ftsrv_)
    name = name.replace('pvr_', 'ft_')
    # pvr without underscore (e.g., pvr → ft in compound names)
    # Be careful: only replace _pvr patterns
    # rgx_ → ftx_
    name = name.replace('rgx_', 'ftx_')
    # rgx without underscore in compound names
    # img_ → fant_
    name = name.replace('img_', 'fant_')
    # ospvr_ → osft_
    name = name.replace('ospvr_', 'osft_')
    # sofunc_pvr → sofunc_ft, sofunc_rgx → sofunc_ftx
    name = name.replace('sofunc_pvr', 'sofunc_ft')
    name = name.replace('sofunc_rgx', 'sofunc_ftx')
    # gpu_info_innoml → gpu_info_fantml
    name = name.replace('gpu_info_innoml', 'gpu_info_fantml')
    # bridge renames
    name = name.replace('common_innogpu_bridge', 'common_gpu_bridge')
    name = name.replace('common_fantgpu_bridge', 'common_gpu_bridge')
    name = name.replace('common_pvrtl_bridge', 'common_fttl_bridge')
    name = name.replace('common_rgx', 'common_ftx')
    # rgxconfig → ftxconfig, rgxcore → ftxcore
    name = name.replace('rgxconfig', 'ftxconfig')
    name = name.replace('rgxcore', 'ftxcore')

    # Step 7: no-underscore prefix replacements for remaining files
    # pvrsrv (no underscore) must come before pvr (longer match first)
    name = name.replace('pvrsrv', 'ftsrv')
    name = name.replace('pvr', 'ft')
    name = name.replace('rgx', 'ftx')

    return name

def normalize_path(rel_path):
    parts = rel_path.split('/', 1)
    mod = parts[0]
    rest = parts[1] if len(parts) > 1 else ''
    canon_mod = MOD_MAP.get(mod, mod)
    dirname = os.path.dirname(rest)
    basename = os.path.basename(rest)
    norm = normalize_basename(basename)
    if dirname:
        return f"{canon_mod}/{dirname}/{norm}"
    return f"{canon_mod}/{norm}"

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()

def collect_files(base_dir):
    result = {}
    conflicts = []
    for root, dirs, files in os.walk(base_dir):
        for f in sorted(files):
            if f.endswith('.c') or f.endswith('.h'):
                full = os.path.join(root, f)
                rel = os.path.relpath(full, base_dir)
                canon = normalize_path(rel)
                sha = sha256_file(full)
                if canon in result:
                    conflicts.append((canon, rel, result[canon][1]))
                    print(f"CONFLICT: {canon} from {rel} vs {result[canon][1]}", file=sys.stderr)
                else:
                    result[canon] = (full, rel, sha)
    return result, conflicts

def main():
    if len(sys.argv) > 2:
        print("Usage: p2-normalize-v3.py [OUTPUT_PATH]", file=sys.stderr)
        sys.exit(2)

    output_path = sys.argv[1] if len(sys.argv) > 1 else 'build/p2-manifest.tsv'

    if not os.path.isdir(D_SRC):
        print(f"FATAL: D source root not found: {D_SRC}", file=sys.stderr)
        sys.exit(3)
    if not os.path.isdir(F_SRC):
        print(f"FATAL: F source root not found: {F_SRC}", file=sys.stderr)
        sys.exit(3)

    d_files, d_conflicts = collect_files(D_SRC)
    f_files, f_conflicts = collect_files(F_SRC)

    if not d_files:
        print(f"FATAL: no .c/.h files found in D source: {D_SRC}", file=sys.stderr)
        sys.exit(4)
    if not f_files:
        print(f"FATAL: no .c/.h files found in F source: {F_SRC}", file=sys.stderr)
        sys.exit(4)

    all_conflicts = d_conflicts + f_conflicts
    if all_conflicts:
        print(f"FATAL: {len(all_conflicts)} canonical-path conflict(s); aborting.", file=sys.stderr)
        sys.exit(1)
    d_keys = set(d_files.keys())
    f_keys = set(f_files.keys())
    common = sorted(d_keys & f_keys)
    d_only = sorted(d_keys - f_keys)
    f_only = sorted(f_keys - d_keys)
    identical = sum(1 for k in common if d_files[k][2] == f_files[k][2])
    differs = len(common) - identical

    with open(output_path, 'w') as mf:
        mf.write("canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus\n")
        for k in common:
            dp,dr,ds = d_files[k]; fp,fr,fs = f_files[k]
            mf.write(f"{k}\t{dr}\t{fr}\t{ds}\t{fs}\t{'identical' if ds==fs else 'differs'}\n")
        for k in d_only:
            dp,dr,ds = d_files[k]
            mf.write(f"{k}\t{dr}\t\t{ds}\t\tD-only\n")
        for k in f_only:
            fp,fr,fs = f_files[k]
            mf.write(f"{k}\t\t{fr}\t\t{fs}\tF-only\n")

    print(f"D total: {len(d_files)}")
    print(f"F total: {len(f_files)}")
    print(f"Common (pairs): {len(common)}")
    print(f"D-only: {len(d_only)}")
    print(f"F-only: {len(f_only)}")
    print(f"Identical: {identical}")
    print(f"Differs: {differs}")
    print(f"Output: {output_path}")
    if d_only:
        print("\nD-only:"); [print(f"  {k} <- {d_files[k][1]}") for k in d_only]
    if f_only:
        print("\nF-only:"); [print(f"  {k} <- {f_files[k][1]}") for k in f_only]

if __name__ == '__main__':
    main()
