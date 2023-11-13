*[[Geometric functions]] polygon deflated*

[[images/Deflate_new_w300.png]]

## syntax
- polygon_dXd(*polygon_data_item*, *deflatesize*)
- polygon_dXhv(*polygon_data_item*, *deflatesize*)

## description

The polygon_iXX functions result in deflated versions of an original *polygon_data_item*.

There are different ways of deflating polygons related to how angles are treated.

Multiple functions are implemented, the figure shows 4 of these functions. See the differences in how the angles are rounded off.

The degree of roundness is obtained by increasing the value 4 to 16.

The suffix 'd' or 'hv' is used for the kernel; diagonal (diamond shaped) or horizontal-vertical (square shaped).

For these deflations the [Minkowski sum method](https://www.boost.org/doc/libs/1_51_0/libs/polygon/doc/gtl_minkowski_tutorial.htm) is used.

The *deflatesize* [[parameter]] indicates with how many units in the coordinate system used the original polygon is deflated.

## applies to

- [[attribute]] *polygon_data_item* and with an ipoint or spoint [[value type]].
- parameter *deflatesize* with float64 value type.

## conditions

1. The [[composition type|composition]] type of the *polygon_data_item* needs to be polygon.
2. The order of the points in the *polygon_data_item* needs to be clockwise for exterior bounds and counter clockwise for holes in polygons (right-hand-rule).
3. The current implementation of these functions request coordinate values to be less than 67.108.864. Convert detailed coordinates (e.g. in mm) to cm/dm if your coordinate values exceed this number.

These functions result in problems for (integer) coordinates larger than 2^25 (after translation where the first point is moved to (0, 0)). If your integer coordinates for instance represent mm, 2^25[mm] = about 33[km]. The reason is that for calculating intersections, products of coordinates are calculated and casted to float64 with a 53 bits mantissa (in the development/test environment of boost::polygon these were float80 values with a 64 bits mantissa). We advise to keep the size of your integer coordinates for polygons limited and for instance do not use a mm precision for country borders (meter or kilometer might be sufficient).

## since version

7.042

## examples

<pre>
attribute&lt;ipoint&gt; building_deflated_4D75 (polygon, source/bld) := <B>polygon_d4D(</B>bld/border, 75<B>)</B>;
<I>// deflates building borders with 75 units in the coordinate system
// angles are rounded off with 4 points per angle
// deflates using a diagonal kernel (diamond shaped).</I>

attribute&lt;ipoint&gt; building_deflated_i_8D75 (polygon, source/bld) := <B>polygon_d8D(</B>bld/border, 75<B>)</B>;
<I>// deflates building borders with 75 units in the coordinate system
// angles are rounded off with 8 points per angle.
// deflates using a diagonal kernel (diamond shaped).</I>

attribute&lt;ipoint&gt; building_deflated_i_16D75 (polygon, source/bld) := <B>polygon_d16D(</B>bld/border, 75<B>)</B>;
<I>// deflates building borders with 75 units in the coordinate system
// angles are rounded off with 16 points per angle.
// deflates using a diagonal kernel (diamond shaped).</I>

attribute&lt;ipoint&gt; building_deflated_4HV75(polygon, source/bld) := <B>polygon_d4HV(</B>bld/border, 75d>)</B>;
<I>// deflates building borders with 75 units in the coordinate system
// angles are rounded off with 4 points per angle
// deflates using a horizonal-vertical kernel (square shaped).</I>
</pre>

## see also
- [[bg_buffer_multi_polygon]]
- [[polygon inflated]]