*[[Geometric functions]] sub (difference)*

[[images/Polygon_difference_w440.png]]

## syntax
- sub(*polygon_data_itemA*, *polygon_data_itemB*)
- *polygon_data_itemA* - *polygon_data_itemB*

## definition

sub(*polygon_data_itemA*, *polygon_data_itemB*) or *polygon_data_itemA*
- *polygon_data_itemB* results in a new [[polygon]] [[data item]] with the **difference** of the two [[arguments|argument]]: *polygon_data_itemA* and *polygon_data_itemB*

The left part of the figure illustrates the arguments: *polygon_data_itemA* and *polygon_data_itemB*.

The resulting polygon (right side of the image) is the original square (*polygon_data_itemA*) minus the ditstrict *polygon_data_itemB*.

## description

The sub function can for example be used to create a mask polygon.

## applies to

data items *polygon_data_itemA* and *polygon_data_itemB* with an ipoint or spoint [[value type]].

## conditions

1. The [[composition type|composition]] of the arguments items needs to be polygon.
2. The domain of the arguments must match of be [[void]].

## since version

7.112

## example
<pre>
parameter&lt;ipoint&gt; geometry (poly) := square/geometry[0] <B>-</B> union_polygon/geometry;
</pre>

## see also
- [[sub]]
- [[mul (overlap)]]