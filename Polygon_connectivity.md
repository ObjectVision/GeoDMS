*[[Geometric functions]] polygon_connectivity*

[[images/Polygon_connectivity_w200.png]]

## syntax
- polygon_connectivity(*polygon_data_item*)

## definition

polygon_connectivity(*polygon_data_item*) results in a new uint32 [[domain unit]] with **one entry for each 'connection**' in the *polygon_data_item* [[argument]].

A 'connection' is defined as two polygons having at least one common interior point.

## description

The function results in an F1 and F2 [[attribute]] with [[relations|relation]] to the [[domain unit]] of the *polygon_data_item* attribute.

The relations indicate which connections exists, each connection only occurs once (a connection between polygon 0 and 1 only occurs as F1: 0 and F2: 1 and not vice versa).

Use the [[multiply|Mul (overlap)]] operator for [[polygons|polygon]] to calculate the overlap between connected polygons.

## applies to

attribute *polygon_data_item* with an ipoint or spoint [[value type]].

## conditions

1.  The [[composition type|composition]] type of the *polygon_data_item* [[argument]] needs to be polygon.
2.  The order of the points in *polygon_data_item* needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule).

## since version

7.135

## example
<pre>
unit&lt;uint32&gt; connection := <B>polygon_connectivity(</B>district/geometry<B>)</B>;
</pre>

| F1  | F2  |
|-----|-----|
| 0   | 3   |
| 0   | 5   |
| 0   | 6   |
| 1   | 2   |
| 1   | 3   |
| 1   | 6   |
| 2   | 3   |
| 2   | 4   |
| 2   | 6   |
| 3   | 4   |
| 3   | 5   |
| 3   | 6   |
| 4   | 5   |

*domain connection, nr of rows = 13*