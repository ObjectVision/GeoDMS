Vector data is data with a one-dimensional [[domain unit]], with at least one [[feature attribute]] and optionally other [[attributes|attribute]] for the same domain unit.

[[images/VectorDomain.png]]

**The feature attribute refers to the coordinates of the vectors.** 

The GeoDMS supports the following vector types:
- [[point]]: point data, the feature attribute refers to one coordinate per entry.
- [[arc]]: arc data, the feature attribute refers to at least two coordinates per entry.
- [[polygon]]: polygon data, the feature attribute refers to at least three coordinates per entry.

It is important to know the [[coordinate system]] of your vector data, as they may effect the results of functions like [[arc_length]] and [[connect_info]].

## vector sources

Vector data in GeoDMS applications is read from (and can also be written to some of) the following formats:
- [[ESRI Shapefile]] (point, arc, polygon)
- [[GeoPackage]] (point, arc, polygon)
- [[ASCII Files]] : [[CSV]] (point) and [[XML]] (point, arc, polygon)
- [[FileGeoDatabase]] (point, arc, polygon)
- [[PostgreSQL PostGIS|PostGis]] (point, arc, polygon)
- [[XML/GML|XML]](point, arc, polygon)
- [[FSS]] files (point, arc, polygon)

## vector calculations

The GeoDMS contains multiple functions to calculate with the vector coordinates, see mainly the [[Geometric functions]]  
