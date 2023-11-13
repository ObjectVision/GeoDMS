*[[Network functions]] first node*

## syntax

- first_node(*arc_polygon_dataitem*)

## definition

first_node(*arc_polygon_dataitem*) results in a [[point]] [[data item]] with the coordinates of the **first point of an arc or polygon** of [[argument]] *arc_polygon_dataitem*.

## description

Use the [[arc2segm]] function to split an [[arc]] in [[segments|segment]].

## applies to

data item *arc_polygon_dataitem* with fpoint or dpoint [[value type]] and [[composition type|composition]] type arc or polygon.

## example

<pre>
attribute&lt;fpoint&gt; first_node (Road) := <B>first_node(</B>road/geometry<B>)</B>;
</pre>

| road/geometry                                        |**first_node**        |
|------------------------------------------------------|----------------------|
| {2 {399246, 112631}{398599, 111866}}                 | **{399246, 112631}** |
| {3 {398599, 111866}{399495, 111924} {401801,111524}} | **{398599, 111866}** |
| {2 {401529, 114921}{398584, 114823}}                 | **{401529, 114921}** |

*domain Road, nr of rows = 3*

## see also

- [[last_node]]