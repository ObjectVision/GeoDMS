[[images/polygon_format.png]]

Polygon [[feature attributes|feature attribute]] are [[attributes|attribute]] with a two-dimensional [[value type]] (PointGroup) and **at least three coordinates** for each entry in the [[domain unit]]. The last coordinate need to be the same as the first coordinate (closed geometry).

Polygon attributes are used as feature attribute for [[vector data]]. 

The points in the correct sequence define a [polygon](https://en.wikipedia.org/wiki/Polygon), a surface defined between a start point, one or more intermediates and an end point. The start point and end point need to be identical.

Polygon attributes are often used for [[geographic|Geography]] coordinates of regions, countries etc.

To make the GeoDMS aware that a sequence of coordinates need to be interpreted as an polygon, this is done by configuring the [[composition type|composition]] to polygon.

## data model

In the GeoDMS data model a polygon is a sequence of points. The order of points in a sequence needs to be clockwise for exterior bounds (islands) and counter clockwise for holes in polygons (lakes).

For polygons with multiple rings (islands or holes), artificial lines are added between the rings, that are ignored when mapping the data in a mapview. But these artificial lines can effect [[arc]] oriented functions like [[arc_length]] and [[connect_info]].

We advice to configure the [[split_polygon]] function first on polygons with multiple rings and apply arc oriented functions on the result of the split polygon.

## example

The polygon example a describes a data structure where each element in a domain unit is related to one or multiple polygon geometries. In these cases two domain units are necessary (such a structure could also apply to arcs):

<pre>
container administrative
{
   unit&lt;uint8&gt; province
   :  NrofRows   = 13
   ,  DialogData = "ProvinceShapes/prvnr"
   ,  DialogType = "Map"
   {
      attribute&lt;string&gt; label: 
         ['Abroad','Groningen','Friesland','Drenthe','Overijssel','Gelderland','Utrecht',
          'Noord-Holland','Zuid-Holland','Zeeland','Noord-Brabant','Limburg','Flevoland'];`
   }
   unit&lt;uint32&gt; ProvinceShape: StorageName = "%projDir%/data/Geography/prv.dbf"
   {
      attribute&lt;point_rd&gt; geometry (polygon) : StorageName = "%projDir%/data/Geography/prv.shp";
      attribute&lt;province&gt; prvnr;
   }
}
</pre>

The following two [[domain units|domain unit]] are configured:

1.  The province unit is the domain unit of the administrative regions called provinces. The labels are configured for this domain unit as data item    in the configuration. The [[DialogType]] is set to Map and the DialogData to the prvnr [[relation]] referring to the ProvinceShapes domain unit.
2.  The ProvinceShape domain unit describes the total set of geometries/shapes. Some provinces like Friesland consist of multiple shapes (multiple islands). Therefore the number of elements of this ProvinceShape domain is larger than the number of elements of the province domain.

Each ProvinceShape element is related to one province. This relation, called prvnr is read from the .dbf file.

The feature attribute [[geometry]] is read from an [[ESRI Shapefile]] and has *polygon* configured as [[composition type|composition]].

## see also
- [[point]]
- [[arc]]
- [[vector data]]
- [[grid data]]
