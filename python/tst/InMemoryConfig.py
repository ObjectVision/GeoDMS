# Demonstrates building a complete GeoDMS configuration in memory (no .dms model
# script) and querying results via Primary Data Access, using the geodms module.
#
# Resolves the geodms module from the GEODMS_PYDIR environment variable when set (else
# from the dev-tree bin dir relative to this script). Exits 0 on success / 1 on failure.
import os
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
geodms_path = os.environ.get('GEODMS_PYDIR') or os.path.abspath(os.path.join(script_dir, '..', '..', 'bin', 'Release', 'x64'))
sys.path.append(geodms_path)
if hasattr(os, 'add_dll_directory') and os.path.isdir(geodms_path):
    os.add_dll_directory(geodms_path)
os.environ['PATH'] += os.pathsep + geodms_path

try:
    from geodms import *
except Exception as e:
    print(f"InMemoryConfig.py: FAIL: cannot import geodms: {e}")
    sys.exit(1)

print(f"geodms version: {version()}")

engine = Engine()

# --- build a configuration from scratch, without a model script ----------------
config = engine.create_config_root('demo')
root = config.root()

# 1) a parameter and setting its value
nr_param = root.add_param('nrEntries', 'uint32')
nr_param.set_param_int(5)

pi = root.add_param('pi', 'float64')
pi.set_param_float(3.14159)

label = root.add_param('label', 'string')
label.set_param_str('hello from python')

# 2) a domain unit sized by an expression, plus a primary-data attribute over it.
#    A domain's element count comes from its calculation rule; range(uint32, 0, n)
#    defines an n-element domain.
entities = root.create_unit('Entity', 'uint32')
entities.set_expr('range(uint32, 0, 5)')

values = root.add_attribute('value', entities.asConst(), 'float64')
values.set_values_from_float_list([1.0, 2.0, 3.0, 4.0, 5.0])

# 3) a derived attribute defined purely by an expression (no script file)
doubled = root.add_attribute('doubled', entities.asConst(), 'float64')
doubled.set_expr('value * 2.0')

total = root.add_param('total', 'float64')
total.set_expr('sum(value)')

# --- query results via Primary Data Access -------------------------------------
print(f"nrEntries  = {nr_param.get_param_float()}")
print(f"pi         = {pi.get_param_float()}")

total.update()
print(f"total      = {total.get_param_float()}")

doubled.update()
print(f"value      = {values.asDataItem().get_values_as_float_list()}")
print(f"doubled    = {doubled.asDataItem().get_values_as_float_list()}")

# --- query / inspect the configuration tree ------------------------------------
# the kind of an item is derived from isUnitItem() / isDataItem(), and described
# through its value type / value composition (no runtime-class introspection needed).
print("\nconfiguration tree:")
for item in root.sub_items():
    if item.isUnitItem():
        u = item.asUnitItem()
        print(f"  {item.name():12s} [unit] value_type={u.value_type_id().name}")
    elif item.isDataItem():
        d = item.asDataItem()
        descr = f"{d.domain_unit().value_type_id().name} -> {d.values_unit().value_type_id().name}"
        print(f"  {item.name():12s} [attr] {descr}, vc={d.value_composition().name}")
    else:
        print(f"  {item.name():12s} [container]")

# --- verify the computed results -----------------------------------------------
try:
    assert nr_param.get_param_float() == 5.0
    assert abs(pi.get_param_float() - 3.14159) < 1e-9
    assert total.get_param_float() == 15.0
    assert values.asDataItem().get_values_as_float_list() == [1.0, 2.0, 3.0, 4.0, 5.0]
    assert doubled.asDataItem().get_values_as_float_list() == [2.0, 4.0, 6.0, 8.0, 10.0]
except AssertionError as e:
    print("InMemoryConfig.py: FAIL")
    sys.exit(1)

print("InMemoryConfig.py: PASS")
sys.exit(0)
