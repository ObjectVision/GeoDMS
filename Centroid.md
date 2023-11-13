*[[Geometric functions]] centroid*

[[images/centroid_function.png]]

## syntax

- centroid(*polygon_data_item*)

## definition

centroid(*polygon_data_item*) results in a [[point]] [[data item]] with the **centroid** of the *polygon_data_item* [[argument]].

## description

The [centroid](https://en.wikipedia.org/wiki/Centroid) of a [[polygon]] does not have to be located within the polygon. Use the [[centroid_or_mid]] function to find a point always located within or at the border of that polygon such that [[point_in_polygon]] would return true;

## applies to

- [[data item]] *polygon_data_item* with fpoint or dpoint [[value type]] and [[composition|composition type]] polygon

## example

<pre>
attribute&lt;point_rd&gt; centroid (district) := <B>centroid(</B>district/geometry<B>)</B>;
</pre>

| district/geometry      | **centroid**         |
|------------------------|----------------------|
| {21 {403025, 113810}{4 | **{402955, 113049}** |
| {17 {400990, 113269}{4 | **{401159, 112704}** |
| {19 {401238, 115099}{4 | **{401268, 114017}** |

*domain district, nr of rows = 3*

## implementation details

see code from: https://github.com/ObjectVision/GeoDMS/blob/v13/rtc/dll/src/geo/Centroid.h
![image](https://github.com/ObjectVision/GeoDMS/assets/2284361/1a45e0c4-8ab6-4eae-9b5b-92d2f9cd5c2e)

## see also

- [[centroid_or_mid]]