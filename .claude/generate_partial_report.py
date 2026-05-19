"""One-shot: generate the regression-test HTML report from existing .bin
files without re-running any test. Used after we killed full.py mid-run
on 2026-05-12 so the user can read the partial .l results before fixing
the Linux Bokeh sampler (issue #1104)."""
import sys
import importlib.util

profiler_path = r"C:/dev/GeoDMS_2026/profiler/profiler.py"
regression_path = r"C:/dev/GeoDMS_2026/profiler/regression.py"

def _import(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

profiler = _import("profiler", profiler_path)
regression = _import("regression", regression_path)
# regression.py reaches into a global `profiler` symbol it expects already imported
regression.profiler = profiler

result_paths = {
    "results_base_folder": r"C:/dev/tst/Regression/GeoDMSTestResults",
    "results_folder": r"C:/dev/tst/Regression/GeoDMSTestResults/20_0_0_l_x64_SF_S1S2S3_OVSRV10",
}

regression.collect_and_generate_test_results("20.0.0.l", result_paths)
