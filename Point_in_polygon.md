*[[Geometric functions]] point_in_polygon*

[[images/point_in_polygon_function.png]]

## syntax

- point_in_polygon(*point_data_item*, *polygon_data_item*)

## definition

point_in_polygon(*point_data_item*, *polygon_data_item*) results for each point of the *point_data_item* in a [[relation]] towards the [[domain unit]] of the *polygon_data_item* **in which the point is located**.

The resulting [[values unit]] is the domain unit of the *polygon_data_item*.

If a point is not located in any polygon, the function results in a [[null]] value (the yellow point in the image). If a point is located in multiple polygons, the function results in the 'first' relation found. As the function uses a tile specific spatial index, the concept of what is first is dependent on the tile division. With the same tile division, the function is deterministic, but if the tile division is changed this can influence the results of this point_in_polygon function. If you want to explicitly configure the ranking of the polygons in these cases, use the [[point_in_ranked_polygon]] function.

## description

The point_in_polygon function was used in earlier versions of the GeoDMS to rasterize polygon data. Since version 6.025, it is advised to use the 
 [[poly2grid]] function for this purpose.

## applies to

- a [[data item]] *point_data_item* with Point [[value type]]
- a data item *polygon_data_item* with [[composition type|composition]] type polygon and Point value type

## example

<pre>
attribute&lt;district&gt; district_rel (ADomain) := <B>point_in_polygon(</B>Adomain/point, district/geometry<B>)</B>;
</pre>

| point            |**district_rel** |
|------------------|----------------:|
| {401331, 115135} | **6**           |
| {399476, 111803} | **4**           |
| {399289, 114903} | **1**           |
| {401729, 111353} | **5**           |
| {398696, 111741} | **null**        |

*ADomain, nr of rows = 5*

| District/geometry|
|------------------------|
| {21:{403025, 113810},{4|
| {17:{400990, 113269},{4|
| {19:{403128, 115099},{4|
| {23:{402174, 113703},{4|
| {30:{401531, 114646},{4|
| {13:{402757, 114546},{4|
| {54:(405282, 113562},{4|

*domain District, nr of rows = 7*

## see also

- [[point_in_ranked_polygon]]
- [[point_in_all_polygons]]