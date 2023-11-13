- point [[data items|data item]] are data items with a two-dimensional [[value type]] (PointGroup) and a **single coordinate** for each entry.
- [[point function]] is a function creating point data items.

The next topics on this page describe the point data item. The point function is described on the [[point function]] page.

In the GeoDMS point data items are used for:

## vector data

[[images/point_format.png]]

The [[feature attribute]] of point [[vector data]] always refers to **one** coordinate for each element in the [[domain unit]].

Vector point data is often read from a [[ESRI Shapefile]] or [[Geopackage]]. X and Y [[attributes|attribute]] can also easily be read as numeric attributes from a [[data source]] and combined with a [[point function]] to a [[feature attribute]], as in the next example.

<pre>
unit&lt;uint32&gt; residence
:  StorageName = "=System/DbName"
,  SqlString   = "SELECT * FROM Residences ORDER BY id"
{
   attribute&lt;coord_rd&gt; x;
   attribute&lt;coord_rd&gt; y;
   attribute&lt;point_rd&gt; geometry := point(x, y, point_rd);
}
</pre>

The x and y attributes are read from the data source with as values unit coord_rd. The [[geometry]] [[data item]] combines both x and y
coordinate with the point function to create a two dimensional data item of coordinates. The [[values unit]] of this [[geometry]] attribute is
point_rd, defining the coordinate system.

Subitems of the location item can be configured to define multiple [[visualisation styles|Visualisation Style]].

## grid data

As the nature of [[grid data|grid domain]] is two-dimensional, the identification of a grid cell in a Grid Domain is also two-dimensional.

## see also
- [[arc]]
- [[polygon]]
- [[vector data]]
- [[grid data]]