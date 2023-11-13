*[[Geometric functions]] overlay_polygon*

[[images/Intersect_w320.png]]

## syntax

- overlay_polygon(*first_polygon_data_item*, *second_polygon_data_item*)

## description

overlay_polygon(*first_polygon_data_item*, *second_polygon_data_item*) results in a new uint32 [[domain unit]] with an entry for each **intersecting** part of the *first_polygon_data_item* and the *second_polygon_data_item*.

The function generates three [[subitems|subitem]] for the new domain unit:
- geometry: the [[geometry]] of the resulting [[polygons|polygon]] (in the figure the yellow polygons). This [[attribute]] has the same [[values unit]] as the *first_polygon_data_item* and *second_polygon_data_item* attributes.
- first_rel: a [[relation]] for the new domain unit towards the domain of the *first_polygon_data_item*.
- second_rel: a relation for the new domain unit towards the domain of the *second_polygon_data_item*.

In other GIS software the term intersect is often used for this operation.

## applies to

attributes *first_polygon_data_item* and *second_polygon_data_item* with an ipoint or spoint [[value type]].

## conditions

1.  The [[composition type|composition]]  of the *first_polygon_data_item* and *second_polygon_data* [[arguments|argument]] needs to be polygon.
2.  The [[values unit]] of the *first_polygon_data_item* and *second_polygon_data* arguments items must match.
3.  The order of the points in the *first_polygon_data_item* and *second_polygon_data* needs to be clockwise for exterior bounds and
    counter clockwise for holes in polygons (right-hand-rule).

This function results in problems for (integer) coordinates larger than 2^25 (after translation where the first point is moved to (0, 0)). If your integer coordinates for instance represent mm, 2^25[mm] = about 33 [km]. The reason is that for calculating intersections, products of coordinates are calculated and casted to float64 with a 53 bits mantissa (in the development/test environment of boost::polygon these were float80 values with a 64 bits mantissa). We advise to keep the size of your integer coordinates for polygons limited and for instance do not use a mm precision for country borders (meter or kilometer might be sufficient).

## since version


7.042

## example

<pre>
unit&lt;uint32&gt; intersect := <B>overlay_polygon(</B>Building/geometry, District/geometry<B>)</B>;
</pre>