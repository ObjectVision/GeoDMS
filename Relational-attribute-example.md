*[[Configuration examples]] Relational attribute*

[[Relational attributes|relation]] are used to relate [[domains|domain unit]]. They are use in many calculations, especially in [[aggregation|aggregation functions]] and  [[relational functions]].

Relational attributes can be made in different ways, two examples that occur often are presented here

## example 1, relation based on (external) code attributes

<pre>
container administrative
{
   unit&lt;uint32&gt; neighborhood
   :  StorageName = "%SourceDataDir%/CBS/2017/neighborhood"
   ,  StorageType = "gdal.vect"
  {
     attribute&lt;string&gt;       municipality_code;
     attribute&lt;municipality&gt; <B>municipality_rel</B> := rlookup(municipality_code, municipality/values);
  }
  unit&lt;uint32&gt; municipality := unique(neighborhood/municipality_code);
}
</pre>
*Explanation:*

The example presents how an external code can be used to configure a relation. In the example the external code municipality_code is read from a [[storage|source]]. Based on
this [[attribute]] the municipality domain unit is configured with the [[unique]] function. This results in a domain unit with the generated [[subitem]] values, containing all unique
municipality_codes in alphabetic order.

The resulting relation: **municipality_rel** attribute, configured with the [[rlookup]] values, results in the [[index numbers]] of the municipality domain for the neighborhood domain.

## example 2, relation based on geometry (point in polygon)

<pre>
container bag
{
   unit&lt;uint32&gt; vbo
   :  StorageName = "%SourceDataDir%/BAG/20170101/vbo.shp"
   ,  StorageType = "gdal.vect"
  {
     attribute&lt;point_rd&gt;     geometry;
     attribute&lt;municipality&gt; <B>municipality_rel</B> := point_in_polygon(geometry, municipality/geometry);
  }
  unit&lt;uint32&gt; municipality
   :  StorageName = "%SourceDataDir%/CSB/2017/municipality.shp"
   ,  StorageType = "gdal.vect"
  {
     attribute&lt;point_rd&gt; geometry (poly);
  }
}
</pre>

*Explanation:*

The example presents how a relation can be configured based on a point in polygon relation. In the example the geometry of a vbo (verblijfsobject in the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG)) and the geometry of municipalities are read from storages. The [[point_in_polygon]] function results in the [[index numbers]] of the municipality domain for the vbo domain. The relation indicates in which municipality the vbo is located.