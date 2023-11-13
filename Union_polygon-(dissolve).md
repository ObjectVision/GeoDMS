*[[Geometric functions]] union_polygon (dissolve)*

[[images/Union_polygon_w320.png]]

## syntax
- union_polygon(*polygon_data_item*)

## description

union_polygon(*polygon_data_item*) results in a [[parameter]] with all polygons of the *polygon_data_item*, in which the lines between adjacent polygons are removed.

In other GIS software the term **dissolve** is often used for this operation.

## applies to

[[attribute]] *polygon_data_item* with an ipoint or spoint [[value type]]

## conditions

1.  The [[composition type|composition]] type of the *polygon_data_item* item needs to be polygon.
2.  The [[domain unit]] of the *polygon_data_item* item must be of value type uint32.
3.  The order of points in the *polygon_data_item* needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule).

This function result in problems for (integer) coordinates larger than 2^25 (after translation where the first point is moved to (0, 0)). If your integer coordinates for instance represent mm, 2^25[mm] = about 33[km]. The reason is that for calculating intersections, products of coordinates are calculated and casted to float64 with a 53 bits mantissa (in the development/test environment of boost::polygon these were float80 values with a 64 bits mantissa). We advise to keep the size of your integer coordinates for polygons limited and for instance do not use a mm precision for country borders (meter or kilometer might be sufficient).

## since version

7.042

## example

<pre>
parameter&lt;ipoint&gt; geometry (polygon) := <B>union_polygon(</B>ipolygon(District/geometry)<B>)</B>;
</pre>

## see also

- [[partitioned_union_polygon (dissolve by attribute)]]
- [[add (union)]]