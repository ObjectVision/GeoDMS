*[[Geometric functions]] arc_length*

## syntax

- arc_length(*arc_poly_dataitem*, *valuesunit of coordinate system*)

## definition

arc_length(*arc_poly_dataitem*, *valuesunit*) calculates the **length** of an [[arc]] or the **outline** of a [[polygon]] [[data item]] *arc_poly_dataitem*.

The resulting [[values unit]] is configured as second [[argument]]. This gives an interpretation to the resulting values. It does not convert them. For example, if the coordinate system is in meters, then the resulting output are meter, but you should also configure it to be interpreted as meters. If you want kilometers as output, you'll have to manually convert it to km.

## description

The definition of the **outline** of a polygon is only equal to the [perimeter](wikipedia:Perimeter "wikilink") if the polygon has no islands and lakes.

In case a polygon consits of multiple rings, the outline also contains the artificial lines needed to relate the rings, see [[polygon]] data model.

If you want to calculate the [perimeter](wikipedia:Perimeter "wikilink"), use a [[split polygon]] function first to make different polygons for all exterior rings (the outline of the lakes are still a part of the calculated arc_length).

## applies to

- arc or polygon data item *arc_poly_dataitem* with fpoint or dpoint [[value type]] and [[composition type|composition]] arc or polygon.

## example

<pre>
attribute&lt;meter&gt; road_length (Road) := <B>arc_length(</B>road/geometry, meter<B>)</B>;
</pre>

|road/geometry                                       |road_length|
|----------------------------------------------------|----------:|
|{2 {399246, 112631}{398599, 111866}}                |1001.92    |
|{3 {398599, 111866}{399495, 111924} {401801,111524}}|3238.31    |
|{2 {401529, 114921}{398584, 114823}                 |2946.63    |

*domain Road, nr of rows = 3*