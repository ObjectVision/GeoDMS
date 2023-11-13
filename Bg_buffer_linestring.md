*[[Geometric functions]] bg_buffer_linestring*

[[images/Bg_buffer_linestring.png]]

## syntax

- bg_buffer_linestring(*arc_data_item*, *size*, *nr_angles*)

## definition

bg_buffer_linestring(*point_data_item*, *size*, *nr_angles*) results in a [[polygon]] [[data item]] with <B>buffer polygons around each arc</B> in the *arc_data_item*. The resulting data item has the same [[domain unit]] as the *arc_data_item*, with [[composition type|composition]] type poly.

The *size* argument indicates the buffer size.

The *nr_angles* argument indicates how many angles are used to make the buffer angles. More angles results in smoother circles but also in more
data.

The *bg_* prefix of the function name indicates that the implementation of operator uses [boost geometry](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/index.html)
library, more specifically, the [buffer](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/geometry/reference/algorithms/buffer/buffer_4.html)
function.

## applies to

- [[attribute]] *arc_data_item* with a point [[value type]] and a composition type arc. Composition type polygon might also work but is not yet tested.
- [[parameter]] *size* with a [[float64]] value type
- parameter *nr_angles* with a [[uint8]] value type

## since version

8.031

## example

<pre>
attribute&lt;fpoint&gt; arc_buffer (poly, arcset) := <B>bg_buffer_linestring(</B>arcset/geometry, 10.0, 16b<B>)</B>;
</pre>

more examples of buffer functions can be found here: [[Buffer processing|Buffer processing example]]

## see also

- [[bg_buffer_point]]
- [[bg_buffer_multi_polygon]]
- [[bg_buffer_multi_point]]