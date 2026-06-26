# GeoDMS python-bindings unit test: load a configuration, navigate the item tree,
# change an expression and (re)calculate a dependent result, and read values back.
#
# Runs non-interactively (suitable for batch/CI): it resolves the geodms module from
# the GEODMS_PYDIR environment variable when set (else from the dev-tree bin dir relative
# to this script), prints PASS/FAIL, and exits 0 on success / 1 on failure.
import os
import sys

print('Geodms python test module')
print(f"{sys.version}")

script_dir = os.path.dirname(os.path.abspath(__file__))
geodms_path = os.environ.get('GEODMS_PYDIR') or os.path.abspath(os.path.join(script_dir, '..', '..', 'bin', 'Release', 'x64'))
print(f"geodms module dir: {geodms_path}")
sys.path.append(geodms_path)
if hasattr(os, 'add_dll_directory') and os.path.isdir(geodms_path):
    os.add_dll_directory(geodms_path)
os.environ['PATH'] += os.pathsep + geodms_path

config_file = os.path.join(script_dir, 'basic_data_test.dms')


def check(condition: bool, message: str):
    if not condition:
        raise AssertionError(message)
    print(f"  OK: {message}")


try:
    from geodms import *

    print(f"geodms version: {version()}")

    engine = Engine()

    # load a geodms configuration
    config = engine.load_config(config_file)
    root = config.root()
    check(not root.is_null(), "configuration root loaded")

    # find an existing parameter
    param_item = root.find("/parameters/test_param")
    check(not param_item.is_null(), "found /parameters/test_param")

    # a non-existent path yields a null item (not an error)
    missing = root.find("/does/not/exist")
    check(missing.is_null(), "non-existent path returns a null item")

    # change the parameter expression and (re)calculate a dependent result
    param_item.set_expr("3b")
    result_item = root.find("/export/IntegerAtt")
    check(not result_item.is_null(), "found /export/IntegerAtt")
    result_item.update()
    value = result_item.asDataItem().asDataItem().get_value_as_int(0)
    check(value == 3, f"/export/IntegerAtt[0] == 3 after set_expr('3b') (got {value})")

    # read primary data of a configured attribute
    ints = root.find("/reference/IntegerAtt")
    ints.update()
    values = ints.asDataItem().asDataItem().get_values_as_int_list()
    check(values == [0, 1, 256, -100, 9999], f"/reference/IntegerAtt values (got {values})")

    print("UnitTests.py: PASS")
    sys.exit(0)

except Exception as e:
    print(f"UnitTests.py: FAIL: {type(e).__name__}: {e}")
    sys.exit(1)
