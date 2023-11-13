Investigate, describe and compare:

# MultiThreading libraries

-   Parallel Patterns Library (PPL): <http://msdn.microsoft.com/en-us/library/dd492418.aspx>
-   Intel's Threading Building Blocks (TBB): <http://msdn.microsoft.com/en-us/library/dd492418.aspx>

The GeoDms now uses PPL for the implementation of the following two GeoDms options

-   MT1, which enables the parallel (multi-threaded) processing of tiles of the same item.
-   MT2 (experimental), which enables then parellel processing of multiple items.

Both can be enabled or disabled from the Tools->Options dialog under the General Settings tab. These settings are stored in a user/machine
specific registry and affect both the [[GeoDMS GUI]] GUI as the [[GeoDMSRun]]. 

MT1 requires data items to be partitioned into multiple [[tiles|Tile]]. Both options require more memory resources of the system than serial processing and, especially with a Win32 process, this can lead to memory allocation failures which are not always handled smoothly. Work in progres is to support a mechanism that allows for MT1 to allocate all required resources at the start of a tile task (and reuse those resources for the next tasl in queue for that thread) or wait if those resources cannot be made available. For MT2 this doesn't seem feasible but some mechanism must be implemented to stall or at least not start threads when their task tend to exhaust memory resources, especially waiting for a swap-out operation should not cause the threading mechanism to fire new threads, as it occasionally does.

# GPU Hardware acceleration

-   [[Windows GDI|http://msdn.microsoft.com/en-us/library/windows/desktop/dd145203(v=vs.85>).aspx]] has been used to implement the first version of
    [[poly2grid]]. Disadvantage is that all (potentially hardware accelerated) operations are done on Device Dependent Bitmaps, of which the  bit-depth is uncertain, see [Mantis issue 276 on the GDI poly2grid function failure on Citrix with a virtual video driver from Novell due to limited (16) bit-depth of their DDB's](http://www.mantis.objectvision.nl.objectvision.hosting.it-rex.nl/view.php?id=276).
-   CUDA, see <http://en.wikipedia.org/wiki/CUDA> and <http://www.drdobbs.com/parallel/introduction-to-cuda-cc/240143066>: Nice, but depending on NVIDIA Graphics Hardware.
-   OpenCL, see <http://en.wikipedia.org/wiki/OpenCL>:
-   DirectCL
-   C++ Accelerated Massive Parallelism (AMP)
-   AMP is depreciated since VC 2022, presumably due to other developments, such as: <https://en.wikipedia.org/wiki/SYCL>
-   HSA Bolt: <http://www.drdobbs.com/parallel/heterogeneous-programming/240144126> for heterogenous platforms with load balancing.

# dedicated libraries

-   IPP 7.0, see <http://software.intel.com/en-us/intel-ipp>: used for FFT based [[convolution]], which uses multiple cores internally.
-   GDAL offers an alternative for [[poly2grid]], but this will not utilize any multi-threading or hardware acceleration.
-   Bolt: a C++ Template Library for HSA with functions such as bolt::amp::sort and bolt::amp::translate, see:      <http://twimgs.com/ddj/images/article/2012/1212/DrDobbsHeterogeneousComputing.pdf>; requires MSVC 2012.

# MSVC 2012 AMP support

-   <http://msdn.microsoft.com/en-us/library/hh265137.aspx>
-   <http://msdn.microsoft.com/en-us/library/hh388953.aspx>
-   Tile support: <http://msdn.microsoft.com/en-us/library/hh873135.aspx>