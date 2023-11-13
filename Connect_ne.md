*[[Network functions]] connect_ne*

[[images/Connect_ne.png]]

## syntax

- connect_ne(*arc_polygon_dataitem*, *arc_polygon_match_dataitem*, *point_dataitem*, *point_match_dataitem*)

## definition

The connect_ne functions works in a similar way as the second variant of the **[[connect]]** function, with the extra condition that the values of the *arc_polygon_match_dataitem* **may not match** with the values of the *point_match_dataitem*.

The figure shows an example of how this connect_ne function can be used.

With the connect function the blue star location would be connected to the orange road. Assume this orange road is a highway, then a connection should not be made to this road. The connect_ne can be used to connect the blue star to the nearest road not being a highway.

## description

The [[value type]] of matching [[arguments|argument]]: *arc_polygon_match_dataitem* and *point_match_dataitem* need to be uint32. This means street names (as in the example) first need to be converted to street [[index numbers]], see the example.

Points with [[null]] values for the *point_match_dataitem* will always be connected to the nearest road (even if the road has a null value in the *arc_polygon_match_dataitem*).

To arcs/polygons with null values for the *arc_polygon_match_dataitem*, no points will be connected (except for points that are nearest to these roads and have a null value as *point_match_dataitem*).

## applies to

- [[data item]] *arc_polygon_dataitem* with [[composition type|composition]] arc or polygon. Be aware, if you connect points to polygons, always use a [[split_polygon]] first of the polygon geometry to get rid of the connection lines in polygons between islands and lakes.
- data item *point_dataitem* with fpoint or dpoint [[value type]]
- data items *arc_polygon_match_dataitem* and *point_match_dataitem* with uint32 value type.

## conditions

1.  The value type of the *arc_polygon_dataitem* and *point_dataitem* [[arguments|argument]] must match.
2.  The *arc_polygon_dataitem* and *arc_polygon_match_dataitem* arguments need to have the same [[domain unit]].
3.  The *point_dataitem* and *point_match_dataitem* arguments need to have the same domain unit.

## since version

7.131

## example

<pre>
unit&lt;uint32&gt; location : nrofrows = 5
{
   attribute&lt;point_rd&gt; point      := src/location/point;
   attribute&lt;string&gt;   streetname : ['Astreet','CRoad','BStreet','DSquare','BStreet'];
   attribute&lt;street&gt;   street_rel := rlookup(streetname, street/values);
}
unit&lt;uint32&gt; road : nrofrows = 4
{
   attribute&lt;point_rd&gt; line (arc) := src/road/line;
   attribute&lt;string&gt;   streetname : ['Astreet','BStreet','CRoad','DSquare'];
   attribute&lt;street&gt;   street_rel := rlookup(streetname, street/values);
}
unit&lt;uint32&gt; street := unique(roadarcs/street);

unit&lt;uint32&gt; connect_ne := <B>connect_ne(</B>road/line, road/street_rel, location/point, location/street_rel<B>)</B>;
</pre>

## see also

- [[connect]]
- [[connect_eq]]
- [[connect_info_ne]]