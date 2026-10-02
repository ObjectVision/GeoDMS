# Runs experiments of an earlier full.py round on any engine folder, one after another, each with that
# experiment's own environment but its log, local data and results in a folder of its own.
#
# usage: python run_exp.py [--dry-run] <label> <tag> <engine dir> <experiment> [<experiment> ...]
#   <label>       the full.py result folder whose .bin files describe the experiments, e.g. 20_22_1_m, in the
#                 results base of machine_paths.py
#   <tag>         a name for this run; output goes to <out>/<tag>, local data to fp_<tag> beside the round's own
#                 (<LocalDataDir>/runs/<label> -> <LocalDataDir>/runs/fp_<tag>)
#   <engine dir>  a folder with GeoDmsRun.exe, e.g. C:/LocalData/GeoDMS_engine/<name> (a kept copy) or bin/Release/x64
#   <experiment>  an experiment name without the label, e.g. t641_1_RSopen_MakeBaseData, or
#                 <experiment>@<items>: that experiment with <items> in place of its own items and no pre_clean,
#                 for full.py's indicator steps, which have no .bin of their own (store_results=False); <items>
#                 may start with a verb (t101_network_od_pc4_dense@@statistics /netwerk/...), and the log is
#                 named after the first item that is not one
#   --dry-run     load the experiments and print what would be deleted and started, without touching anything
# Set PERF_OUT to choose <out>; the default is scratch/perf_runs in the tree this script is in.
import sys, os, pickle, subprocess, time, json, shutil, glob, re
from pathlib import Path
from machine_paths import RESULTS_BASE, harness_on_path

argv = sys.argv[1:]
dry = '--dry-run' in argv
if dry:
    argv.remove('--dry-run')
label, tag, exe_dir = argv[0], argv[1], os.path.abspath(argv[2]).replace('\\', '/')
SRC = f"{RESULTS_BASE}/{label}/bin"
OUT = os.environ.get('PERF_OUT', str(Path(__file__).resolve().parents[4] / 'scratch' / 'perf_runs')).replace('\\', '/')
harness_on_path(sys.path)  # the pickled Experiment class lives there
d = f"{OUT}/{tag}"
if not dry:
    os.makedirs(d + '/log', exist_ok=True)
    os.makedirs(d + '/result', exist_ok=True)

def parse_env(s):
    env = {}
    for part in s.split(';'):
        if '=' in part:
            k, v = part.split('=', 1)
            env[k] = v
    return env

def fix(v, subst):
    for a, b in subst:
        v = v.replace(a, b)
    return v

for arg in argv[3:]:
    name, _, item = arg.partition('@')
    exp = pickle.load(open(f"{SRC}/{label}__{name}.bin", 'rb'))
    if item:
        parts = exp.command.split(' ')
        cfg = next(i for i, p in enumerate(parts) if p.lower().endswith('.dms'))
        parts = parts[:cfg + 1] + [item]
        first = next(t for t in item.split() if not t.startswith('@'))
        parts = [p.replace(f"/{name}.txt", f"/{name}_{first.strip('/').replace('/', '_')}.txt") for p in parts]
        exp.command = ' '.join(parts)
        exp.pre_clean = []
    raw = parse_env(exp.environment_variables)
    # Redirected as the round recorded them: its results folder, its local data, which full.py gives each round
    # a folder of its own for (<LocalDataDir>/runs/<label>), and its engine folder, quoted as GeoDmsPath is.
    run_root = raw['GEODMS_DIRECTORIES_LOCALDATADIR']
    if not run_root.endswith(f"/runs/{label}"):
        sys.exit(f"{label} computed in {run_root}, which every round shares; use a round with its own runs/<label>")
    subst = [(raw.get('results_folder', f"{RESULTS_BASE}/{label}"), d),
             (run_root, f"{run_root[:-len(label)]}fp_{tag}"),
             (raw['GeoDmsPath'], f'"{exe_dir}"')]
    e = {k: fix(v, subst) for k, v in raw.items()}
    env = dict(os.environ)
    env.pop('NoDefaultCurrentDirectoryInExePath', None)
    env.update(e)
    tmp = e['tmpFileDir']
    pre_clean = [fix(p, subst) for p in (exp.pre_clean or [])]
    # full.py's command reads "<dir>"/GeoDmsRun.exe; quote the whole path and start it without a shell
    cmd = re.sub(f'"{re.escape(exe_dir)}"/(\\S+)', lambda m: f'"{exe_dir}/{m.group(1)}"', fix(exp.command, subst), count=1)
    if dry:
        print(f"{tag} {arg} from {SRC}/{label}__{name}.bin", flush=True)
        for k, v in e.items():
            if v != raw[k]:
                print(f"    env {k}={v}")
        print(f"    write {tmp}/results_folder.txt: {d}/result")
        for p in pre_clean:
            print(f"    delete {p}" + ("" if os.path.isdir(p) else " (absent)"))
        print(f"    start {cmd}", flush=True)
        continue
    os.makedirs(tmp, exist_ok=True)
    with open(f"{tmp}/results_folder.txt", "w") as f:  # full.py writes it before each round, without a newline
        f.write(f"{d}/result")
    for p in pre_clean:
        if os.path.isdir(p):
            shutil.rmtree(p)
    t0 = time.time()
    rc = subprocess.call(cmd, env=env)
    print(f"{tag} {arg} rc={rc} secs={time.time() - t0:.0f}", flush=True)
if dry:
    sys.exit(0)
for f in sorted(glob.glob(f"{d}/result/*.result.json")):
    # a metric without a value carries counts (n_total, n_diff: t101, t200, t300, ...) and is printed whole
    vals = [m.get('value', m) for m in json.load(open(f)).get('metrics', [])]
    print(f"{tag} {os.path.basename(f)} values={vals}", flush=True)
print(f"{tag} DONE", flush=True)
