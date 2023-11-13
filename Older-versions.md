## Versions 2022

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
-   TableView-\>Sort on column issue due to inconsistent state when
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
    [ClassifyJenksFisher](ClassifyJenksFisher "wikilink") classification
-   bg_buffer_point, see [bg_buffer](bg_buffer "wikilink")
-   dyna_point_with_ends, dyna_segment, dyna_segment_with_ends, see
    [dyna_point](dyna_point "wikilink")
-   [point_in_all_polygons](point_in_all_polygons "wikilink"):
    (D1-\>point, D2-\>polygon) -\> unit<uint32> { first_rel: . -\> D1;
    second_rel: . -\> D2 }
-   [point_in_ranked_polygon](point_in_ranked_polygon "wikilink"), with
    known issue [https://geodms.nl/mantis/view.php?id=302
    302](https://geodms.nl/mantis/view.php?id=302_302 "wikilink"), which
    is fixed in 8.036

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
    S3 from the GeoDmsGui.exe -\> Tools -\> Options -\> Advanced -\>
    check '3: Pipelined Tile Operations'.

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

-   issue 248: "Check Failed Error: item-\>DataInMem()"
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

-   arrow operator (-\>) to index an [attribute](attribute "wikilink")
    in the [NameSpace](NameSpace "wikilink")of the [values
    unit](values_unit "wikilink") of that index, i.e.:
    classifications/x/name\[x_rel\] can now be written as x_rel-\>name.

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

This affects for example the [BAG Toolkit
2.2](BAG_Toolkit_2.2 "wikilink"); Use version 7.326 or later for the
[BAG Toolkit 2.2](BAG_Toolkit_2.2 "wikilink").

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

-   LeftButtonClick -\> SetFocus in active layer and all raster layers,
    information on focus item is presented in legend
-   Drag -\> Map Panning,
-   LeftButtonDoubleClick -\> ExplainValue of focus element in active
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

## Versions of 2019

### 2019-12-04: [GeoDms Version 7.206 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7206-Setup-x64.exe)

This version fixes some issues related to CalcCache resource usage and
various other improvements.

Select GeoDmsGui-\>Tools-\>Options-\>Current Configuration-\>Minimum
size for DataItem specific swapfiles in CalcCache and change the value
from 1000000 to 1000000000 to improve calculation speed for the price of
increased probability of memory overflow related issues. This is
recommended for Windows 10 where memory overflow issues seem to be delt
with well, and not recommended for Windows 2008 server.

Fixes:

-   [issue 1238](http://www.mantis.objectvision.nl/view.php?id=1238):
    domain units with a sub-item named geometry now are automatically
    Mapviewable.
-   [issue 1301](http://www.mantis.objectvision.nl/view.php?id=1301):
    solved memory error with some large data sets.
-   [issue 1305](http://www.mantis.objectvision.nl/view.php?id=1305):
    TableView index error fixed
-   [issue 1307](http://www.mantis.objectvision.nl/view.php?id=1307): No
    more Cannot obtain a Read Lock because there is 1 Write Lock on this
    data
-   [issue 1314](http://www.mantis.objectvision.nl/view.php?id=1314):
    Fixed appcrash
-   [issue 1316](http://www.mantis.objectvision.nl/view.php?id=1316):
    Minimum size for DataItem specific swapfiles is now stored in the
    registry per user, per machine.
-   [issue 1318](http://www.mantis.objectvision.nl/view.php?id=1318):
    Dijkstra now warns when givens Mass and MaxImpedance attributes
    contain NULL values.

### 2019-11-01: [GeoDms Version 7.203 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7203-Setup-x64.exe)

Fixes:

-   Startup issue on Windows Server 2008

### 2019-10-21: [GeoDms Version 7.202 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7202-Setup-x64.exe)

Fixes:

-   [issue 1280](http://www.mantis.objectvision.nl/view.php?id=1280)
    Stack overflow exceptions now result in an error message; Stack size
    has increased for GeoDmsRun.exe
-   [issue 1290](http://www.mantis.objectvision.nl/view.php?id=1290)
    Various other issues related to the display of diagnostic messages.
-   [issue 1205](http://www.mantis.objectvision.nl/view.php?id=1205)
    [1256](http://www.mantis.objectvision.nl/view.php?id=1256)[1294](http://www.mantis.objectvision.nl/view.php?id=1294)
    Display of the Thousand separator is now settable per user and per
    machine in Tools-\>Options. Default is off.

Known issue: On Windows Server 2008 R2, the GeoDMS software might block
during dll-initialisation due to concurrency runtime limitations. This
issue does not occur on Windows 10. For users with Windows Server 2008
R2, install and use GeoDMS 7.203.

### 2019-10-09: [GeoDms Version 7.201 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7201-Setup-x64.exe)

Fixes:

-   [issue 1258](http://www.mantis.objectvision.nl/view.php?id=1258)
    Dijkstra on empty nodeset/linkset
-   [issue 1279](http://www.mantis.objectvision.nl/view.php?id=1279)
    Vesta crashed on Municipal selection, caused by dijkstra on a
    nodeset without links.
-   [issue 1289](http://www.mantis.objectvision.nl/view.php?id=1289)
    Fixed GUI action Update subtree, introduced by 7.200, relative to
    7.182.

### 2019-10-04: [GeoDms Version 7.200 for x64](https://www.geodms.nl/downloads/Setup/GeoDmsSetup/GeoDms7200-Setup-x64.exe)

Fixes various issues introduced since 7.182, such as issues 941, 1184.

Reverted:

-   Yellow items, i.e. not actually copying and retaining cache items as
    endogenous config items by keeping a temporary user perspective.
-   Separate production of signature objects reflecting cache results
    with formal domain and values units based on processing formal
    arguments and actual data result objects.

Reversion of these changes implies that some cache entries are still
identified by expressions containing treeitem names and that for some
operators, some formal arguments cannot be substituted by their related
calculation rules.

### 2019-04-11: GeoDms Version 7.198 (withdrawn, use 7.203 instead)

In this version, the GUI has been stream-lined. Many icons have been
replaced, the toolbar now contains 32x32 pixel icons similar to the
qGis-look, GUI operations to change a model configuration have been
removed (such as the Insert operations) as the use case of building a
model configuration interactively never flew since the syntax of the
configuration files in much more clear to work with.

Implements issues:

-   [1202](http://www.mantis.objectvision.nl/view.php?id=1202): A
    separate GeoDmsCaller.exe is now provided to send GUI control
    messages to an existing instance of GeoDmsGui.exe
-   [1205](http://www.mantis.objectvision.nl/view.php?id=1205): Thousand
    separators have now been implemented in the representation of
    quantities in TableView, LispExpr string representation, statistics,
    etc., but not in Export to Clipboard as .csv.
-   [1206](http://www.mantis.objectvision.nl/view.php?id=1206): WM(T)S
    support now also supports background layers that are made of .jpg
    files.
-   [1207](http://www.mantis.objectvision.nl/view.php?id=1207): The
    SourceDescr tab can now be used to see which storages have been used
    for a certain result or container.
-   [1212](http://www.mantis.objectvision.nl/view.php?id=1212): new
    qGis-style icons.
-   [1217](http://www.mantis.objectvision.nl/view.php?id=1217): display
    of palette icon in tree-view for different color aspect attributes.

Fixes issues:

-   [1164](http://www.mantis.objectvision.nl/view.php?id=1164): gebruik
    van Symbol fonts (zoals: ttf-Wingdings) voor PointLayer
    visualisatie.
-   [1182](http://www.mantis.objectvision.nl/view.php?id=1182): issue
    with connections to arcs with zero length
-   [1191](http://www.mantis.objectvision.nl/view.php?id=1191):
-   [1192](http://www.mantis.objectvision.nl/view.php?id=1192):
-   [1198](http://www.mantis.objectvision.nl/view.php?id=1198):
-   [1213](http://www.mantis.objectvision.nl/view.php?id=1213):
-   [1214](http://www.mantis.objectvision.nl/view.php?id=1214):
-   [1215](http://www.mantis.objectvision.nl/view.php?id=1215):
-   [1220](http://www.mantis.objectvision.nl/view.php?id=1220):
-   [1221](http://www.mantis.objectvision.nl/view.php?id=1221):

### 2019-02-20: GeoDms Version 7.196 (withdrawn, use 7.203 instead)

This release optimizes the use of memory, especially of models that
calculate attributes of the same entities sequentially, such as in a
loop with time steps. For this, large freed memory blocks are retained
and reissued when demanded to avoid the system to wait on synchronized
OS memory recollection.

-   The .tmp part of the CalcCache is no longer in use, instead all
    non-persistent intermediary results are stored in the (system page
    file backed) heap. EmptyWorkingSet is called when in stress.
-   Full Names are no longer stored per item and only composed when
    requested. This greatly reduces the stress on the internal
    Tokenizer.
-   Internal representation of Undefined SharedStr and StringRef made
    more consistent.

Fixed issues related to these changes:

-   issue [http://www.mantis.objectvision.nl/view.php?id=1192
    1192](http://www.mantis.objectvision.nl/view.php?id=1192_1192 "wikilink"):
    lock already taken
-   issue [http://www.mantis.objectvision.nl/view.php?id=1192
    1191](http://www.mantis.objectvision.nl/view.php?id=1192_1191 "wikilink"):
    combine(range(u, xb, xe), range(v, yb, ye)) now works
-   issue [http://www.mantis.objectvision.nl/view.php?id=1024
    1024](http://www.mantis.objectvision.nl/view.php?id=1024_1024 "wikilink"):
    Startup issue on Windows 7.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1183
    1183](http://www.mantis.objectvision.nl/view.php?id=1183_1183 "wikilink"):
    memory allocation errors
-   issue [http://www.mantis.objectvision.nl/view.php?id=1184
    1184](http://www.mantis.objectvision.nl/view.php?id=1184_1184 "wikilink"):
    domain unification error when using ParseXML
-   issue [http://www.mantis.objectvision.nl/view.php?id=1185
    1185](http://www.mantis.objectvision.nl/view.php?id=1185_1185 "wikilink"):
    Layer Control names and unit specs are represented again.

Other Fixed issue:

-   issue [http://www.mantis.objectvision.nl/view.php?id=11177
    1177](http://www.mantis.objectvision.nl/view.php?id=11177_1177 "wikilink"):
    numerical instability in the weight aggregation of
    nth_element_weighted could give undefined results. This could affect
    Fusion, 2UP, RSL and other projects using nth_element_weighted.

### 2019-01-06: GeoDms Version 7.185 (withdrawn, use 7.200 instead)

GeoDMS 7.185 mainly contains fixes of reported issues and has a
mechanism (in debug mode) to detect potential dead-locks by checking a
hierarchy of the critical sections. This has resulted in the removal of
many potential dead-locks.

New functionality:

-   issue [http://www.mantis.objectvision.nl/view.php?id=1092
    1092](http://www.mantis.objectvision.nl/view.php?id=1092_1092 "wikilink"):
    code analysis for clean-up support: now a source can be set, more
    used items are included (such as when used indirectly in
    properties), and representation colors have been improved.

Possible breaking change:

-   issue [http://www.mantis.objectvision.nl/view.php?id=1141
    1141](http://www.mantis.objectvision.nl/view.php?id=1141_1141 "wikilink"):
    Now two domains are considered different if they refer to different
    items, even if their calculation rule is the same. For example:
    unit<uint32> landuse_class: nrofrow=5; unit<uint32> building_type:
    nrofrows=5; are now domain-incompatible. Thus, the entity
    compatibility check is now more strict. Tested projects (and adapted
    when necessary): Fusion, Vesta, RSL.

Fixes:

-   issue [http://www.mantis.objectvision.nl/view.php?id=1094
    1094](http://www.mantis.objectvision.nl/view.php?id=1094_1094 "wikilink"):
    color palettes are now hidden for Layers with more than 32 classes
    and for Wms Layers.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1152
    1152](http://www.mantis.objectvision.nl/view.php?id=1152_1152 "wikilink"):
    Viewing info in a feature layer with an indirect geometry now works.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1157
    1157](http://www.mantis.objectvision.nl/view.php?id=1157_1157 "wikilink"):
    After right-clicking a color in a layer-control, the GUI doesn't
    crash anymore.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1159
    1159](http://www.mantis.objectvision.nl/view.php?id=1159_1159 "wikilink"):
    Diagnostics: Detail pages now also provide info when an script-error
    was found when processing its contents.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1166
    1166](http://www.mantis.objectvision.nl/view.php?id=1166_1166 "wikilink"):
    CalcCache: no more illegal abstract error when opening an persistent
    sequence array from a previous session.

Progress on issue 823: gdal2.vect and gdal2.grid have now been
implemented.

## Versions of 2018

### 2018-10-15: GeoDms Version 7.182 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7182-Setup-x64.exe)

Implemented:

-   issue [http://www.mantis.objectvision.nl/view.php?id=823
    823](http://www.mantis.objectvision.nl/view.php?id=823_823 "wikilink"):
    now all GDAL 1.91 data is installed inside the GeoDMS program
    folder.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1092
    1092](http://www.mantis.objectvision.nl/view.php?id=1092_1092 "wikilink"):
    background of required items are colored in the TreeView when a
    target is set from the Popup menu-\>Code analysis...

Fixed:

-   issue [http://www.mantis.objectvision.nl/view.php?id=1085
    1085](http://www.mantis.objectvision.nl/view.php?id=1085_1085 "wikilink"):
    Group by \> sort on attribute now works
-   issue [http://www.mantis.objectvision.nl/view.php?id=1086
    1086](http://www.mantis.objectvision.nl/view.php?id=1086_1086 "wikilink"):
    nth_element_weighted now can process zero weights

<!-- -->

-   issue [http://www.mantis.objectvision.nl/view.php?id=1088
    1088](http://www.mantis.objectvision.nl/view.php?id=1088_1088 "wikilink"):
    Crash or not, depending on Order of calculation.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1090
    1090](http://www.mantis.objectvision.nl/view.php?id=1090_1090 "wikilink"):
    Enter when the Current Item Bar has focus, only activates that item;
    F5 has now been disabled.
-   issue [http://www.mantis.objectvision.nl/view.php?id=1111
    1111](http://www.mantis.objectvision.nl/view.php?id=1111_1111 "wikilink"):
    split_polygon(geometry parameter) now works.

### 2018-08-29: GeoDms Version 7.180 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7180-Setup-x64.exe) and [Win32](https://www.geodms.nl/downloads/Setup/GeoDms7180-Setup-Win32.exe)

This version has been compiled with:

-   Visual Studio 2017, version 14.1 (which replaced Visual Studio 2015,
    v 12.0).
-   Boost 1.67 (was: 1.66)
-   GDAL 2.3.1 (additional to 1.9.1)

Many compiler warnings have been dealt with and syntax improvement
opportunities have been implemented.

Fixed issues:

-   [1014](http://www.mantis.objectvision.nl/view.php?id=1014) (VS 2017)
-   [1074](http://www.mantis.objectvision.nl/view.php?id=1074):
    Centroid_or_mid took too long.
-   [208](http://www.mantis.objectvision.nl/view.php?id=208): popup menu
    doesn't activate all items at first request.

Implemented:

-   issue 1089: more informative about box.

Progress on issues:

-   [823](http://www.mantis.objectvision.nl/view.php?id=823): GDAL 2.3.1
    can now be used for reading; use gdal2.vect or gdal2.grid as
    StorageType.
-   [1092](http://www.mantis.objectvision.nl/view.php?id=1092): Added
    'Instantiated From' reference in the General Detail page, and
    calculation rules (expression properties) now have

### 2018-07-18: GeoDms Version 7.177 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7177-Setup-x64.exe)

Implements:

-   issue [811](http://mantis.objectvision.nl/view.php?id=811):
    unconnected pairs in an OD matrix now have Undefined Impedance.
    **Breaking Change**: Check the usage of dense OD impedance matrices
    calculated with dijkstra_m function as unconnected pairs now have
    impedance <null> and no longer maxDist.
-   issue [1025](http://mantis.objectvision.nl/view.php?id=1025): Get
    and set statusflags from command line

Fixes:

-   issue [988](http://mantis.objectvision.nl/view.php?id=988): color
    for NoData always transparent in FeatureLayer and GridLayer.
-   issue [1038](http://mantis.objectvision.nl/view.php?id=1038): no
    error msg "No such host is known" when drawing background layer
    without internet connection

Supplemental changes related to

-   issue [644](http://mantis.objectvision.nl/view.php?id=644):
    Visualisation styles shown in legend

### 2018-07-04: GeoDms Version 7.176 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7176-Setup-x64.exe) and [Win32](https://www.geodms.nl/downloads/Setup/GeoDms7176-Setup-Win32.exe)

**Breaking change**:

-   issue [1068](http://mantis.objectvision.nl/view.php?id=1068).

For documenting projects with many inheritance relations, such as
[Vesta](https://github.com/RuudvandenWijngaart/VestaDV), the status quo
was no longer sustainable. Now the combination of inheritance and
overriding is stopped. An overriding item without calculation rule no
longer inherits the sub-items of the overridden item. Check if your
config will be affected before updating to this version.

**Furthermore**:

This version also contains a few fixes and improvements of recent
functionality, more specifically:

-   issues related to the new Table Group By Tool
    ([790](http://mantis.objectvision.nl/view.php?id=790)):
    [1060](http://mantis.objectvision.nl/view.php?id=1060),
    [1061](http://mantis.objectvision.nl/view.php?id=1061),
    [1062](http://mantis.objectvision.nl/view.php?id=1062).
-   issue related to the current item bar:
    [1065](http://mantis.objectvision.nl/view.php?id=1065): Ctrl-C and
    Ctrl-V now work.
-   issue [1063](http://mantis.objectvision.nl/view.php?id=1063):
    connect operator.
-   issue [1067](http://mantis.objectvision.nl/view.php?id=1067): mean
    operator.
-   issue [909](http://mantis.objectvision.nl/view.php?id=909): shp/dbf
    export now has the same set of attributes as a .csv export.
-   issue [985](http://mantis.objectvision.nl/view.php?id=985): zoom
    level of the overview panel in the MapView.
-   issue [1001](http://mantis.objectvision.nl/view.php?id=10001) and
    related [644](http://mantis.objectvision.nl/view.php?id=644):
    different feature layers without thematic attribute now get
    different colors.

### 2018-06-25: GeoDms Version 7.175 (withdrawn)

This version is published to present new functionality in the TableView
for the purpose of testing and evaluation. It doesn't contain fixed
issues.

Implements:

-   Issue [790](http://mantis.objectvision.nl/view.php?id=790): Group by
    Tool in the TableView for grouping rows on the values of one or more
    columns.

As this version introduced new issues that have been fixed in GeoDMS
7.176, the GeoDMS 7.175 is withdrawn from this publication page.

### 2018-06-11: GeoDms Version 7.174 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7174-Setup-x64.exe)

Fixes:

-   Desktop issue
    [1053](http://mantis.objectvision.nl/view.php?id=1053),
-   PP2 [1040](http://mantis.objectvision.nl/view.php?id=1040).
-   Diagnostic messages.

### 2018-06-05: GeoDms Version 7.173 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7173-Setup-x64.exe)

Fixes many minor issues related to recent additions:

-   Info Tool issue
    [494](http://mantis.objectvision.nl/view.php?id=494).
-   issue [1043](http://mantis.objectvision.nl/view.php?id=1043):
    TableView of fixed domain
-   Current Item Bar issues
    [1045](http://mantis.objectvision.nl/view.php?id=1045),
    [1046](http://mantis.objectvision.nl/view.php?id=1046),
    [1047](http://mantis.objectvision.nl/view.php?id=1047).
-   WmsLayer issues
    [1048](http://mantis.objectvision.nl/view.php?id=1048),
    [1049](http://mantis.objectvision.nl/view.php?id=1049).

### 2018-05-23: GeoDms Version 7.172 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7172-Setup-x64.exe)

Implements:

-   issue [820](http://mantis.objectvision.nl/view.php?id=820) An
    editable item path can be used to jump to other items (use F5 or
    mouse-click to search).
-   issue [809](http://mantis.objectvision.nl/view.php?id=809) now
    config files with UTF8 Byte Order Mark can be procesed.

Fixes:

-   issue [1007](http://mantis.objectvision.nl/view.php?id=1007)
    const_file_view Error when changing reference of unit <dpoint>
    LatLong
-   issue [1027](http://mantis.objectvision.nl/view.php?id=1027) Error
    Dialog appeared for non-related items when requesting data.
-   issue [1030](http://mantis.objectvision.nl/view.php?id=1030) LUS
    Demo: issue with PP2 enabled
-   issue [1035](http://mantis.objectvision.nl/view.php?id=1035)
    ViewDetails unknonw exception.

### 2018-04-30: GeoDms Version 7.170 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7170-Setup-x64.exe)

Fixes:

-   issue [1032](http://mantis.objectvision.nl/view.php?id=1032) with
    writing dijkstra_m results.

### 2018-04-24: GeoDms Version 7.169 for x64, withdrawn because of issue 1032.

Performance improvement:

-   [1021](http://mantis.objectvision.nl/view.php?id=1021):
    [Dijkstra_functions](Dijkstra_functions "wikilink") now run
    different origin zones in parallel when multiple CPU's are present.
    With 8 CPU's this maked the production of an OD table approximately
    6 times faster.

### 2018-04-19: GeoDms Version 7.168 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7168-Setup-x64.exe)

New feature:

-   Dijkstra_m can now produce OrgZone_MaxImp as startpoint attribute,
    see issue [1021](http://www.mantis.objectvision.nl/view.php?id=1021)
    for an example.

Fixed issues:

-   [1022](http://www.mantis.objectvision.nl/view.php?id=1022): When
    copying to Clipboard from the TableView, now wide characters are
    used that can better represent UTF8 characters than the earlier
    usage of ASCII when writing to the Clipboard,
-   [1024](http://www.mantis.objectvision.nl/view.php?id=1024): GeoDMS
    starts again on Windows 7.
-   [592](http://www.mantis.objectvision.nl/view.php?id=592): event log
    info of dijkstra operations is immediately visible again (with MT2
    on).

### 2018-04-13: GeoDms Version 7.167 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7167-Setup-x64.exe)

**Warning**: This version requires Windows 10, see issue
[1024](http://www.mantis.objectvision.nl/view.php?id=1024). Please
replace by 7.168 if you install on an older version of Windows.

Fixes issue [1018](http://www.mantis.objectvision.nl/view.php?id=1018):
background drawing wasn't clipped, causing frames and some text controls
to disappear, introduced by GeoDMS Version 7.166; please update any
7.166 to 7.168.

And fixes diagnostic (=error message related) issues:

-   [999](http://www.mantis.objectvision.nl/view.php?id=999):
-   [1012](http://www.mantis.objectvision.nl/view.php?id=1012):

### 2018-03-28: GeoDms Version 7.166 (withdrawn)

Fixes issues:

-   WMS related:
    [996](http://www.mantis.objectvision.nl/view.php?id=996),
    [997](http://www.mantis.objectvision.nl/view.php?id=997),
    [998](http://www.mantis.objectvision.nl/view.php?id=998),
    [1003](http://www.mantis.objectvision.nl/view.php?id=1003),
    [1010](http://www.mantis.objectvision.nl/view.php?id=1010),
    [1011](http://www.mantis.objectvision.nl/view.php?id=1011),
-   [588](http://www.mantis.objectvision.nl/view.php?id=588):
    split_polygon
-   [994](http://www.mantis.objectvision.nl/view.php?id=994): PolyData
    sneller in TableView
-   [995](http://www.mantis.objectvision.nl/view.php?id=995):
    TableCoposer operatoren AsString en AsList werken nu ook voor
    tabellen van meer dan 4 GB.
-   [999](http://www.mantis.objectvision.nl/view.php?id=999): some extra
    debug info with ViewDetails might help to diagnose this issue.

### 2018-03-07: GeoDms Version 7.165 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7165-Setup-x64.exe)

Fixed issues:

-   [990](http://www.mantis.objectvision.nl/view.php?id=990): faster
    lookup and corrected permutation_rnd in case of PP1
-   [899](http://www.mantis.objectvision.nl/view.php?id=899): redraw
    layers when activated.

### 2018-03-05: GeoDms Version 7.163 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7163-Setup-x64.exe)

Implemented issues:

-   236, 312: background layers are only read when the current
    zoom-factor don't rule them out.
-   918: wms as background layer.

Fixed issues:

-   983,
-   984,
-   986,
-   987

upgrade from boost 1.60.0 to 1.66.0

### 2018-02-13: GeoDms Version 7.162 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7162-Setup-x64.exe)

Fixed issues:

-   [427](http://www.mantis.objectvision.nl/view.php?id=427),
-   [793](http://www.mantis.objectvision.nl/view.php?id=793): now values
    are displayed in ExplainValue of aggregation in the Detail Pages,
-   [961](http://www.mantis.objectvision.nl/view.php?id=961),
-   [968](http://www.mantis.objectvision.nl/view.php?id=968),
-   [969](http://www.mantis.objectvision.nl/view.php?id=969),
-   [971](http://www.mantis.objectvision.nl/view.php?id=971)

Implemented issues:

-   [199](http://www.mantis.objectvision.nl/view.php?id=199),
-   [743](http://www.mantis.objectvision.nl/view.php?id=743),
-   [931](http://www.mantis.objectvision.nl/view.php?id=931) Settings
    are now stored in user and machine specific profiles, even when the
    LAN provides users with roaming profiles.

Added:

-   operator do(exec_operation, resulting_filename): see issue 971
-   operators min_index(values, partitioning(optional)) and
    max_index(values, partitioning(optional)): see issue 743.

## Versions of 2017

### 2017-12-21: GeoDms Version 7.159 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7159-Setup-x64.exe)

Added:

-   operator mapping_count(srcDomain, dstDomain, CountType)

### 2017-12-13: GeoDms Version 7.157 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7157-Setup-x64.exe)

Fixes:

-   issue 953 (which was fixed incorrectly in GeoDMS 7.156)

### 2017-11-30: GeoDms Version 7.155 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7155-Setup-x64.exe)

Poly2grid has been made even faster by using for each row a vector based
linked list of edges that start in that row, which prevents sorting the
edges. Now poly2grid is O(n+p+r\*log(k)) where n = the number of edges,
p = the number of pixels to be painted, r - the number of rows, and k =
the maximum number of edges crossing one row.

Fixes:

-   issue 915
-   issue 928
-   issue 934

### 2017-11-23: GeoDms Version 7.152 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7152-Setup-x64.exe)

Fixes:

-   issue 926 (poly2grid artifacts)
-   issue 5
-   issue 829
-   issue 885
-   issue 911
-   issue 918
-   issue 919
-   issue 928

Poly2grid has been implemented much faster by sorting and processing
edges from low to high rows.

### 2017-10-13: GeoDms Version 7.148 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7148-Setup-x64.exe)

Fixes:

-   poly2grid drawing on too many pixes in case of a horizontal border
    (issues 908 and 916).

New:

-   Properties DomainUnit_FullName en ValuesUnit_FullName (issue 829).
-   Button for TableView export (issue 561)

### 2017-08-28: GeoDms Version 7.145 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7145-Setup-x64.exe)

Fixes:

-   issues 885, 896, 898, 904, 905, 906

### 2017-06-13: GeoDms Version 7.141 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7141-Setup-x64.exe)

-   GeoDMS 7.140 introduced an issue for older versions of Windows, such
    as Server 2008 R2 Datacenter: critical section locking during
    initialization when loading DLL's caused a deadlock. This is now
    avoided by registering operators without locking the operator
    registry during initialization.

The 7.140 version had been developed with a focus on bug fixing and
getting things more stable. The regression tests have been expanded to
include GUI actions in order to keep more functionality working as it
worked in previous versions.

Functional and/or Breaking changes:

-   Default code editor is now the default installation path for Notepad
    ++, see [configuration of your preferred code
    editor](Configuration_File_Editor "wikilink")

Now Implemented:

-   [000821](http://www.mantis.objectvision.nl/view.php?id=821) Export
    Primary Data to.. now uses the full path for a default filename.
-   [000859](http://www.mantis.objectvision.nl/view.php?id=859) Allow
    longer item names, read from GDAL sources, such as fgdb.

Fixes:

-   [000894](http://www.mantis.objectvision.nl/view.php?id=894) reading
    boolean data with GDAL.
-   [000890](http://www.mantis.objectvision.nl/view.php?id=890) GDAL
    issue with reading 16 bits data as 32 bits values
-   [000887](http://www.mantis.objectvision.nl/view.php?id=887) Edit
    Palette Dialog issues
-   [000884](http://www.mantis.objectvision.nl/view.php?id=884) Multi
    Select issues in point- and polygon layers.
-   [000874](http://www.mantis.objectvision.nl/view.php?id=874) &
    [000893](http://www.mantis.objectvision.nl/view.php?id=893)
-   [000872](http://www.mantis.objectvision.nl/view.php?id=872)
-   [000871](http://www.mantis.objectvision.nl/view.php?id=871) Lock
    already taken issue.
-   [000859](http://www.mantis.objectvision.nl/view.php?id=859)
-   [000539](http://www.mantis.objectvision.nl/view.php?id=539) order of
    items in Explain value is now determined by the order of appearance
    in the calculation rule.

Regression test now includes the following GUI actions: Show MapView,
Edit Palette and Reclassify.

### 2017-02-21: GeoDms Version 7.139 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7139-Setup-x64.exe)

Fixes:

-   000539
-   000855
-   000859
-   000860
-   000865

### 2017-02-08: GeoDms Version 7.138 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7138-Setup-x64.exe)

Fixes:

-   000852
-   000853
-   000854
-   000807

### 2017-01-09: GeoDms Version 7.137 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7137-Setup-x64.exe)

Main improvement of this release is that all read data action and most
calculations are now done in worker threads, which keeps the main thread
responsive for GUI actions. Specific handling enables the user to change
data or close views, which will invalidate running tasks. Adding a
thematic attribute without pre-configured classification to a MapView
now no longer waits for the completion of the data processing for the
Jenks Fisher classifications.

New functionality:

-   000842: Implemented a new property with the name 'FullSource'

Fixes:

-   000626: AddLayer cmd no longer freezes on making data available for
    generating a classification scheme.
-   000847: read/write lock error.
-   000788: case parameter references
-   000837: Reading tiff with GDAL.grid and the original TIFF reader now
    both fully support reading area's with different scales and extents
    (sub-sections as well as enclosing extents, for which the border
    area is set as undefined or false). GDAL.grid now also supports
    reading attributes with values of type Bool, UInt2, or UInt4.

<!-- -->

-   0000802: \[Export\] Export data to .csv / Table Copy to Clipboard
    took extremely long - resolved.
-   0000813: \[TableView\] DataReadLock issue in EditPalette - resolved.
-   0000539: \[Explain Value\] Faster retrieval of aggregant values when
    explaining an aggregation.
-   0000738: \[Calculations\] connect and connect_info using additional
    attribute information - resolved.
-   0000385: \[Views\] Activate existing layers/columns in MapView and
    TableView when related items are activated again - implemented.
-   0000005: \[Export\] Export Scalebar with correct symbol and text
    size. - resolved.
-   0000801: \[TableView\] Copy to Clipboard (man on the beach) fails
    with selected data - resolved.
-   0000791: \[Calculations\] Memory Allocation Failed in template
    instantiation - resolved.
-   0000609: \[CalcCache\] connect_neighbour results in invalid outcome
    for tiled data - resolved.
-   0000788: \[Calculations\] reference for a case parameter differs
    between first and following instantiations - resolved.
-   0000783: \[Calculations\] substr results in GeoDmsGui has stopped
    working \> crash - resolved.
-   0000580: \[Desktop\] DetailPage Value Info geeft alleen relevante
    informatie bij activering uit tabel/kaart of back/forward knop -
    partially fixed.

## Versions of 2016

### 2016-04-11: GeoDms Version 7.130 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7130-Setup-x64.exe) and [Win32](https://www.geodms.nl/downloads/Setup/GeoDms7130-Setup-Win32.exe)

Addresses issues:

-   Fixes a bug in ordering items (sort, rlookup, rjoin, etc.) that was
    introduced in 7.126 in the fix of issue
    [739](http://www.mantis.objectvision.nl/view.php?id=739).
-   [753](http://www.mantis.objectvision.nl/view.php?id=753)
-   [754](http://www.mantis.objectvision.nl/view.php?id=754)
-   [755](http://www.mantis.objectvision.nl/view.php?id=755)

There are currently known issues with PP2, so I recommend to set

-   Tools-\>Options-\>General settings-\>Parallel Processing 1 = ON
    (parallel tile processing)
-   Tools-\>Options-\>General settings-\>Parallel Processing 2 = OFF
    (multiple calculation steps simultaneously)