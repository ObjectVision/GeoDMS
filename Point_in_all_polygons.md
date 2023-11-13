*[[Geometric functions]] point_in_all_polygons*

[[images/Point_in_all_polygons.png]]

## syntax

- point_in_all_polygons(*point_data_item*, *polygon_data_item*)

## definition

point_in_all_polygons(*point_data_item*, *polygon_data_item*) results in a new [[uint32]] [[domain unit]] with two subitems: *first_rel* and *second_rel*.

These subitems indicate <B>which point is located in which polygons</B>, as a point can be located in multiple polygons and a polygon can container multiple points, this is an *n* to *n* relation.

The *first_rel item* contains the [[relation]] to the *point_data_item*.

The *second_rel item* contains the relation to the *polygon_data_item*.

## applies to

- a [[data item]] *point_data_item* with Point [[value type]]
- a data item *polygon_data_item* with [[composition type|composition]] polygon and Point value type

## since version

8.035

## example
<pre>
unit&lt;uint32&gt; district_relations := <B>point_in_all_polygons(</B>Pointdomain/point, district/geometry<B>)</B>;
</pre>

| **first_rel** | **second_rel** |
|--------------:|---------------:|
| **0**         | **0**          |
| **1**         | **1**          |
| **2**         | **0**          |
| **2**         | **1**          |

| Pointdomain/id | Pointdomain/label | Pointdomain/point |
|---------------:|-------------------|-------------------|
| 0              | A                 | {401331, 115135}  |
| 1              | B                 | {399476, 111803}  |
| 2              | C                 | {399289, 114903}  |
|                |                   |                   |

*domain Pointdomain, nr of rows = 3*

| District/id | District/geometry      |
|------------:|------------------------|
| 0           | {21:{403025, 113810},{4|
| 1           | {17:{400990, 113269},{4|
|             |                        |

*domain District, nr of rows = 2*

## see also

- [[point_in_polygon]]
- [[point_in_ranked_polygon]]