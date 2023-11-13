Geometric [[functions|Operators and functions]] calculate with geometries (coordinates) of [[points|point]], [[arcs|arc]] or [[polygons|polygon]]

## point

- [[point|point function]] - *make a point attribute based on an X and Y attribute*
- [[point_isNearby]] - *compare if two points are (almost) the same location*
- [[PointRow]] - *get the row numbers of a point attribute*
- [[PointCol]] - *get the column numbers of a point attribute*
- [[dist(ance)|dist]] - *calculate the distance between two point sets*
- [[sq(ua)r(e)dist(ance)|sqrdist]] - *calculate the square distance between two arrays of points*
- [[bg_buffer_point]] - *creates a buffer polygon for each point in a pointset*

## polygon

- [[area]] - *calculates the surface of each polygon*
- [[centroid]] 
- [[centroid_or_mid]] - *the centroid if it is located within a polygon or else a mid-point of a polygon.*
- [[poly2grid]] - *a grid representation of polygons*
- [[poly2grid_untiled]] - *a grid representation of polygons*
- [[lower_bound]] - *the lowest X and Y values of the points in each polygon*
- [[upper_bound]] - *the highest X and Y values of the points in each polygon*
- [[center_bound]] - *the center X and Y values of the points in each polygon*
- [[overlay_polygon (intersect)]] - *domain unit with the intersecting part of each polygon*
- [[split_polygon]] - *domain unit with all single polygons*
- [[split_partitioned_union_polygon]] - *combination of [[split_polygon]] and [[partitioned_union_polygon|partitioned_union_polygon (dissolve by attribute)]]*
- [[polygon inflated]] - *increases each polygon*
- [[polygon deflated]] - *decreases each polygon*
- [[polygon_connectivity]] - *domain unit with the connected polygons*
- [[sub (difference)]] - *a cutout of a polygon in another polygon*
- [[mul (overlap)]] - the *overlap of two arrays of polygons*
- [[add (union)]] - *the element by element union of two polygons*
- [[union_polygon (dissolve)]] - *remove lines of adjacent polygons*
- [[partitioned_union_polygon (dissolve by attribute)]] -  *remove lines of adjacent polygons, grouped by a relation*
- [[bg_simplify_polygon]] - *simplify the geometry of a polygon*
- [[bg_simplify_multi_polygon]] - *simplify the geometry of a multi polygon*
- [[bg_buffer_multi_polygon]] - *creates a buffer polygon for each polygon*

_Several geometric polygon functions can be combined in a single operator; for the full list of those combinations see [[here|combined_geometric_functions]]._

## points/arcs

- [[dyna_point]] / dyna_point_with_ends - *configures a point set for an arc on an equal distance*
- [[dyna_segment]] / dyna_segments_with_ends - *configures a segment set for an arc on an equal distance*

## points/polygons

- [[point_in_polygon]] - *index number of the polygon in which a point is located*
- [[point_in_ranked_polygon]] - *index number of the polygon in which a point is located, using a ranking in case a polygon is located in multiple polygons*
- [[point_in_all_polygons]] - *relation between index numbers of the polygons in which points are located*
- [[sequence2points]] - *all vertices of each arc/polygon as separate points*
- [[points2sequence]] - *constructs arc/polygon from vertices*

## arc/polygon

- [[arc_length]]
- [[arc2segm]] *- divides an arc/polygon data item into [[segments|segment]]*
- [[bg_simplify_linestring]] - *simplifies the geometry of an arc*
- [[bg_buffer_linestring]] - *creates a buffer polygon for each arc*
- [[bg_buffer_multi_point]]- *creates a buffer polygon for each coordinate of the arc/polygon*

## points/arcs/polygons

### conversion
- [[RD2LatLongWgs84]] - *converts coordinates from RD to LatLongWgs84*
- [[LatLongWgs842RD]]- *converts coordinates from LatLongWgs84 to RD*
- [[RD2LatLongGE]]- *converts coordinates from RD to LatLongGE*
- [[RD2LatLongEd50]]- *converts coordinates from RD to LatLongEd50*
- [[RD2LatLong]]- *converts coordinates from RD to LatLong*