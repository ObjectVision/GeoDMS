For some [[functions|Operators and functions]] (like [[union_polygon|Union_polygon_(dissolve)]]) the sequence order of points in a [[polygon]] matters.

## rules

The following rules apply to the sequence of points in a [[polygon]]:

1.  outer rings need to be configured clock wise
2.  inner rings (lakes) are configured counter clock wise
3.  the first and the last point need to have the same coordinate

If the sequence of points is configured manually and used in the [[sequence2points]], always configure the correct sequence, although for the map and functions like [[point_in_polygon]] an incorrect sequence might not matter.

## validation

A way to test if the correct sequence is configured is by requesting the area of the [[polygon]] with the [[area]] function.
A positive value indicates a correct sequence, a negative value indicates the points are configured in the incorrect sequence