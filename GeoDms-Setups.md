### Starting from GeoDMS version 8.042, releases/setups can be found at: <https://github.com/ObjectVision/GeoDMS/releases>

## 2022: 8.0 Series

The first half 2022 we've worked on a significant renovation of the GeoDMS, resulting in the 8.0 series. Management of intermediate results
has become more functional, regular tiling is used by default to facilitate memory management by re-using fixed sized blocks as tile
storages. Element-by-element operations are now pipelined (when S3 is set). The CalcCache is no longer used for intermediate storage. The
GeoDms 8 should process almost all GeoDms 7 model configurations with the following issues:

-   formal domain and values units are checked for items that are replaced by their calculation rules in other calculation rules,
    resulting in new but relevant diagnostics.
-   Differences in the regular tiling of domains may result in:
    -   different results for poly2grid and point_in_polygon, although now the results have become independent of the actual tiling, they did depend of the active tiling in the GeoDMS 7 series.
    -   .ffs storages produced with GeoDMS 7 that store tiled attributes without their domain may result in corrupted data when read with GeoDMS 8.

## Older versions

### 2022-10-10: GeoDms Version 8.041

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8041-Setup-x64.exe)

New functionality:

-   large palettes with random colours for the unique values of
    categorical thematic attributes.
-   thematic attributes can now be represented by multiple aspects, work
    in progress.

Fixes:

-   issue 283
-   issue 291
-   issue 309
-   issue 310

### 2022-09-29: GeoDms Version 8.039

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8039-Setup-x64.exe)

Fixes:

-   Issue on Azure with 8.038
-   TableView->Sort on column issue due to inconsistent state when
    setting an index was interrupted by a user message.
-   minor issues and compiler warnings

### 2022-09-26: GeoDms Version 8.038

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8038-Setup-x64.exe)

Fixes:

-   issue 303: dyna_points
-   issue 305: fixed crash
-   ReadElemFlags now work

Known issue:

-   on Azure (with OS Windows Server 2019 Datacenter version 1809)
    starting this version may sometimes result in: "System Error: ...
    Rtc.dll was not found."; GeoDms 8.039 seems not to cause this
    behaviour.

### 2022-09-22: GeoDms Version 8.037

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8037-Setup-x64.exe)

Fixes:

-   issue 297: Diagnostic message now have a better source location
    indication
-   issue 304
-   point_in_ranked_polygon now works

Implements:

-   issue 291: various layout issues in Layer Control symbols

Trello

-   61: hex numbers can now be used in dms syntax, preceded by the
    percent-sign, for example %FFFFFF
-   65: null values and failure reasons have grey text in the TableView

Detail pages:

-   clean-up generic and Explore pages

### 2022-09-14: GeoDms Version 8.035

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8035-Setup-x64.exe)

New functionality

-   issue 291 PenColor arc-layer, WIP.
-   Thread throttling

Fixes:

-   TIFF read issue

### 2022-09-01: GeoDms Version 8.033

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms8033-Setup-x64.exe)

New functions:

-   ClassifyNonzeroJenksFisher as alternative for
    [ClassifyJenksFisher](ClassifyJenksFisher "wikilink")
    classification
-   bg_buffer_point, see [bg_buffer](bg_buffer "wikilink")
-   dyna_point_with_ends, dyna_segment, dyna_segment_with_ends, see
    [dyna_point](dyna_point "wikilink")
-   [point_in_all_polygons](point_in_all_polygons "wikilink"):
    (D1->point, D2->polygon) -> unit<uint32> { first_rel: . -> D1;
    second_rel: . -> D2 }
-   [point_in_ranked_polygon](point_in_ranked_polygon "wikilink"), with
    known issue [<https://geodms.nl/mantis/view.php?id=302>
    302](https://geodms.nl/mantis/view.php?id=302_302 "wikilink"),
    which is fixed in 8.036

Fixes:

-   TIFF read error, ntroduced in 7.412 and hotfixed in 7.414.
-   combine(..., bool), required for vesta50
-   issue 294
-   number of threads in the recursive Unique function is now limited to
    the number of vCPUs.
-   bg_buffer_linestring and bg_buffer_multi_polygon: various quality
    improvements.
-   an issue with tiling SPoint data of which the last tile would pass
    the 2^15 boundary if not cut-off by the domain's range, such as with
    the rtc_10m raster.

GUI

-   raster and feature attributes with negative values are now displayed
    with a more adapted classification and colour scheme, handling
    negative, zero, and positive as
-   Ctrl-Alt-D as hot-key for "Default View".

Known issues:

-   explain value in detail page lacks info on the source of a value
    resulting from union_data.
-   S3 (pipelined tile processing) is off by default when installing the
    GeoDMS on a system where geoDMS was already used. I advise to enable
    S3 from the GeoDmsGui.exe -> Tools -> Options -> Advanced -> check
    '3: Pipelined Tile Operations'.

### 2022-08-29: GeoDms Version 8.031

(withdrawn, download 8.033)

Known issue, solved in 8.033:

-   tiff driver incorrectly determines if a tiff is tiled or striped,
    resulting in not being able to read striped tiffs. Fixed in 8.033.

### 2022-08-19: GeoDms Version 8.027

(withdrawn, download 8.033)

## 2022: 7.400 Series

We concluded 2021 with the 7.400, which is compiled with Visual Studio
2022 to comply to the most recently implemented C++20 language rules.

We started building and using GDAL 3.04 by vcpkg, which includes recent
drivers for GeoPackage, netCDF, but also the openssl that we started
using in 7.326. vcpkg allows us to have a completely working debug and
release build of GDAL 3, which makes debugging easier. See also: [GDAL
update](GDAL_update "wikilink")

### 2022-09-01: GeoDms Version 7.414

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7414-Setup-x64.exe)

Fixes:

-   TIFF read error, possibly introduced since 7.411 or 7.412.
-   hang on MapView:GetCaption, such as in RSL3 without label
    work-around.
-   issues with ArgMax and maxelem; Trello #92.

### 2022-05-31: GeoDms Version 7.412

(withdrawn because of a tiff driver issue that has been resolved in
7.414)

Fixed:

-   issues with writing GeoPackage.
-   Notepad++ syntax file GeoDMS_npp_def.xml is now included in the
    setup. Note: last update of the keyword list is of 03/10/2019.
-   TIFF storage manager now uses LIBTIFF 4.3.0 (latest) of vcpkg with
    BIGTIFF support (was 4.2 )
-   AsItemName

Known issues:

-   tiff driver incorrectly determines if a tiff is tiled or striped,
    resulting in not being able to read striped tiffs. Fixed in 8.033.

### 2022-04-22: GeoDms Version 7.411

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7411-Setup-x64.exe)

New in 7.411:

-   operator [AsItemName](AsItemName "wikilink")
-   operator [ReadElems](ReadElems "wikilink") now has formatting flags
    to allow strings to contain spaces, tabs and/or comma's.

Fixed:

-   (partially) issues with
    [polygon_connectivity](polygon_connectivity "wikilink"),
    [bg_buffer](bg_buffer "wikilink"); see also: [mantis issue 265 sub
    1](https://geodms.nl/mantis/view.php?id=265).
-   Default ColorPalette is now redesigned and determined per MapView to
    produce a more consistent coloring of binary rasters and
    non-thematic feature layers.

GDAL 3:

-   fixed reading non-string attributes from a .CSV file.
-   fixed writing to geojson and gml
-   added reading Bool, UInt2, and UInt4 rasters
-   added reading multiple band raster-data into a composite UInt32
    raster.

### 2022-03-11: GeoDms Version 7.410

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7410-Setup-x64.exe)

New operators in 7.410:

-   outer_polygon(polygon attr(poly)); resulting in polygons without
    lakes
-   outer_multi_polygon(multi_polygon attr(poly)); resulting in
    multi_polygons without lakes

New operators in 7.409:

-   bg_simplify_linestring(linestring attr(arc), maxTolerance: Float64)
-   bg_simplify_multi_polygon(polygon attr(poly), maxTolerance: Float64)
-   bg_buffer_linestring(linestring attr(arc), distance: Float64,
    nrCornersInCircle: UInt8);
-   bg_buffer_multi_polygon(lpolygon attr(arc), distance: Float64,
    nrCornersInCircle: UInt8);
-   bg_buffer_multi_point(multi_point attr(sequence), distance: Float64,
    nrCornersInCircle: UInt8);

GeoDmsRun

-   additional command line option to redirect text output, such as
    statistics results, to a specific file

Breaking changes:

-   removed operator: bg_simplify(), introduced in 7.408
-   some layer attributes, when read from a GDAL3 vector source are read
    as a smaller GeoDms integer type than in 7.408

Fixed:

-   a failure to write multiple attributes to a layer with GDAL does not
    cause the GeoDms to not close.

### 2022-03-08: GeoDms Version 7.409

7.409 has been revoked because of the issue with writing multiple layers
to a GeoPackage that seems to result in only the last layer containing
data. This did not occur in 7.408 and is fixed in 7.410.

### 2022-02-18: GeoDms Version 7.408

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7408-Setup-x64.exe)

New in 7.408:

-   operator bg_simplify(line or polygon attribute, maxTolerange:
    Float64); See Trello #50
-   statistics opvragen via GeoDmsRun en nu met Frequency count voor
    value types tot en bet 16 bits, Trello #19
-   [AsHex](AsHex "wikilink") werkt nu ook voor UInt4, UInt8, UInt16,
    UInt64

GDAL:

-   see [GDAL update](GDAL_update "wikilink"), especially improved
    support for writing to:
    -   [csv](csv "wikilink")
    -   [Geopackage](Geopackage "wikilink")

Fixed:

-   performance issue met connect_info, Trello #45
-   Start en eindtijden rekenproces in .xml met export meta info, Trello
    #18
-   [issue: 259](https://www.geodms.nl/mantis/view.php?id=259)
-   [issue: 233](https://www.geodms.nl/mantis/view.php?id=233)
-   [issue: 110](https://www.geodms.nl/mantis/view.php?id=110)
-   [issue 152](https://www.geodms.nl/mantis/view.php?id=152)
-   [issue: 59](https://www.geodms.nl/mantis/view.php?id=59)
-   Trello #46
-   [issue: 244](https://www.geodms.nl/mantis/view.php?id=244)
-   [issue: 41](https://www.geodms.nl/mantis/view.php?id=41)

### 2022-01-26: GeoDms Version 7.407

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7407-Setup-x64.exe)

Fixes an issue with GDAL that might delete existing source data due to
an unintended attribute write attempt.

GDAL:

-   storage types gdalwrite.vect and gdalwrite.grid that can be used
    when data is intended to be exported whereas storage managers of
    type gdal.grid and gdal.vect now only allow for reading data from an
    external source.
-   reading and writing subbyte elements: Bool, UInt2 and UInt4 are now
    supported.

### 2022-01-21: GeoDms Version 7.406

GeoDms x64 installation: revoked because of the gdal source data at risc
issue, update to 7.407 or later.

Implements

-   issue 218: now SessionStartTime and CurrentTime are written into
    outputmetainfo.xml files
-   issue 246: generate item statistics from GeoDmsRun.exe; known issue
    <https://geodms.nl/mantis/view.php?id=254> will be fixed in 7.407

Fixes

-   issue 248: "Check Failed Error: item->DataInMem()"
-   issue 251:
-   sporaric recursive cs-lock on operationcontext clean-up which caused
    program termination
-   InterestCount leak in exceptional AbstrCalculator construction

Diagnostics:

-   report "This seems to be a GeoDms internal error; ..." where
    appropriate

gdal

-   usage of specified GDAL_Driver, GDAL_Options and
    GDAL_LayerCreateOptions
-   vcpkg upgrade to update to GDAL 3.4.0, released 2021/11/04

gdal.grid

-   support of reading sub-datasets as multiple rasters
-   reading multiple datasets from a netCDF file

gdal.vect

-   read/write support for int64 data
-   write support for .csv data and .dbf data; known issue: numeric
    output is quoted in a resulting .csv file.

### 2022-01-10: GeoDms Version 7.404

GeoDms x64 installation: revoked because of the gdal source data at risc
issue, update to 7.407 or later.

Furthermore, now also arc and polygon feature data can be written with
GDAL 3.

new GDAL 3 functions, see also [GDAL update](GDAL_update "wikilink"):

-   writing point features and attributes to various GDAL vector targets
-   writing raster data to TIFF via GDAL
-   reading GeoPackage data, fixed issue
    <https://geodms.nl/mantis/view.php?id=152>
-   reading OpenStreetMap's [pbf
    data](https://wiki.openstreetmap.org/wiki/PBF_Format), fixed issue
    [<https://geodms.nl/mantis/view.php?id=244>](https://geodms.nl/mantis/view.php?id=152)

<!-- -->

-   using the DialogData value as values unit for all loaded geometries
    of tables found in a storage that contains multiple tables/layers.

## 2021: 7.300 Series

In 2021, we started with the 7.300 series to indicate that the GeoDMS
code is now being compiled with Visual Studio 2019 (the 7.2xx series
were built with VS2017) to comply to the most recently implemented C++20
language rules.

### 2021-07-12: GeoDms Version 7.326

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7326-Setup-x64.exe)

Same as 7.325 except for:

-   fix of issue 240: WMTS background layers are now (only) read from
    https, using openssl. This change was necessary because the Dutch
    nationaal georegister stopped serving tiles through http.
-   fix of issue 242: handling of a transparency channel (alpha) in WMTS
    tiles.

### 2021-03-12: GeoDms Version 7.325

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7325-Setup-x64.exe)

Same as 7.324 except for:

-   a fix of an issue with the potential function results, see
    [241](https://geodms.nl/mantis/view.php?id=241) and
    [239](https://geodms.nl/mantis/view.php?id=239)
-   a fix of an access violation in case of failure to read data for a
    wms background layer by http. A mechanism for utilizing htts with
    openTLS is expected for GeoDMS 7.326. see issue
    [240](https://geodms.nl/mantis/view.php?id=240)

### 2021-24-11: GeoDms Version 7.324

GeoDms x64 is withdrawn because of issues with the potential function,
see [241](https://geodms.nl/mantis/view.php?id=241) and
[239](https://geodms.nl/mantis/view.php?id=239)

Fixes:

-   better implementation of deterministic parallel convolution, aka the
    [Potential with kernel](Potential_with_kernel "wikilink") function.
    See issues [232](https://geodms.nl/mantis/view.php?id=232) and
    [239](https://geodms.nl/mantis/view.php?id=239)
-   sporadious deadlocks during a long chain of batch operations,
    specifically seen in LuisaBees sensitivity scripts, see issue
    [132](https://geodms.nl/mantis/view.php?id=132)
-   management ownership of future calculation results (aka interest
    count holders) which caused order of operation issues such as
    [141](https://geodms.nl/mantis/view.php?id=141)
-   code simplifications:
    -   ItemLocks no longer use cs_lock_map but directly own
        (calculated) data, provided by realised future calculation
        results.
    -   concurrency::unbounded_buffer replaced by std::counted_semaphore
    -   made critical sections faster and simpler (and less reentrant!)
        by separating resource disconnection (still inside a critical
        section) from resource deallocation (moved outside such section)
        by better usage of move or swap operations.
-

### 2021-11-11: GeoDms Version 7.321

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7321-Setup-x64.exe)

-   More regular default tile sizes, update on the changes in 7.317.
    This fixes issue 238 where overlapping tiling was created since
    7.317.
-   GeoDmsRun.exe now sends to STDOUT ( std::cout ) for display in
    console all output that is also sent to an optional logfile and that
    GeoDmsGui.exe also displays in the EventLog; one can redirect STDOUT
    to a logfile as alternative to the /L command line option; Fatal
    errors are still sent to STDERR (std::cerr ).

### 2021-11-08: GeoDms Version 7.320

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7320-Setup-x64.exe)

-   transfer of future result ownership from a operation to its awaiters
    is now protected by transfering ArgRefs as optional reference count
    holders with RAII.
-   with /S1, the[Potential](Potential "wikilink") operator now
    calculates tiles Multi Threaded again (this was stalled since 7.315)
    but now with a deterministic result
-   furthermore identical to 7.319.

### 2021-10-28: GeoDms Version 7.319

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7319-Setup-x64.exe)

-   reading a limited extent from a TIF data source caused all GridData
    to be read, causing large memory resource usage for projects such as
    LUISA and 2UP where country data is read from larger TIF data.
-   furthermore identical to 7.318.

### 2021-10-27: GeoDms Version 7.318

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7318-Setup-x64.exe)

-   sporadic halting, issues 132 and 191, is presumably fixed. The
    sporadic halting issue has been seen in Luisa, Land Use Scanner, LUS
    demo, and 2UP. Please report to us if seen still with this or later
    versions.
-   furthermore identical to 7.317.

### 2021-10-26: GeoDms Version 7.317

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7317-Setup-x64.exe)

-   code signed by "Object Vision B.V."
-   Changed tiling/segmenting for large entities; tile sizes are now
    more equal for different sized entities
-   With MT1 on, transformations on separable segments larger than 4096
    elements are processed parallel in sub-segments of 4096 items each.
-   [Point in polygon](Point_in_polygon "wikilink") is now independent
    of the order and tiling of the array of points. Results were
    dependent on that order for points that were in multiple
    (overlapping) polygons.
-   the [Unique](Unique "wikilink") and [Rlookup](Rlookup "wikilink")
    operations now also work with numeric and point sequences as values.
-   now the [StatusFlags](StatusFlags "wikilink") can also be set as
    command line flag when starting the [GeoDMS
    GUI](GeoDMS_GUI "wikilink")

Breaking changes:

-   Results of [Point in polygon](Point_in_polygon "wikilink") when
    polygons overlap
-   stored .fss results for tiled entities without decoupled entity
    definition and resulting segmentation need to be reproduced.

### 2021-10-12: GeoDms Version 7.315

(withdrawn)

### 2021-10-08: GeoDms Version 7.314

GeoDms x64 installation, click
[here](https://www.geodms.nl/downloads/Setup/GeoDms7314-Setup-x64-OS.exe)

-   code signed by "Open Source Developer, Martinus Hilferink"

New calculation rule syntax:

-   [UTF-8](wikipedia:UTF-8#:~:text=UTF%2D8%20is%20a%20variable,Transformation%20Format%20%E2%80%93%208%2Dbit.&text=Code%20points%20with%20lower%20numerical,are%20encoded%20using%20fewer%20bytes. "wikilink")
    character support in [tree item names](Tree_item_name "wikilink")

<!-- -->

-   arrow operator (->) to index an [attribute](attribute "wikilink")
    in the [NameSpace](NameSpace "wikilink")of the [values
    unit](values_unit "wikilink") of that index, i.e.:
    classifications/x/name\[x_rel\] can now be written as x_rel->name.

<!-- -->

-   [And](And "wikilink") and [Or](Or "wikilink") as binary infix
    operators as alternative for && and \|\|.

New functions:

-   EXEC_EC that executes an external process and returns its ExitCode,
    which enables a modeller to use that result in for example the
    construction of a storage name of a source that can only be read
    after completion of that process.

Fixed:

-   issue with maxDist in [Connect](Connect "wikilink"), [Connect
    info](Connect_info "wikilink") and dist_info.

<!-- -->

-   issues related to WMTS support for LambertEA europe and LatLong

<!-- -->

-   hanging threads with MT2 on

<!-- -->

-   with /S1, the results of [Potential](Potential "wikilink") were not
    exactly deterministic as it incorrectly relied on the associativity
    of floating point addition of the overlapping tile results.

Performance improvements:

-   ShadowTile generation (used in indexed [Lookup](Lookup "wikilink"))
    and [Union data](Union_data "wikilink") are now done in parallel
    when /S1 is on and the element type is separable (i.e. fixed size,
    thus not string or sequence, and not sharing bits in the same byte,
    thus not Bool, UInt2 nor UInt4).

BREAKING CHANGE:

[ExplicitSuppliers](ExplicitSuppliers "wikilink") can no longer be used
to control the execution of external processes such as with Exec('dir
\>\> filelist') to prepare for internal value calculations. For this,
now use EXEC_EC, which returns 0 on success or an errorcode in order to
guarantee its derivation before usage.

This affects for example the BAG Toolkit; Use version 7.326 or later for the BAG Toolkit.

### 2021-07-20: GeoDms Version 7.313

[GeoDms x64 installation code-signed by "Open Source Developer, Martinus
Hilferink"](https://www.geodms.nl/downloads/Setup/GeoDms7313-Setup-x64.exe)

Implemented:

\- issue 59: writing available projection info to a .PRJ file when
writing a .SHP file

\- issues 198 and 217: take the current Clipboard contents and scroll in
TableView to the specified row-number (Ctrl-G) or search-key (Ctrl-F)
and in MapView to the specified coordinate.

Fixed:

\- issue 219: use GridLayer with basegrid for raster subset domains.

### 2021-06-10: [GeoDms Version 7.312 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7312-Setup-x64.exe)

Implemented connect_info and dist_info functions that divide the tiles
of points over different worker threads and that can take a maximum
square distance per point to avoid connecting remote locations.

Fixed issue 213 that gave error to the result of a
unique(float32(round(some attribute with values with a metric))).

### 2021-05-13: [GeoDms Version 7.311 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7311-Setup-x64.exe)

Implemented join_equal_values functions , see
<https://geodms.nl/mantis/view.php?id=107>

Fixed Help url issue <https://geodms.nl/mantis/view.php?id=199>

### 2021-05-06: [GeoDms Version 7.310 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7310-Setup-x64.exe)

Fixed: issue <https://geodms.nl/mantis/view.php?id=195>

### 2021-02-24: [GeoDms Version 7.308 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7308-Setup-x64.exe)

Fixed issue 141: MT issues with Stored Properties, such as DialogData,
StorageReadOnly, SqlString.

Now all Stored Properties are read from the main thread and kept private
as task related context.

### 2021-02-23: [GeoDms Version 7.307 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7307-Setup-x64.exe)

Architecture

-   argmin, argmax, min_elem, max_elem are now implemented with internal
    operators (and no longer depend on RewriteExpr.lsp)

New operators:

-   argmin_uint8, agmax_uint8, argmin_uint16, argmax_uint16

Fixed issues:

-   argmin, argmax with 1 argument now results in the correct domain

### 2021-02-01: [GeoDms Version 7.306 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7306-Setup-x64.exe)

Fixed issues:

-   issue 174: connect_info with a uint8 domain for the arc/polygon set
    to connect to now works.

### 2021-02-01: [GeoDms Version 7.305 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7305-Setup-x64.exe)

New operations:

-   subset operations that avoid the creation of `nr_OrgEntity`:
    -   `select_unit(`*`condition`*`)`,
    -   `select_data(select_unit(`*`condition`*`: `**`D`**`->bool) `*`orgData`*`: `**`D`**`->`**`V`**`)`
        which runs through *condition* and picks-up the *orgData*
        elements that correspond with condition elements that are true
        without the use of an intermediate *org_rel* (formerly known as
        *nr_OrgEntity*).
-   subset operations that result in `org_rel` as a replacement of
    `nr_OrgEntity`: `select_orgrel(`*`condition`*`)`
-   subset operations that optimize for expected limitation of the
    cardinality and ordinals of the result set:
    -   `select_unit_uint8(`*`condition`*`)`,
        `select_unit_uint16(`*`condition`*`)`,
        `select_unit_uint32(`*`condition`*`)`
    -   `select_orgrel_uint8(`*`condition`*`)`,
        `select_orgrel_uint16(`*`condition`*`)`,
        `select_orgrel_uint32(`*`condition`*`)`
-   (partitioned) summation operations with specified value types:
    -   `sum_(|u)int(8|16|32|64)(`*`values`*`: `**`D->V`**`[, `*`partition`*`: `**`D->P`**`])`,
    -   `sum_float64(...)`,
    -   `sum_(i|u|d)point(...)`,
-   partitioned counting operations that optimize for expected
    limitation of the cardinality and ordinals of the results:
    -   `pcount_uint(8|16|32)(`*`partition`*`: `**`D->P`**`): `**`P->`**`uint(8|16|32)`,

Fixed issues:

-   [issue 182](https://www.geodms.nl/mantis/view.php?id=182): a domain
    with a single tile that didn't cover the defined range resulted in
    inconsistent data size handling. Now, if NrTiles = 1 and tile\[0\]
    strictly smaller than range then GetCount() = #range, but
    GetTileCount(0) is #tile, and data is only calculated and used for
    the non-covering tile.
-   [issue 183](https://www.geodms.nl/mantis/view.php?id=183): using
    unknown_item

### 2021-01-25: [GeoDms Version 7.304 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7304-Setup-x64.exe)

Upgrades:

-   build environment: MSVC 2019 (was: MSVC 2019)
-   compiler: C++ 14.2 (was 14.1)
-   platformtoolset: v142
-   LanguageStandard: stdcpplast
-   boost: 1.75.0 (was: 1.69.0)

Code clean-up:

-   the use of boost::mpl and type_traits have been replaced by
    equivalent elements of namespace std
-   fixes related to the more strict C++20 language rules.

Feature request:

-   [issue 172](https://www.geodms.nl/mantis/view.php?id=172): GUI
    MapView controls: Panning and feature-info have become the default
    mouse actions; the related Buttons in the toolbar have been removed.

Fixed issues:

-   [issue 176](https://www.geodms.nl/mantis/view.php?id=176):
    Synchronisation of data read from .fss with reading domain
    cardinality from the same .fss
-   [issue 181](https://www.geodms.nl/mantis/view.php?id=181): not
    operator (issue introduced in 7.300 as a result of new aggregation
    operations

## Versions of 2020

### 2020-12-03: [GeoDms Version 7.238 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7238-Setup-x64.exe)

### 2020-10-23: [GeoDms Version 7.234 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7234-Setup-x64.exe)

### 2020-10-05: [GeoDms Version 7.229 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7229-Setup-x64.exe)

Fixes issues related to processing many red items and ExplainValue
tooling: 1429, 1433 GUI issues: 1326, 1429

MapView Toolbar: now default mouse functions:

-   LeftButtonClick -> SetFocus in active layer and all raster layers,
    information on focus item is presented in legend
-   Drag -> Map Panning,
-   LeftButtonDoubleClick -> ExplainValue of focus element in active
    layer, see Detailpage
-   InfoTool is now removed.
-   ZoomIn, ZoomOut and most selection Tools replace the Drag function,
    but leave LeftButtonClick and LeftButtonDoubleClick unchanged. The
    SelectObject Tool does disable the SetFocus function when active.

TableView Toolbar:

-   InfoTool now works as a command to ExplainValue on the Focus
    element.

Fixed issues for Vesta.

### 2020-09-04: [GeoDms Version 7.222 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7222-Setup-x64.exe)

Fixes issues 1414, 1415, 1416, 1417, 1419, Fixed issues for Vesta.

### 2020-08-04: [GeoDms Version 7.220 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7220-Setup-x64.exe)

Fixes issues 1411, 1413

Fixes issues with cancel working thread, GetLabelAttr values and Value
Info Detail Page.

Warning: we're working on a found issue in the Vesta Regression test
causing an Access Violation. Do not use 7.220 for Vesta for now.

### 2020-04-08: [GeoDms Version 7.215 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7215-Setup-x64.exe)

Contains various fixes of GeoDMS 7.213: Fixes issues 1380, 1382, 1325,
1360 Fixes Progress Messaging

### 2020-03-16: [GeoDms Version 7.213 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7213-Setup-x64.exe)

Contains various fixes of GeoDMS 7.212.

### 2020-03-11: [GeoDms Version 7.212 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7212-Setup-x64.exe)

New features:

-   Gui: F2 helps to trace the source of errors
-   dijkstra_m for Network travel time matricices: a logit based decay
    function can now be specified for intermediate aggregation

Regression tests

-   includes Vesta and different Multi Threading setttings.

Dataset size:

-   better support for datasets with more than 2^32 rows, more
    specifically, rowset size issues were solved for merge,
    raster_merge, dijkstra_m, and aggregations with more than 2^32
    partitions.

Diagnostics

-   better detection and reporting when values or row-counts
    unexpectedly exceed 2^32.

Fixes many MT2 and dijkstra issues ...

-   Fixed GeoDmsGui crashes, see: [issues
    1347](http://www.mantis.objectvision.nl/view.php?id=1347),
    [1348](http://www.mantis.objectvision.nl/view.php?id=1348),
-   Fixed Delphi error in GeoDmsGui crashes, see: [issues
    1347](http://www.mantis.objectvision.nl/view.php?id=1347)
    [348](http://www.mantis.objectvision.nl/view.php?id=1348)
-   Fixed "Failed to generate' en 'access violation' error, see: [issue
    1373](http://www.mantis.objectvision.nl/view.php?id=1373)
-   Dijkstra issues: various changes and fixes have been made in the
    GeoDMS to accommodate OD pair sets with more than 2^32 rows,

see: [issue 1362](http://www.mantis.objectvision.nl/view.php?id=1362)

-   GDAL 2 fixes and improvements, see [issues
    1368](http://www.mantis.objectvision.nl/view.php?id=1368) and
    [issues 1369](http://www.mantis.objectvision.nl/view.php?id=1369)

The boost::polygon library is now included in our subversion repository
and some initiary investigations and updates were made as preparation of
work as discussed at [issue
1208](http://www.mantis.objectvision.nl/view.php?id=1208).

### 2020-01-08: [GeoDms Version 7.207 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7207-Setup-x64.exe)

Added operators: subset_uint8, subset_uint16, subset_uint32

Fixes minor issues 1319, 1329.

## Archive 2016..2019

Some documented versions of earlier years can be found on the page with
[Older GeoDMS versions](Older_GeoDMS_versions "wikilink")