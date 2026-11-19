# geodms

This package makes the GeoDMS engine available as a Python module. [GeoDMS](https://www.geodms.nl) is an open-source platform for spatial models over large grid, vector and attribute data, written as configurations in its own `.dms` language. With this module a Python script loads such a configuration, finds its items, changes an expression and reads the calculated values back, in the same process.

```
pip install geodms
```

There are wheels for CPython 3.12, 3.13 and 3.14 on 64-bit Windows and on 64-bit Linux with glibc 2.39 or newer, such as Ubuntu 24.04, Debian 13, Fedora 40 and RHEL 10. Each holds the engine and the libraries it uses (GDAL, PROJ, GEOS, Arrow and others), the GDAL and PROJ data and the GeoDMS `library` folder. On Windows those libraries carry names of their own, so that they can be loaded in the same process as the GDAL, PROJ and SQLite of QGIS or of Python itself. On Linux GDAL, PROJ and GEOS are linked into the engine; the module works next to an osgeo imported in the usual way, but not in a process that already has a GDAL in its global symbol scope, such as QGIS.

```python
import geodms

engine = geodms.Engine()                         # one per process
with engine.load_config("model.dms") as config:  # closes the configuration at the end of the block
    item = config.const_root().find("/results/total").asDataItem()
    item.update()                                # calculates the item and what it depends on
    print(item.get_value_as_float(0))
```

`config.close()` closes a configuration as well, and the items of a closed configuration raise an exception when they are used. An item keeps its configuration loaded for as long as the script holds it, also without a variable for the `Config`, as in `engine.load_config("model.dms").const_root().find("/results/total")`. A process has one `Engine` and one loaded configuration at a time: `load_config` refuses while the `Config` of the loaded configuration still exists, and closes a configuration that only items still hold.

The functions of the module are described on the wiki page [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings), and [GeoDMS through Python](https://github.com/ObjectVision/GeoDMS/wiki/GeoDMS-through-Python) compares this module with the other ways to combine GeoDMS and Python. The source is on [GitHub](https://github.com/ObjectVision/GeoDMS). GeoDMS is licensed under the GPL, version 3 or later; the licence texts of the bundled libraries are in the `.dist-info/licenses` folder of the wheel.
