*[[Expression]] fast calculations: Programming Architecture*

## programming architecture

-   All data is managed in arrays and data operations are vectorized.
-   Large arrays are segmented (or tiled in case of 2d raster data) in order to parallel process data segments in separate threads, aka MT1
-   Different independent operations are executed in parallel, aka MT2, [Multi Threading](http://en.wikipedia.org/wiki/Multithreading_29#Multithreading),
-   interest counting and a configurable CalcCache for storing intermediate results for as long as needed, but not longer.
-   Use of [C++ templates](http://en.wikipedia.org/wiki/C2B_Templates), [STL](http://en.wikipedia.org/wiki/Standard_Template_Library), [BOOST](http://en.wikipedia.org/wiki/Boost_(C2B_libraries)), [GDAL.](http://en.wikipedia.org/wiki/GDAL)
-   Management of run time properties of datasets to select the fastest algorithms, such as [counting-sort](http://en.wikipedia.org/wiki/Counting_sort) and comparable methods for [[modus]], join etc.
-   Retain intermediate resources for reuse in repeatedly called functions, such as in: [[poly2grid]].
-   Usage of Intel's Math Kernel Library for fast convolution, boost::uBlas for matrix operations.
-   calculations can be segmented functionally and their results merged afterwards to run on multiple machines.

Under Study:

-   C++ AMP for deploying GPU's see <http://wiki.objectvision.nl/index.php/Parallel_Processing_and_GPU_Acceleration>
    and <https://docs.microsoft.com/en-us/cpp/parallel/amp/cpp-amp-overview?view=vs-2019> GeoDMS is now compiled witt VS 2017.
-   virtual data segments with segment level ownership and lifetime, aka MT3
-   a RUST like unique write and shared read owners of intermediate results, [[value based calculating]].