## FIXED in 8.007: crash RSLight2021_ontwikkel_2

*GeoDMS version: 8.006*

**configuration**: %RegressionTestsSourceDataDir%\\prj_snapshots\\RSLight2021_ontwikkel_2\\Runs\\PrepareData.dms

a.k.a.
D:\\SourceData\\RegressionTests\\prj_snapshots\\RSLight2021_ontwikkel_2\\Runs\\PrepareData.dms openklappen **item**: MaakOntkoppeldeBronData resulted in crash daarna: /MaakOntkoppeldeBronData/Generate_Run0 openenen

openklappen **item**: MaakOntkoppeldeBronData results in crash

MT settings 0 & 2 enabled, 1 & 3 disabled

# NAD, BREAKING CHANGE: select_orgrel and arrow operator

*GeoDMS version: 8.006*

Part of the unit test

# PARTALLY FIXED in 8.008: writing indirect data targets

*GeoDMS version: 8.006*

Part of the unit test, but indirect targets now cause longer calculation times and larger memory usage, compare performance of 8.008 with 8.007

# value info

## FIXED in 8.007: crash in value info

*GeoDMS version: 8.006*

**configuration**: %RegressionTestsSourceDataDir%\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms
a.k.a
D:\\SourceData\\RegressionTests\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms

request **item** :
t611_lus_demo_2022_results_test/A1_GE_Discr_calculated in Map View

Double Click on any cell to get value info

Double Click in Detail Page Value Info on value below LUClasses

Double Click Again in Detail Page Value Info on value below LUClasses

MT settings 0 & 2 enabled, 1 & 3 disabled

## value info for union_unit results

descr: explain value did show the source item and source row number.
reproduction: xxx,

# FIXED in 8.015: application hangs

**configuration**: %TstDir%\\Regression\\cfg\\stam.dms a.k.a.
C:\\dev\\tst\\Regression\\cfg\\stam.dms

**request item**:
/BAG_MakeSnapshot/snapshot_20170701/vbo/src/polygon/vbo_id in table

MT settings 0 & 2 enabled, 1 & 3 disabled

# FIXED in 8.011: reading fss with TiledUnit results in ReadSequence Error: stream size 1310720 conflicts with internal size 1048576

**configuration**: %RSLight_2021Path%\\Runs\\PrepareData.dms **request item**: /PrivData/VU/RegionalAvgCharacteristics/vrijstaand_size in table

# FIXED in 8.011: Request value info results in error Access violation in 'Shv.dll'.

**configuration**:
D:\\SourceData\\RegressionTests\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms
**request item**:
/Simulations/A1_GE/CaseData/Compacted/SuitabilityMaps/residential in Map
view and click on a cell

# FIXED in 8.010: selection in table results in error: DataArrayBase Error: Index 1 out of array-range (array.size

0 ) =

**configuration**:
D:\\SourceData\\RegressionTests\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms
**request item**:
/Simulations/A1_GE/CaseData/Compacted/SuitabilityMaps/residential in
table view, select first cell with selection tool, scroll in table,
error occurs

# FIXED in RSL 3 config, count over vbo_rel gives domain unification error

-   **branch**:
    <https://pbl.sliksvn.com/ruimtescanner/PBL/ProjDir/branches/RSLight2021_ontwikkel_3/>
-   **revision**: 8842, dd 06-07-2022
-   **configuration**: \\RSLight2021_ontwikkel_3\\Runs\\casus.dms na
    batch/RunAll.cmd
-   **request item**:
    /BaseData/Verdeling_VSSH/panden_ParticuliereHuurKoop/aantal_woningen

Fout treedt op bij 8.015 maar niet bij 7.412

Note that: values unit of index arg of lookup is:
"/SourceData/Vastgoed/BAG/PerJaar/Y2022/Panden/pand" and domain of
value-array is "/BaseData/Verdeling_VSSH/panden_ParticuliereHuurKoop"
arg1A = "/nr_ORgEntity"

# FIXED in 8.011: pcount error in last tile

**configuration**: %TstDir%\\Regression\\cfg\\stam.dms a.k.a.
C:\\dev\\tst\\Regression\\cfg\\stam.dms **request item**:
BAG_MakeSnapshot/snapshot_20170701/pand/Results/unique/vbo_count in
tableview, look at values for id \>= 10.485.760

# NAD: Point_in_polygon results differ from 7 series when polygon array contains overlap and is segmented differently

Cause: a spatial index is made per segment and different segmentation
may result in a different order of considering intersection. As this
order is now unspecified, it is unspecified which polygon is found when
a point is inside multiple polygons. Unspecified behaviour is only
guaranteed to be reproducible within the same version of the GeoDms, and
in casu with the same segmentation of polygons.

Possible adaptations:

-   check that polygons don't overlap
-   use overlay_polygon on little triangles of the points or (NYI) point_x_polygon
-   (done) Polygon index is now made for all polygons, not tile-by-tile
-   (NYI) new functions:
    -   point_in_polygons variant that delivers cross-table,
    -   point_in_unique_polygon that returns data failure when multiple
        polygons are found,
    -   point_in_polygon_or_null
    -   count_in_polygon

# FIXED in 8.014, Group By in table results in Error: DisplayValue called prematurely

**configuration**: %TstDir%\\Regression\\cfg\\stam.dms a.k.a.
C:\\dev\\tst\\Regression\\cfg\\stam.dms

**request item** : /brondata/bodemgebruik/Hoofdgroep in table, select
column, click Group By tool, error occurs

# FIXED in 8.015: Error in 8.014, Check Failed Error: sm \|\| this->IsPassor()

**configuration**: %prj_snapshotsDir%\\lus_demo_2022\\cfg\\demo.dms
a.k.a.
D:\\SourceData\\RegressionTests\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms

**select item** : Geography/rdc_base, Open Detail Pages, Properties,
error is shown

# FIXED in 8.024, Error in 8.015, WMTS layer is not shown

**configuration**: %prj_snapshotsDir%\\lus_demo_2022\\cfg\\demo.dms
a.k.a.
D:\\SourceData\\RegressionTests\\prj_snapshots\\lus_demo_2022\\cfg\\demo.dms

**select item** : Current_Situation/Current_landuse in MapView, no WMTS
layer is presented

# FIXED in 8.024, Point_In_Polygon crash in 8.019

**configuration**: %TstDir%\\Regression\\cfg\\stam.dms a.k.a.
C:\\dev\\tst\\Regression\\cfg\\stam.dms

**request item** :
/BAG_MakeSnapshot/snapshot_20170701/vbo/results/unique/pand_rel in
table, application crashes

# FIXED in 8.027, Error in 8.024, Map View & Layer control not updated after changing colors

**configuration**: %TstDir%\\Operator\\cfg\\microtst.dms a.k.a.
C:\\dev\\tst\\operator\\cfg\\microtst.dms

**select item** : BackGroundLayer/district/hoek in Map View, edit the color for the first class. Map View and Legend are not updated. Panning does not update the colors in the map, zooming does. For the Layer control the palette need to be hided and shown to update the color

# FIXED in 8.026, Error in 8.024, Changing the Classification in Map View

**configuration**: %TstDir%\\Operator\\cfg\\microtst.dms a.k.a. C:\\dev\\tst\\operator\\cfg\\microtst.dms

**select item** : BackGroundLayer/district/hoek in Map View, activate the edit palette option, change the #Classes to 3 and choose equal counts, error: ValidateCount (3) failed becasue this unit has count 7. After clicking OK, the Map view is updated, but the count column is empty.

# FIXED in 8.027, Error in 8.024, Table View not updated after selecting items

**configuration**: %TstDir%\\Operator\\cfg\\microtst.dms a.k.a. C:\\dev\\tst\\operator\\cfg\\microtst.dms

**select item** : BackGroundLayer/district/hoek in TableView, select an entry with the select objects tool, table is not updated (blue color is not shown). After activating another cell, table is updated

# FIXED in 8.027, Error in 8.024, Table View ony one selection action possible

**configuration**: %TstDir%\\Operator\\cfg\\microtst.dms a.k.a.
C:\\dev\\tst\\operator\\cfg\\microtst.dms

**select item** : BackGroundLayer/district/hoek in TableView, select first one item and then select all, even after activating another cell table is not updated. After the table is requested again, application crashes.