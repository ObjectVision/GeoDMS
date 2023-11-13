*[[Geometric functions]] partitioned_union_polygon (dissolve by attribute)*

[[images/Union_polygon_part_w320.png]]

## syntax

- partitioned_union_polygon(*polygon_data_item*, *relation*)

## description

partitioned_union_polygon(*polygon_data_item*, partioning) results in an attribute with all polygons from *polygon_data_item*, grouped by the [[argument]] *[[relation]]*.

Lines between adjacent polygons within each group are removed.

The [[domain unit]] of the resulting [[attribute]] is the [[values unit]] of the *relation*.

In other GIS software the term **dissolve** is often used for this operation.

## applies to

- attribute *polygon_data_item* with an ipoint or spoint [[value type]]
- *relation* with a value type of the group CanBeDomainUnit

## conditions

1.  The [[composition type|composition]] of the *polygon_data_item* item needs to be polygon.
2.  The domain unit of the *polygon_data_item* item must be of value type uint32.
3.  The domain unit of arguments *polygon_data_item* and *relation* must match.
4.  The order of points in the *polygon_data_item* needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule).

This function result in problems for (integer) coordinates larger than 2^25 (after translation where the first point is moved to (0, 0)). If your integer coordinates for instance represent mm, 2^25[mm] = about 33[km]. The reason is that for calculating intersections, products of coordinates are calculated and casted to float64 with a 53 bits mantissa (in the development/test environment of boost::polygon these were float80 values with a 64 bits mantissa). We advise to keep the size of your integer coordinates for polygons limited and for instance do not use a mm precision for country borders (meter or kilometer might be sufficient).

## since version

7.042

## example

<pre>
attribute&lt;ipoint&gt; geometry (polygon, region): = <B>partitioned_union_polygon(</B>ipolygon(district/geometry), region_rel<B>)</B>;
</pre>

## see also

- [[union_polygon (dissolve)]]