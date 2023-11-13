*[[Geometric functions]] point_in_ranked_polygon*
[[images/point_in_all_ranked_polygon_function.png]]

## syntax

-   point_in_ranked_polygon(*point_data_item*, *polygon_data_item*, *rank_data_item*)

## definition

point_in_ranked_polygon(*point_data_item*, *polygon_data_item*, *rank_data_item*) results for each point of the *point_data_item* in a
[[relation]] towards the domain unit of the *polygon_data_item* **in which the point is located**.

The resulting [[values unit]] is the [[domain unit]] of the *polygon_data_item*.

If a point is not located in any polygon, the function results in a [[null]] value. If a point is located in multiple polygons (the red point in the image), the function results in the polygon with the lowest *rank_data_item* value.

## description

The *rank_data_item* argument is used to make an explicit choice for a polygon, if a point is located in multiple polygons. Therefore the rank values need to distinguish the different polygons. If a constant is used as rank value, the point_in_ranked_polygon results in the same values as the [[point_in_polygon]] function.

## applies to

- a [[data item]] *point_data_item* with Point [[value type]]
- a data item *polygon_data_item* with [[composition type|composition]] type polygon and Point value type
- a data item *rank_data_item* with a uint8, (u)int32, float32 or float64 value type

## since version

8.036

## example

<pre>
attribute&lt;city&gt; city_rel (ADomain) := <B>point_in_ranked_polygon(</B>Adomain/point, city/geometry, city/rank<B>)</B>;
</pre>

| point            |**city_rel** |
|------------------|------------:|
| {401331, 115135} | **3**       |
| {399476, 111803} | **2**       |
| {399289, 114903} | **1**       |
| {401729, 111353} | **5**       |
| {398696, 111741} | **null**    |

*ADomain, nr of rows = 5*

| City/geometry          | City/rank |
|------------------------|----------:|
| {21:{403025, 113810},{4| 2         |
| {17:{400990, 113269},{4| 1         |
| {19:{403128, 115099},{4| 7         |
| {23:{402174, 113703},{4| 4         |
| {30:{401531, 114646},{4| 6         |
| {13:{402757, 114546},{4| 5         |
| {54:(405282, 113562},{4| 3         |

*domain City, nr of rows = 7*

## see also

- [[point_in_polygon]]
- [[point_in_all_polygons]]