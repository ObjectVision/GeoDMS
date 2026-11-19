# geodms-globio

This package is the GLOBIO flavour of [geodms](https://pypi.org/project/geodms/): the GeoDMS engine as a Python module, built against the GDAL, PROJ and GEOS of the conda environment of the [GLOBIO 4 model](https://github.com/GLOBIO4/GlobioModelPublic), so that in a GLOBIO script GeoDMS reads and writes data with the same GDAL version as the `osgeo` package of that environment. [GeoDMS](https://www.geodms.nl) is an open-source platform for spatial models over large grid, vector and attribute data, written as configurations in its own `.dms` language. For any other use, install `geodms`.

```
pip install geodms-globio
```

There is one wheel, for 64-bit Windows and CPython @PYTHON_VERSION@, the Python of the GLOBIO 4 environment. It holds the engine and the libraries it uses, copied from that environment (GDAL @GDAL_VERSION@, PROJ @PROJ_VERSION@, GEOS @GEOS_VERSION@ and others), under names of their own. The GDAL and PROJ data and the GeoDMS `library` folder are included. So `osgeo` and `geodms` can be imported in either order, each with its own copy of the same GDAL version, also when the conda environment is not activated.

The package installs the module `geodms`, as `geodms` does; install one of the two in an environment, not both.

```python
from osgeo import gdal
import geodms

engine = geodms.Engine()                         # one per process
with engine.load_config("model.dms") as config:  # closes the configuration at the end of the block
    item = config.const_root().find("/results/total").asDataItem()
    item.update()                                # calculates the item and what it depends on
    print(item.get_value_as_float(0))
```

The functions of the module are described on the wiki page [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings). The source is on [GitHub](https://github.com/ObjectVision/GeoDMS). GeoDMS is licensed under the GPL, version 3 or later; the licence texts of the bundled libraries are in the `.dist-info/licenses` folder of the wheel.
