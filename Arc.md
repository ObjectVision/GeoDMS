[[images/arc_format.png]]

Arc [[feature attributes|feature attribute]] are [[attributes|attribute]] with a two-dimensional [[value type]] (PointGroup) and **at least two coordinates** for each entry in the [[domain unit]].

Arc attributes are used as feature attribute for [vector data](https://github.com/ObjectVision/GeoDMS/wiki/Vector-data).

The points in the correct sequence define an [arc](https://en.wikipedia.org/wiki/Arc_(geometry)), a line between a start point, zero or more intermediates and an end point. Arc attributes are often used for [[geographic|geography]] coordinates of roads, railways, rivers etc.

To make the GeoDMS aware that a sequence of coordinates need to be interpreted as an arc, this is done by configuring the [[composition type|Composition]] to arc.

## example

The following example shows the configuration of a [[domain unit]] road, in which each road is represented by one arc.
The data is read from an [[ESRI ShapeFile]] with [[gdal.vect]].

<pre>
unit&lt;uint32&gt; road
:  StorageName = "%SourceDataDir%/OSM/road.shp"
,  StorageType = "gdal.vect"
{
   attribute&lt;point_rd&gt; geometry (arc);
   attribute&lt;string&gt;   name;
}
</pre>

## see also
- [[point]]
- [[polygon]]
- [[vector data]]
- [[grid data]]
