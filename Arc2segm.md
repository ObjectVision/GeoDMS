*[[Geometric functions]] arc2segm(ent)*

## syntax

- arc2segm(*arc_polygon_dataitem*)

## definition

arc2segm(*arc_polygon_dataitem*) **divides** an [[arc]] or [[polygon]] [[data item]] *arc_polygon_dataitem* **into [[segments|segment]]**.

The function results in a new [[domain unit]] with two [[subitems|subitem]]:

1. point: a [[point]] data item with the first point of each segment;
2. nextpoint: a point data item with the last point of each segment.

## description

The arc2segm function is used to split up an arc geometry from which intermediates need to become nodes in a network.

## applies to

data item *arc_polygon_dataitem* with fpoint or dpoint [[value type]] and [[composition type|composition]] arc or polygon.

## example

<pre>
unit&lt;uint32&gt; segments := <B>arc2segm(</B>road/geometry<B>)</B>;
</pre>

| road/geometry                                         |
|-------------------------------------------------------|
| {2 {399246, 112631}{398599, 111866}}                  |
| {3 {398599, 111866}{399495, 111924} {401801, 111524}} |
| {2 {401529, 114921}{398584, 114823}}                  |

*domain Road, nr of rows = 3*

| **point**            | **nextpoint**        |
|----------------------|----------------------|
| **{399246, 112631}** | **{398599, 111866}** |
| **{398599, 111866}** | **{399495, 111924}** |
| **{399495, 111924}** | **{401801, 111524}** |
| **{401529, 114921}** | **{398584, 114823}** |

*domain segments, nr of rows = 4*