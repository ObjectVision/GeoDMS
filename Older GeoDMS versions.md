## Versions of 2019

### 2019-12-04: [GeoDms Version 7.206 for x64](https://www.geodms.nl/downloads/Setup/GeoDms7206-Setup-x64.exe)

This version fixes some issues related to CalcCache resource usage and
various other improvements.

Select GeoDmsGui->Tools->Options->Current Configuration->Minimum size
for DataItem specific swapfiles in CalcCache and change the value from
1000000 to 1000000000 to improve calculation speed for the price of
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
    machine in Tools->Options. Default is off.

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

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1192>
    1192](http://www.mantis.objectvision.nl/view.php?id=1192_1192 "wikilink"):
    lock already taken
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1192>
    1191](http://www.mantis.objectvision.nl/view.php?id=1192_1191 "wikilink"):
    combine(range(u, xb, xe), range(v, yb, ye)) now works
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1024>
    1024](http://www.mantis.objectvision.nl/view.php?id=1024_1024 "wikilink"):
    Startup issue on Windows 7.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1183>
    1183](http://www.mantis.objectvision.nl/view.php?id=1183_1183 "wikilink"):
    memory allocation errors
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1184>
    1184](http://www.mantis.objectvision.nl/view.php?id=1184_1184 "wikilink"):
    domain unification error when using ParseXML
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1185>
    1185](http://www.mantis.objectvision.nl/view.php?id=1185_1185 "wikilink"):
    Layer Control names and unit specs are represented again.

Other Fixed issue:

-   issue [<http://www.mantis.objectvision.nl/view.php?id=11177>
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

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1092>
    1092](http://www.mantis.objectvision.nl/view.php?id=1092_1092 "wikilink"):
    code analysis for clean-up support: now a source can be set, more
    used items are included (such as when used indirectly in
    properties), and representation colors have been improved.

Possible breaking change:

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1141>
    1141](http://www.mantis.objectvision.nl/view.php?id=1141_1141 "wikilink"):
    Now two domains are considered different if they refer to different
    items, even if their calculation rule is the same. For example:
    unit<uint32> landuse_class: nrofrow=5; unit<uint32> building_type:
    nrofrows=5; are now domain-incompatible. Thus, the entity
    compatibility check is now more strict. Tested projects (and adapted
    when necessary): Fusion, Vesta, RSL.

Fixes:

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1094>
    1094](http://www.mantis.objectvision.nl/view.php?id=1094_1094 "wikilink"):
    color palettes are now hidden for Layers with more than 32 classes
    and for Wms Layers.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1152>
    1152](http://www.mantis.objectvision.nl/view.php?id=1152_1152 "wikilink"):
    Viewing info in a feature layer with an indirect geometry now works.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1157>
    1157](http://www.mantis.objectvision.nl/view.php?id=1157_1157 "wikilink"):
    After right-clicking a color in a layer-control, the GUI doesn't
    crash anymore.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1159>
    1159](http://www.mantis.objectvision.nl/view.php?id=1159_1159 "wikilink"):
    Diagnostics: Detail pages now also provide info when an script-error
    was found when processing its contents.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1166>
    1166](http://www.mantis.objectvision.nl/view.php?id=1166_1166 "wikilink"):
    CalcCache: no more illegal abstract error when opening an persistent
    sequence array from a previous session.

Progress on issue 823: gdal2.vect and gdal2.grid have now been
implemented.

## Versions of 2018

### 2018-10-15: GeoDms Version 7.182 for [x64](https://www.geodms.nl/downloads/Setup/GeoDms7182-Setup-x64.exe)

Implemented:

-   issue [<http://www.mantis.objectvision.nl/view.php?id=823>
    823](http://www.mantis.objectvision.nl/view.php?id=823_823 "wikilink"):
    now all GDAL 1.91 data is installed inside the GeoDMS program
    folder.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1092>
    1092](http://www.mantis.objectvision.nl/view.php?id=1092_1092 "wikilink"):
    background of required items are colored in the TreeView when a
    target is set from the Popup menu->Code analysis...

Fixed:

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1085>
    1085](http://www.mantis.objectvision.nl/view.php?id=1085_1085 "wikilink"):
    Group by > sort on attribute now works
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1086>
    1086](http://www.mantis.objectvision.nl/view.php?id=1086_1086 "wikilink"):
    nth_element_weighted now can process zero weights

<!-- -->

-   issue [<http://www.mantis.objectvision.nl/view.php?id=1088>
    1088](http://www.mantis.objectvision.nl/view.php?id=1088_1088 "wikilink"):
    Crash or not, depending on Order of calculation.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1090>
    1090](http://www.mantis.objectvision.nl/view.php?id=1090_1090 "wikilink"):
    Enter when the Current Item Bar has focus, only activates that item;
    F5 has now been disabled.
-   issue [<http://www.mantis.objectvision.nl/view.php?id=1111>
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
    working > crash - resolved.
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

-   Tools->Options->General settings->Parallel Processing 1 = ON
    (parallel tile processing)
-   Tools->Options->General settings->Parallel Processing 2 = OFF
    (multiple calculation steps simultaneously)