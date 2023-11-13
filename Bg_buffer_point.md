*[[Geometric functions]] bg_buffer_point*

[[images/Bg_buffer_point.png]]

## syntax

- bg_buffer_point(*point_data_item*, *size*, *nr_angles*)

## definition

bg_buffer_point(*point_data_item*, *size*, *nr_angles*) results in a <B>[[polygon]] [[data item]] with buffer circles around each point</B> in the *point_data_item*. The resulting data item has the same [[domain unit]] as the *point_data_item*.

The *size* argument indicates the buffer circle size.

The *nr_angles* argument indicates how many angles are used to make the buffer circles. More angles results in smoother circles but also in more data.

The *bg_* prefix of the function name indicates that the implementation of operator uses [boost geometry](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/index.html)
library, more specifically, the [buffer](https://www.boost.org/doc/libs/1_80_0/libs/geometry/doc/html/geometry/reference/algorithms/buffer/buffer_4.html) function.

## applies to

- [[data item]] *point_data_item* with a point [[value type]]
- [[parameter]] *size* with a [[float64]] value type
- parameter *nr_angles* with a [[uint8]] value type

## since version

8.031

## example

<pre>
attribute&lt;fpoint&gt; point_buffer_circles (pointset) := <B>bg_buffer_point(</B>pointset/geometry, 10.0, 16b<B>)</B>;
</pre>

more examples of buffer functions can be found here: [[Buffer processing example]]

## see also

- [[bg_buffer_linestring]]
- [[bg_buffer_multi_polygon]]
- [[bg_buffer_multi_point]]