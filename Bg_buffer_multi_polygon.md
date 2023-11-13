*[[Geometric functions]] bg_buffer_multi_polygon*

[[images/Bg_buffer_multi_polygon.png]]

## syntax
- bg_buffer_multi_polygon(*polygon_data_item*, *size*, *nr_angles*)

## definition

bg_buffer_multi_polygon(*point_data_item*, *size*, *nr_angles*) results in a [[polygon]] [[data item]] with <B>buffer polygons around each polygon</B> in the *polygon_data_item*. The resulting data item has the same [[domain unit]] as the *polygon_data_item*.

The *size* arguments indicates the buffer size.

The *nr_angles* arguments indicates how many angles are used to make the buffer angles. More angles results in smoother circles but also in more data.

The *bg_* prefix of the function name indicates that the implementation of operator uses [boost geometry](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/index.html) library, more specifically, the
[buffer](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/geometry/reference/algorithms/buffer/buffer_4.html) function.

## description

The bg_buffer_multi_polygon function can be used as alternative for the [[polygon inflated]] and [[polygon deflated]] functions. The
bg_buffer_multi_polygon is much faster, but for complex polygons it might be less accurate.

- Use a positive *size* argument to increase polygons (outer rings including islands are increased, inner rings (lakes) are decreased).
- Use a negative *size* argument to decrease polygons (outer rings including islands are decreased, inner rings (lakes) are increased).

If only the buffer polygon without the orginal polygon is needed, use the [[sub (difference)]] function the cut out the original polygon from the result of the bg_buffer_multi_polygon function.

The bg_buffer_multi_polygon does not always work correctly with holes and lakes if polygon geometries are not valid. If self-interactions occur in polygons we noticed unexpected results. We advice to use e.g. QGis to check the validity of your polygons before applying the bg_buffer_multi_polygon function. Within the GeoDMS you can use the [[mul (overlap)]] function (first and second argument are the same) to clean your polygon topology. In our test cases this solved the issue.

## applies to

- [[attribute]] *polygon_data_item* with a point [[value type]] and a [[composition type|composition]] polygon.
- [[parameter]] *size* with a [[float64]] value type
- parameter *nr_angles* with a [[uint8]] value type

## since version

8.031

## example
<pre>
attribute&lt;fpoint&gt; polygon_buffer (polygon, district) := <B>bg_buffer_multi_polygon(</B>polyset/geometry, 10.0, 16b<B>)</B>;
</pre>

more examples of buffer functions can be found here: [[Buffer processing|Buffer processing example]]

## see also

- [[bg_buffer_point]]
- [[bg_buffer_linestring]]
- [[bg_buffer_multi_point]]