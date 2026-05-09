"""Bulk-rewrite v17 GeoDMS .dms cfg files to v20 syntax.

Applies the operator and identifier renames that the v17 → v20 release
introduced. Walks one or more roots recursively, skipping `.bak` files and
known stale snapshot folders, and writes a `.pre_swor` backup of every
file it changes.

Renames performed
-----------------

| Old                            | New                          |
|--------------------------------|------------------------------|
| `subset(...)`                  | `select_with_org_rel(...)`   |
| `nr_OrgEntity`                 | `org_rel`                    |
| `Format = "EPSG:..."`          | `SpatialReference = "EPSG:..."` |
| `dijkstra_s`                   | `impedance_table`            |
| `dijkstra_m`                   | `impedance_matrix`           |
| `dijkstra_m64`                 | `impedance_matrix_od64`      |
| `select_with_attr` /          | `select_with_attr_by_cond`   |
|  `select_with_attr_(...)`      |                              |
| `partitioned_union_polygon`    | `bp_union_polygon` *         |
| `split_union_polygon`          | `bp_split_polygon` *         |
| `polygon_i4HV(g, sz)`          | `geos_buffer_multi_polygon(g, sz, 16b)` |

\\* The `bp_*` renames choose the IPoint variant. Every active call site
in the surveyed prj_snapshots tree produces an IPoint result
(rdc_mm / LatLong_mdegrees / ipoint). If a DPoint call site surfaces,
fix that one by hand to `geos_union_polygon` / `geos_split_polygon`.

Usage
-----

    # Dry run (default) — list what would change, no writes
    python tools/migrate_v17_to_v20_cfg.py --root=/path/to/prj_snapshots

    # Apply edits — writes <file>.pre_swor backup, then rewrites <file>
    python tools/migrate_v17_to_v20_cfg.py --root=/path/to/prj_snapshots --apply

    # Multiple roots
    python tools/migrate_v17_to_v20_cfg.py --root=/path/A --root=/path/B --apply

If no `--root=` is given, the script falls back to a checked-in default
suitable for the Object Vision OVSRV05 fileserver. Override on any other
machine.

After running, the matching change to the active GeoDMS engine binaries
(release 20.0.0+) is required — the renamed operators don't exist in
older engines.
"""
import os, re, sys

roots_arg = [a for a in sys.argv if a.startswith('--root=')]
if roots_arg:
    roots = [a[len('--root='):] for a in roots_arg]
else:
    # Object-Vision-internal default. Override on any other machine.
    roots = ['//OVSRV05/SourceData/RegressionTests/prj_snapshots']

skip_substrings = (
    '_old',
    '_oud',
    '/lus_demo_2022/',
    '/VestaProductie/',
    '/model-hestia-development',
    '_2022/',
    '/prj_snapshots_archive/',
)

# Regex-based renames (word-boundary, case-insensitive).
SIMPLE_RENAMES = [
    ('subset',           re.compile(r'\bsubset\b', re.IGNORECASE),                'select_with_org_rel'),
    ('nr_OrgEntity',     re.compile(r'\bnr_OrgEntity\b', re.IGNORECASE),          'org_rel'),
    ('Format=EPSG',      re.compile(r'\b[Ff]ormat(\s*=\s*"EPSG:[^"]*")'),         lambda m: 'SpatialReference' + m.group(1)),
    ('dijkstra_s',       re.compile(r'\bdijkstra_s\b', re.IGNORECASE),            'impedance_table'),
    ('dijkstra_m64',     re.compile(r'\bdijkstra_m64\b', re.IGNORECASE),          'impedance_matrix_od64'),
    ('dijkstra_m',       re.compile(r'\bdijkstra_m\b', re.IGNORECASE),            'impedance_matrix'),
    ('select_with_attr', re.compile(r'\bselect_with_attr_?(\s*\()', re.IGNORECASE), lambda m: 'select_with_attr_by_cond' + m.group(1)),
    # partitioned_union_polygon: every active call site produces an IPoint result
    # (rdc_mm / LatLong_mdegrees / ipoint), so rename to the IPoint variant.
    # If a DPoint case surfaces, fix that site by hand to geos_union_polygon.
    ('partitioned_union_polygon', re.compile(r'\bpartitioned_union_polygon\b', re.IGNORECASE), 'bp_union_polygon'),
    # split_union_polygon: both active call sites are IPoint. If a DPoint site
    # surfaces, fix it by hand to geos_split_polygon.
    ('split_union_polygon', re.compile(r'\bsplit_union_polygon\b', re.IGNORECASE), 'bp_split_polygon'),
]

# Custom: polygon_i4HV(geom, buf) -> geos_buffer_multi_polygon(geom, buf, 16b).
# Need balanced-paren handling because geom/buf may contain nested calls.
poly_pat = re.compile(r'\bpolygon_[Ii]4HV\b\s*\(', re.IGNORECASE)

def rewrite_polygon_i4hv(text):
    """Returns (new_text, count). For each polygon_i4HV(...) call, replace with
    geos_buffer_multi_polygon(...) and inject `, 16b` before the closing paren."""
    out = []
    i = 0
    count = 0
    while True:
        m = poly_pat.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()])
        out.append('geos_buffer_multi_polygon(')
        # Walk forward balancing parens to find the matching close.
        depth = 1
        j = m.end()
        in_str = False
        str_ch = ''
        while j < len(text) and depth > 0:
            c = text[j]
            if in_str:
                if c == str_ch and text[j-1] != chr(92):
                    in_str = False
            else:
                if c in ('"', "'"):
                    in_str = True
                    str_ch = c
                elif c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        break
            j += 1
        if j >= len(text):
            # unmatched — bail out, write remainder unchanged
            out.append(text[m.end():])
            break
        inner = text[m.end():j].rstrip()
        out.append(inner)
        out.append(', 16b)')
        i = j + 1
        count += 1
    return ''.join(out), count

dry_run = '--apply' not in sys.argv

stats = {'files_scanned': 0, 'files_changed': 0, 'errors': []}
counters = {label: 0 for label, _, _ in SIMPLE_RENAMES}
counters['polygon_i4HV'] = 0

for root in roots:
    if not os.path.isdir(root):
        print(f'[skip] root not found: {root}')
        continue
    for dirpath, _, files in os.walk(root):
        norm = dirpath.replace(chr(92), '/')
        if any(s in norm for s in skip_substrings):
            continue
        for fn in files:
            if not fn.endswith('.dms') or fn.endswith('.bak'):
                continue
            full = os.path.join(dirpath, fn).replace(chr(92), '/')
            stats['files_scanned'] += 1
            try:
                with open(full, 'r', encoding='utf-8', errors='replace') as f:
                    text = f.read()
            except Exception as e:
                stats['errors'].append((full, repr(e)))
                continue
            new_text = text
            per_file = {}
            for label, pat, repl in SIMPLE_RENAMES:
                hits = len(pat.findall(new_text))
                if hits == 0:
                    continue
                new_text = pat.sub(repl, new_text)
                per_file[label] = hits
                counters[label] += hits
            new_text, ph = rewrite_polygon_i4hv(new_text)
            if ph:
                per_file['polygon_i4HV'] = ph
                counters['polygon_i4HV'] += ph
            if not per_file:
                continue
            stats['files_changed'] += 1
            label_str = ', '.join(f'{k}={v}' for k, v in per_file.items())
            if dry_run:
                print(f'[dry] {full}: {label_str}')
                continue
            try:
                backup = full + '.pre_swor'
                if not os.path.exists(backup):
                    with open(backup, 'w', encoding='utf-8') as f:
                        f.write(text)
                with open(full, 'w', encoding='utf-8') as f:
                    f.write(new_text)
                print(f'[ok ] {full}: {label_str}')
            except Exception as e:
                stats['errors'].append((full, repr(e)))
                print(f'[ERR] {full}: {e}')

print()
print(f"scanned={stats['files_scanned']} changed={stats['files_changed']} errors={len(stats['errors'])}")
print('per-rename counts: ' + ', '.join(f'{k}={v}' for k, v in counters.items()))
for f, e in stats['errors'][:5]:
    print(f'  ERR  {f}: {e}')
