*[[Geometric functions]] bg_simplify_multi_polygon*

[[images/Bg_simplify_polygon_w320.png]]

## syntax
- bg_simplify_multi_polygon(*multi_polygon_data_item*, *simplifyfactor*)

## description

bg_simplify_multi_polygon(*multi_polygon_data_item, simplifyfactor*) results in a [[data item]] with the geometry of the *multi_polygon_data_item*, in which the geometric structure is <B>simplified</B>. Near points are combined to a single point. What is near is configured with the *simplifyfactor*, a [[parameter]] indicating for which distance points are combined. No metric needs to be configured for the *simplifyfactor,* it is using the [[metric]] of the [[coordinate system]].

The *bg_* prefix of the function name indicates that the implementation of operator uses [boost geometry](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/index.html) library, more specifically, the
[simplify](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/geometry/reference/algorithms/simplify/simplify_3.html) function.

## applies to

- [[attribute]] *multi_polygon_data_item* with an point [[value type]] 
- parameter simplifyfactor with a [[float64]] value type

## conditions

1. The [[composition type|composition]] type of the *multi_polygon_data_item* item needs to be polygon.
2. The order of points in the *multi_polygon_data_item* needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule).

This function may result in losing the adjacency of polygons and in overlapping polygons, as shown in the figure above. We advice to be careful with functions like [[point_in_polygon]] on the resulting geometries.

## since version

7.409

## example
<pre>
attribute&lt;fpoint&gt; geometry_simplified (polygon, city) := <B>bg_simplify_multi_polygon(</B>city/geometry, 10.0<B>)</B>;
</pre>

## see also:
- [[bg_simplify_polygon]]
- [[bg_simplify_linestring]]
