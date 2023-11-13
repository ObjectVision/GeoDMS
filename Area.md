*[[Geometric functions]] area*

[[images/area_function.png]]

## syntax

- area(*polygon_data_item*, *valuesunit*)

## definition

area(*polygon_data_item*, *valuesunit*) calculates the **surface** of the polygon_data_item [[argument]], in the image the green hedged area.

The resulting [[values unit]] is configured as second argument.

## description

The resulting area values need to be positive, if the values are negative it means the order of points in the [[polygon]] [[geometry]] is not according to the following rules:

The order of points in a sequence needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule)

## applies to

- [[data item]] *polygon_data_item* with fpoint or dpoint [[value type]] and [[composition type|composition]] polygon.

## conditions

The [[metric]] of the [[values unit]] argument must match with the square of the metric of the coordinates in the coordinate system.

## example

<pre>
attribute&lt;m2&gt; surface (district) := <B>area(</B>district/geometry, m2<B>);</B>
</pre>

| district/geometry       | **surface** |
|-------------------------|-------------|
| {21 {403025, 113810}{4  | **1003100** |
| {17 {400990, 113269}{4  | **474460**  |
| {19 {401238, 115099}{4} | **1246460** |

*domain district, nr of rows = 3*