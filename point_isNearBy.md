*[[Geometric functions]] point_is_NearBy*

## syntax
- point_isNearby(a, b, margin)

## definition
point_isNearby(a, b, margin) results in a boolean [data item](https://github.com/ObjectVision/GeoDMS/wiki/data_item) indicating if the values of [[point]] [[data item]] a <B>are within the margin</B> *margin* of the corresponding values of data item b.

## description

With floating point data values, due to round offs, it can be useful to compare results and accept a margin in which the comparison still results in a True value. Use the point_isNearby function in stead of the [[==]] function for point data in these cases.

The comparison between two missing values results in the value True.

The point_isNearBy function can be used in the similar manner as the [[float_isNearby]] function.

## applies to

Data items *a*, *b*, *margin* with fpoint/dpoint value type

## conditions

1. [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)%7Cliterals) or [[parameters|parameter]] can be compared to data items of any domain).
2. [[Arguments|argument]] must have matching:
   - [[value type]]

## see also
- [[float_isNearby]]