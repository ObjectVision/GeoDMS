*[[Geometric functions]] centroid_or_mid*

[[images/centroid_or_mid_function.png]]

## syntax

- centroid_or_mid(*polygon_data_item*)

## definition

centroid_or_mid(*polygon_data_item*) results in a [[point]] [[data item]] with

1.  the **[[centroid]]** if it is located in the [[polygon]]
2.  or else the [[mid]] of the polygon_dataitem [[argument]]. The [[mid]] of a polygon is calculated as the middle point of the coordinates of the polygon in such a way it is always located within the polygon. See [[https://github.com/ObjectVision/GeoDMS/blob/05c226bb521e51d16299bb532087098e30304618/rtc/dll/src/geo/SelectPoint.h#L190]] for a description.

## description

The centroid_or_mid function results in the centroid if it is located within the polygon or else in the [[mid]] of the polygon.
This means the result of the centroid_or_mid is always located within the polygon, or at its border such that [[point_in_polygon]] would return true.

In the image above the centroid is not located within the polygon. The centroid_or_mid function will still result in a point within the polygon.

## applies to

data item *polygon_dataitem* with fpoint or dpoint [[value type]] and [[composition type|composition]] polygon.

## example
<pre>
attribute&lt;point_rd&gt; centroid_or_mid (district) := <B>centroid_or_mid(</B>district/geometry<B>)</B>;
</pre>

| district/geometry      |**centroid_or_mid**   |
|------------------------|----------------------|
| {21 {403025, 113810}{4 | **{402955, 113049}** |
| {17 {400990, 113269}{4 | **{401159, 112704}** |
| {19 {401238, 115099}{4 | **{401268, 114017}** |

*domain district, nr of rows = 3*

## see also

- [[centroid]]
- [[mid]]