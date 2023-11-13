*[[Geometric functions]] poly2grid*

[[images/Poly2grid_w400.png]]

## syntax

- poly2grid(*polygon_data_item*, *gridunit*)

## definition

The poly2grid function results in **[[grid]] [[data item]] with a [[relation]] to the [[domain unit]]** of the *polygon_data_item*

The resulting data item has:

- as [[values unit]] the [[domain unit]] of the *polygon_data_item* [[argument]]
- as [[domain unit]] the *gridunit* argument

## description

Since 7.020 the poly2grid function is based on a similar function in GDAL. The explicit gdal_poly2grid function with the same signature is a synonym for the poly2grid function since 7.020.

Between the versions 6.025 and 7.020, the poly2grid function used a GDI function to convert data items of polygon domains to grid domains. This function is still available as gdi_poly2grid, with the same signature as poly2grid. The function is faster than the new function based on GDAL, but can result in invalid results on some graphic devices supporting a limited set of colors.

In versions before 6.025 the point_in_polygon function was used for this purpose. This is still possible, but it is advised to use the poly2grid function instead as it is much faster.

In earlier versions, a third argument was used, the *subPixelFactor*. A higher value meant more accurate results, but also more processing time.
From version 7.020 on, with the implementation of the GDAL function, a value of one for this *subPixelfactor* already results in accurate results. This third argument has become obsolete.

## applies to

- data item *polygon_data_item* with a wpoint, spoint, upoint, ipoint, fpoint or dpoint [[value type]]
- [[unit]] *gridunit* with a wpoint, spoint upoint, ipoint, fpoint, dpoint value type

## conditions

1. The [[composition type|composition]] type of the *polygon_data_item* needs to be polygon.
2. The [[domain unit]] of the *polygon_data_item* must be of value type uint32.

## complexity

The complexity of the poly2grid function for one polygon is O(n\* log(n) + x), with:
- n: number of points of the polygon.
- x: number of pixels to be drawn.

This implies the number of points per polygon is an important factor in the calculation speed of the function.

Simplifying polygons can help to improve the calculation speed. See [[Geometric functions]] for simplify functions in the GeoDMS.

## since version

6.025

## example

<pre>
attribute&lt;source/district&gt; DistrictGrid (gridunit/gridcel_10m) := 
   <B>poly2grid(</B>Source/District/border, gridunit/gridcel_10m<B>)</B>;
</pre>

## see also

- example [[Polygon to grid]]
- [[poly2grid_untiled]]