*[[Geometric functions]] mul (overlap)*

[[images/Polygon_multiply_w440.png]]

## syntax
- mul(*polygon_data_itemA*, *polygon_data_itemB*)
- *polygon_data_itemA* \* *polygon_data_itemB*

## definition

mul(*polygon_data_itemA*, *polygon_data_itemB*) or *polygon_data_itemA* * *polygon_data_itemB* results in a new [[polygon]] [[data item]] with the **overlap** of the *polygon_data_itemA* and *polygon_data_itemB*.

The left part of the figure illustrates the [[arguments|argument]]: *polygon_data_itemA* and *polygon_data_itemA*.

The resulting polygon (right side of the image) is the overlap of the original square (*polygon_data_itemA*) and the ditstricts *polygon_data_itemB*.

## description

Use the [[polygon_connectivity]] function to find out which polygons are connected.

Use the [[area]] function on the results of the multiplied polygons to find out the size of the overlap.

## applies to

data items *polygon_data_itemA* and *polygon_data_itemB* with an ipoint or spoint [[value type]].

## conditions

1. The [[composition type|composition]] type of the arguments needs to be polygon.
2. The [[domain unit]] of the arguments must match or be [[void]].

## since version

7.112

## example

<pre>
parameter&lt;ipoint&gt; geometry (poly) := square/geometry[0] <B>*</B> union_polygon/geometry;
</pre>

## see also
- [[sub (difference)]]
- [[mul]]