*[[Network functions]] connect_info*

## syntax

- connect_info(*arcs*, *points, [optional] maxSqrDist*)
- dist_info(*arcs*, *points, [optional] maxSqrDist*)

## definition

The connect_info(*arcs*, *points*) function is used to get **information** for each entry in the [[domain unit]] of [[attribute]] *points* on how the [[connect]] function (second variant) results in a **connection** to the attribute *arcs*.

The connect_info has the same [[arguments|argument]] as the connect function.

The connect_info results in a [[container]] with a set of attributes for the same domain unit as the *points*.

The result contains the following attributes:
-   dist: the distance between the point from the *points* and the CutPoint on the arc/polygon outline from the *arcs*;
-   ArcID: the [[relation]] for each entry of the domain unit of the *points* to the connected arc of the *arcs* domain unit;
-   CutPoint: The coordinate of the connection point on the arcs;
-   InArc: true if the CutPoint is not the first or last point
-   InSegm, true if the CutPoint is not an (intermediate or final) point of the sequence of points.
-   SegmID, id of the segment in the arc to which the point is connected.

## description

The *arcs* should contain unique geometries. Use the [[unique]] function to make a domain unit with unique geometries.

## applies to

- [[data items|data item]] *arcs* and *points* with fpoint or dpoint [[value type]]
- data item *arcs* with [[composition type|composition]] arc or polygon. Be ware, if you connect points to polygons, use a [[split_polygon]] to avoid connecting to segments that separate different rings (lakes or islands) in a polygon.

## conditions

The values type of *arcs* and *points* must match.

## example
<pre>
container location2road := <B>connect_info(</B>road/geometry, location/geometry<B>)</B>;
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

| **dist**   | **ArcID** | **CutPoint**         | **InArc** | **InSegm** | **SegmID** |
|-----------:|----------:|----------------------|-----------|------------|-----------:|
| **79.42**  | **2**     | **{398603, 114824}** | **True**  | **True**   | **0**      |
| **131.00** | **1**     | **{398688, 111872}** | **True**  | **True**   | **0**      |
| **63.09**  | **0**     | **{399186, 112560}** | **True**  | **True**   | **0**      |
| **56.51**  | **2**     | **{399291, 114847}** | **True**  | **True**   | **0**      |
| **119.52** | **1**     | **{399468, 111922}** | **True**  | **True**   | **0**      |
| **220.47** | **2**     | **{401338, 114915}** | **True**  | **True**   | **0**      |
| **180.79** | **1**     | **{401760, 111531}** | **True**  | **True**   | **1**      |
| **610.40** | **1**     | **{401625, 111555}** | **True**  | **True**   | **1**      |

*domain location, nr of rows = 8*

## see also

- [[connect]]
- [[connect_info_eq]]
- [[connect_info_ne]]