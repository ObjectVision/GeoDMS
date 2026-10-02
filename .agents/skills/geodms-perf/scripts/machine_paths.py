# Where this machine keeps the GeoDMS-Test harness and full.py's result folders, for the scripts beside it.
#
# TST_DIR       the GeoDMS-Test working copy: %TstDir% (the name full.py and batch\RunGUITests.bat give it),
#               default C:/dev/tst (OVSRV10; on OVSRV05 a junction to C:/dev/ProjDir_Jip/GeoDMS-Test that is
#               not always there)
# RESULTS_BASE  the folder that holds full.py's <label> result folders: %ResultsBaseDir%, else the
#               ResultsBaseDir of <TST_DIR>/batch/local_settings.json resolved as full.py resolves it, else
#               OVSRV10's C:/LocalData/GeoDMS-Test/Regression
import os, json

def _slash(p):
    return p.replace('\\', '/').rstrip('/')

TST_DIR = _slash(os.environ.get('TstDir') or 'C:/dev/tst')

def _results_base():
    if os.environ.get('ResultsBaseDir'):
        return _slash(os.environ['ResultsBaseDir'])
    settings = f"{TST_DIR}/batch/local_settings.json"
    if not os.path.isfile(settings):
        return 'C:/LocalData/GeoDMS-Test/Regression'
    with open(settings, encoding='utf-8') as f:
        s = json.load(f)
    if 'ResultsBaseDir' not in s:
        return 'C:/LocalData/GeoDMS_Test_Results'  # full.py's built-in default
    # a blank one keeps the results in the working copy (regression.get_result_paths)
    return _slash(s['ResultsBaseDir']) if s['ResultsBaseDir'] else f"{TST_DIR}/Regression/GeoDMSTestResults"

RESULTS_BASE = _results_base()

def harness_on_path(sys_path):
    """Put the harness's batch folders first on sys_path; the pickled Experiment class is batch/generic/profiler.py."""
    if not os.path.isfile(f"{TST_DIR}/batch/generic/profiler.py"):
        raise SystemExit(f"no GeoDMS-Test harness at {TST_DIR}: set TstDir to its working copy")
    sys_path[:0] = [f"{TST_DIR}/batch/generic", f"{TST_DIR}/batch"]
