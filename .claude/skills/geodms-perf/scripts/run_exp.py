# Runs experiments of an earlier full.py round on any engine folder, one after another, each with that
# experiment's own environment but its log, local data and results in a folder of its own.
#
# usage: python run_exp.py <label> <tag> <engine dir> <experiment> [<experiment> ...]
#   <label>       the full.py result folder whose .bin files describe the experiments, e.g. 20_22_1_m
#   <tag>         a name for this run; output goes to <out>/<tag>, local data to C:/LocalData/runs/fp_<tag>
#   <engine dir>  a folder with GeoDmsRun.exe, e.g. C:/LocalData/GeoDMS_engine/<name> (a kept copy) or bin/Release/x64
#   <experiment>  an experiment name without the label, e.g. t641_1_RSopen_MakeBaseData, or
#                 <experiment>@<items>: that experiment with <items> in place of its own items and no pre_clean,
#                 for full.py's indicator steps, which have no .bin of their own (store_results=False)
# Set PERF_OUT to choose <out>; the default is C:/dev/GeoDMS_2026/scratch/perf_runs.
import sys, os, pickle, subprocess, time, json, shutil, glob

label, tag, exe_dir = sys.argv[1], sys.argv[2], sys.argv[3].replace('\\', '/')
SRC = f"C:/LocalData/GeoDMS-Test/Regression/{label}/bin"
OUT = os.environ.get('PERF_OUT', 'C:/dev/GeoDMS_2026/scratch/perf_runs').replace('\\', '/')
sys.path.insert(0, r"C:\dev\tst\batch\generic")  # the pickled Experiment class lives there
sys.path.insert(0, r"C:\dev\tst\batch")
d = f"{OUT}/{tag}"
local = f"C:/LocalData/runs/fp_{tag}"
os.makedirs(d + '/log', exist_ok=True)
os.makedirs(d + '/result', exist_ok=True)

def parse_env(s):
    env = {}
    for part in s.split(';'):
        if '=' in part:
            k, v = part.split('=', 1)
            env[k] = v
    return env

def fix(v):
    v = v.replace(f'C:/LocalData/runs/{label}', local)
    v = v.replace(f'C:/LocalData/GeoDMS-Test/Regression/{label}', d)
    for b in ('"C:/dev/GeoDMS_2026/bin/Release/x64"', '"C:/dev/GeoDMS_2026/build/windows-x64-release/bin"'):
        v = v.replace(b, f'"{exe_dir}"')
    return v

for arg in sys.argv[4:]:
    name, _, item = arg.partition('@')
    exp = pickle.load(open(f"{SRC}/{label}__{name}.bin", 'rb'))
    if item:
        parts = exp.command.split(' ')
        cfg = next(i for i, p in enumerate(parts) if p.lower().endswith('.dms'))
        parts = parts[:cfg + 1] + [item]
        parts = [p.replace(f"/{name}.txt", f"/{name}_{item.strip('/').split('/')[0]}.txt") for p in parts]
        exp.command = ' '.join(parts)
        exp.pre_clean = []
    e = {k: fix(v) for k, v in parse_env(exp.environment_variables).items()}
    env = dict(os.environ)
    env.pop('NoDefaultCurrentDirectoryInExePath', None)
    env.update(e)
    tmp = e['tmpFileDir']
    os.makedirs(tmp, exist_ok=True)
    with open(f"{tmp}/results_folder.txt", "w") as f:  # full.py writes it before each round, without a newline
        f.write(f"{d}/result")
    for p in (exp.pre_clean or []):
        p = fix(p)
        if os.path.isdir(p):
            shutil.rmtree(p)
    # full.py's command reads "<dir>"/GeoDmsRun.exe; quote the whole path and start it without a shell
    cmd = fix(exp.command).replace(f'"{exe_dir}"/GeoDmsRun.exe', f'"{exe_dir}/GeoDmsRun.exe"')
    t0 = time.time()
    rc = subprocess.call(cmd, env=env)
    print(f"{tag} {arg} rc={rc} secs={time.time() - t0:.0f}", flush=True)
for f in sorted(glob.glob(f"{d}/result/*.result.json")):
    vals = [m['value'] for m in json.load(open(f))['metrics']]
    print(f"{tag} {os.path.basename(f)} values={vals}", flush=True)
print(f"{tag} DONE", flush=True)
