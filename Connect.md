*[[Network functions]] connect*

[[images/Connect_w240.png]]

## syntax

1. connect(*point_dataitem_dest*, *point_dataitem_org*)
2. connect(*arc_polygon_dataitem*, *point_dataitem, [optional]maxSqrDist*)

## definition

The connect function **connects nearby geometries**. It has two variants:

1. Both [[arguments|argument]] are single [[point]] [[data items|data item]] (syntax 1). This variant is used to find the nearest     *point_dataitem_dest* for each *point_dataitem_org*. The function results in a [[relation]] for the [[domain unit]] of *point_dataitem_org* towards the
 domain unit of *point_geom_dest*.
2. The first argument is an [[arc]] or [[polygon]], the second argument is a point data item (syntax 2). The points of *point_dataitem* are connected to the nearest arc/polygon outline of the *arc_polygon_dataitem*. Cut points are added as (intermediate) points to the *arc_polygon_dataitem* The functions results in a new domain unit with:
    - a geometry [[subitem]] (previously named UnionData) containing a new [[geometry]], combining the original geometry of the *arc_polygon_dataitem* and the new connections to the *point_dataitem*.
    - an arc_rel [[subitem]] (previously named Nr_OrgEntity) with a [[relation]] to the domain of the *arc_polygon_dataitem* ([[null]] for the new connections).

Additionally, an optional maxSqrDist can be specified, with the squares of the maximum distance that each point of the 2nd argument is allowed to look around.

The figure illustrates this second variant. The dark green houses represent the *point_dataitem*, the light green lines the *arc_polygon_dataitem*. The resulting UnionData subitem of the connect function is presented with the blue lines.

## description

The second variant is often used to build a [network](https://mathinsight.org/network_introduction) typology.

If the distance in the precision of the [[value type]] to the nearest point/arc/polygon is similar for multiple features, points are connected to the
point/arc/polygon with the lowest [[index number|index numbers]].

In both variants, the first argument should contain unique [[geometries|geometry]]. Use the [[unique]] function to make a domain unit with unique geometries.

The [[connect_info]] function is used in the second variant to present information on the extra connections, e.g. the distance of each connection and the coordinates of the CutPoint on the connected arc/polygon.

## applies to

-   data items *point_dataitem_org*, *point_dataitem_dest*, *arc_polygon_dataitem* and *point_dataitem* with fpoint or dpoint value type
-   data item *arc_polygon_dataitem* with [[composition type|composition]] type arc or polygon. Be aware, if you connect points to polygons, always use a [[split_polygon]] first of the polygon geometry to get rid of the connection lines in polygons between islands and lakes.

## conditions

The value type of all arguments must match.

## example

variant 1:

<pre>
attribute&lt;destination&gt; destination_rel  (origin) := <B>connect(</B>destination/geometry, origin/geometry<B>)</B>;
</pre>

| origin/geometry  |**destination_rel** |
|------------------|-------------------:|
| {401331, 115135} |              **0** |            
| {399476, 111803} |              **1** |            
| {399289, 114903} |              **2** |            
| {401729, 111353} |              **3** |            
| {398696, 111741} |              **1** |            

*domain origin, nr of rows = 5*

| destination/geometry |
|----------------------|
| {401331, 115131}     |
| {399138, 112601}     |
| {398600, 114903}     |
| {401729, 112156}     |

*domain destination, nr of rows = 4*

variant 2:

<pre>
unit&lt;uint32&gt; location2road := <B>connect(</B>road/geometry, location/geometry<B>)</B>;
</pre>

| road/geometry                                         |
|-------------------------------------------------------|
| {2 {399246, 112631}{398599, 111866}}                  |
| {3 {398599, 111866}{399495, 111924} {401801, 111524}} |
| {2 {401529, 114921}{398584, 114823}}                  |

*domain road, nr of rows = 3*

| location/geometry |
|-------------------|
| {398600, 114903}  |
| {398696, 111741}  |
| {399138, 112601}  |
| {399289, 114903}  |
| {399476, 111803}  |
| {401331, 115135}  |
| {401729, 111353}  |
| {401729, 112156}  |

*domain location, nr of rows = 8*

| **Location2Road/UnionData**                                 |
|-------------------------------------------------------------|
| **{2: {399246, 112631} {399186, 112560}}**                  |
| **{2: {398599, 111866} {398688, 111872}}**                  |
| **{2: {401529, 114921} {401338, 114915}}**                  |
| **{2: {398600, 114903} {398603, 114824}}**                  |
| **{2: {398696, 111741} {398688, 111872}}**                  |
| **{2: {399138, 112601} {399186, 112560}}**                  |
| **{2: {399289, 114903} {399291, 114847}}**                  |
| **{2: {399476, 111803} {399468, 111922}}**                  |
| **{2: {401331, 115135} {401338, 114915}}**                  |
| **{2: {401729, 111353} {401760, 111531}}**                  |
| **{2: {401729, 112156} {401625, 111555}}**                  |
| **{2: {398603, 114824} {398584, 114823}}**                  |
| **{2: {398688, 111872} {399468, 111922}}**                  |
| **{2: {399186, 112560} {398599, 111866}}**                  |
| **{2: {399291, 114847} {398603, 114824}}**                  |
| **{3: {399468, 111922} {399495, 111924} {401625, 111555}}** |
| **{2: {401338, 114915} {399291, 114847}}**                  |
| **{2: {401760, 111531} {401801, 111524}}**                  |
| **{2: {401625, 111555} {401760, 111531}}**                  |

*domain location2road, nr of rows = 19*

## see also

- [[connect_eq]]
- [[connect_ne]]
- [[connect_neighbour]]
- [[connect_info]]
- [[capacitated_connect]]