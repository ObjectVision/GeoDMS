*[[Geometric functions]] split_polygon*

[[images/Split_w320.png]]

## syntax

- split_polygon(*polygon_data_item*)

## description

split_polygon(polygon_data_item) results in a new uint32 [[domain unit]] with **single polygons** for each (multi)[[polygon]] in the *polygon_data_item* [[argument]].

If the original *polygon_data_item* only contains single polygons, the resulting domain unit has the same number of elements as the domain unit of the
*polygon_data_item* argument.

The figure illustrates a source *polygon_data_item* (left image) of a domain unit with four entries.

Each color represents an entry in the *polygon_data_item*, the red and green areas are multipolygons.

The resulting domain unit of the split_polygon function for this *polygon_data_item* will contain seven entries, with a single polygon for each entry (right image).

The split_polygon functions generates two (and since GeoDMS 8) 3 subitems:

- geometry: the [[geometry]] of the single polygons. This [[attribute]] has the same [[values unit]] as the *polygon_data_item* argument.
- nr_OrgEntity: a [[relation]] for the new domain towards the domain of the *polygon_data_item* argument.
- polygon_rel: a [[relation]] for the new domain towards the domain of the *polygon_data_item* argument. This data item is a copy of the nr_OrgEntity, but whith a name that meets our [[naming conventions]]. The nr_OrgEntity argument is still generated for backward compatibility. 

We advice to use the polygon_rel argument, the nr_OrgEntity argument will be phased out.

## applies to

attribute *polygon_data_item* with an ipoint or spoint [[value type]]

## conditions

1. The [[composition type|composition]] of the *polygon_data_item* argument needs to be polygon.
2. The domain unit of the *polygon_data_item* [[argument]] must be of value type uint32.

This function results in problems for (integer) coordinates larger than 2^25 (after translation where the first point is moved to (0, 0)). If your integer coordinates for instance represent mm, 2^25[mm] = about 33 [km]. The reason is that for calculating intersections, products of coordinates are calculated and casted to float64 with a 53 bits mantissa (in the development/test environment of boost::polygon these were float80 values with a 64 bits mantissa). We advise to keep the size of your integer coordinates for polygons limited and for instance do not use a mm precision for country borders (meter or kilometer might be sufficient).

## since version

7.042

## example

<pre>
unit&lt;uint32&gt; single_polygons := <B>split_polygon(</B>multipolygon/geometry<B>)</B>;
</pre>

## see also
- [[union_polygon (dissolve)]]