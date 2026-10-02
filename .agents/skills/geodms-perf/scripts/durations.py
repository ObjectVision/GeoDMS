# Wall time (first to last log timestamp) and peak commit charge per experiment, for several full.py result
# folders side by side.
#
# usage: python durations.py <folder> [<folder> ...]
#   <folder>  a full.py result folder, e.g. 20_22_0_m, looked up in the results base of machine_paths.py
#             and then in its parent, where OVSRV10 keeps a round moved aside (_aside_<label>_<date>), or an
#             absolute path; the experiment logs are in its log/ subfolder
import sys, os, re, glob
from datetime import datetime
from machine_paths import RESULTS_BASE

folders = sys.argv[1:]
ts = re.compile(rb'(20\d\d-\d\d-\d\d \d\d:\d\d:\d\d)')

def span(p):
    with open(p, 'rb') as f:
        head = f.read(4000)
        f.seek(0, 2); n = f.tell(); f.seek(max(0, n - 4000)); tail = f.read()
    a, b = ts.findall(head), ts.findall(tail)
    if not a or not b:
        return None
    t0 = datetime.strptime(a[0].decode(), '%Y-%m-%d %H:%M:%S')
    t1 = datetime.strptime(b[-1].decode(), '%Y-%m-%d %H:%M:%S')
    return (t1 - t0).total_seconds()

def peak(p):
    with open(p, 'rb') as f:
        f.seek(0, 2); n = f.tell(); f.seek(max(0, n - 6000)); tail = f.read()
    m = re.search(rb'Highest CommitCharge: (\d+)\[MB\]', tail)
    return int(m.group(1)) / 1024 if m else None

def locate(fo):
    if os.path.isabs(fo):
        return fo
    tries = [os.path.join(base, fo) for base in (RESULTS_BASE, os.path.dirname(RESULTS_BASE))]
    return next((t for t in tries if os.path.isdir(t)), tries[0])

data, names = {}, set()
for fo in folders:
    root = locate(fo)
    data[fo] = {}
    for p in glob.glob(os.path.join(root, 'log', '*.txt')):
        n = os.path.basename(p)[:-4]
        s = span(p)
        if s is not None:
            data[fo][n] = (s, peak(p))
            names.add(n)
print(f"{'experiment':52s}" + ''.join(f"{os.path.basename(f.rstrip('/'))[-20:]:>22s}" for f in folders))
tot = [0.0] * len(folders)
for n in sorted(names):
    row = f"{n[:52]:52s}"
    for i, fo in enumerate(folders):
        v = data[fo].get(n)
        if v:
            tot[i] += v[0]
            row += f"{v[0] / 60:9.1f} min" + (f" {v[1]:5.0f} GB" if v[1] is not None else "         ")
        else:
            row += f"{'-':>22s}"
    print(row)
print(f"{'TOTAL':52s}" + ''.join(f"{t / 60:9.1f} min         " for t in tot))
