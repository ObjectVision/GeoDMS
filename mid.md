*[[Geometric functions]] mid*

[[images/centroid_function.png]]

## syntax

- mid(*polygon_data_item*)

## definition

mid(*polygon_data_item*) results in a [[point]] [[data item]] with the **middle** of the *polygon_data_item* [[argument]].

## description

The middle of a [[polygon]] is determined by selecting a horizontal scan-line that separates 50% of the area of that polygon above and below that scan-line, and on that scan-line, the 50% percentile of a set of evenly distributed points on that scan-line that are within that polygon.

If the scan line does not contain any points within the polygon (for example on a hourglass-shaped polygon with an zero-sided waist), other scan-lines are  tried until a point is found for which point_in_polygon would result in true.

## applies to

- [[data item]] *polygon_data_item* with fpoint or dpoint [[value type]] and [[composition|composition type]] polygon



## implementation details:
![image](https://github.com/ObjectVision/GeoDMS/assets/2284361/5a7d3698-72e9-46ec-a152-dd8a0c50b192)

For more info on SelectRow and GetScanPoint, see: 
- https://github.com/ObjectVision/GeoDMS/blob/v13/rtc/dll/src/geo/SelectPoint.h
- https://github.com/ObjectVision/GeoDMS/blob/v13/rtc/dll/src/geo/CalcWidth.h

