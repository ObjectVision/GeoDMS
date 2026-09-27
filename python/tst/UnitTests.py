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
    import geodms as geodms_module
    expected_abi_tag = f"cp{sys.version_info.major}{sys.version_info.minor}"
    check(
        expected_abi_tag in os.path.basename(geodms_module.__file__),
        f"loaded ABI-tagged {expected_abi_tag} extension",
    )
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

    # #1279: fail_reason() on an item that has not failed returns the empty string it
    # documents, rather than dereferencing a null ErrMsgPtr and taking the process down.
    check(param_item.fail_reason() == "", "fail_reason() of a valid item is empty")

    const_root = config.const_root()
    valid = const_root.find("/red_items_check/valid")
    check(not valid.is_null(), "found /red_items_check/valid")
    check(valid.update_metainfo(), "update_metainfo() of a valid item returns True")
    check(valid.fail_reason() == "", "a valid item has no fail reason after update_metainfo()")

    # #1279: what makes an item red in the GUI -- meta info only, no calculation.
    broken = const_root.find("/red_items_check/broken")
    check(not broken.is_null(), "found /red_items_check/broken")
    check(not broken.update_metainfo(), "update_metainfo() of a broken item returns False")
    reason = broken.fail_reason()
    check("BestaatNiet_xyz" in reason, f"fail_reason() names the unresolved identifier (got {reason!r})")

    # #1279: a walker skips template bodies, which are inert and must not be updated.
    tmpl = const_root.find("/red_items_check/tmpl")
    check(not tmpl.is_null(), "found /red_items_check/tmpl")
    check(tmpl.is_template() and tmpl.in_template(), "the template item itself reports is_template/in_template")
    in_body = const_root.find("/red_items_check/tmpl/in_body")
    check(not in_body.is_null(), "found /red_items_check/tmpl/in_body")
    check(in_body.in_template() and not in_body.is_template(), "an item in a template body reports in_template only")
    check(not valid.in_template(), "an ordinary item is not in a template")

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
