[XML](https://en.wikipedia.org/wiki/XML%7CXML) (Extensible Markup Language) is a markup language often used as exchange format for primary
data. With the GeoDMS data can be read from xml files in two steps:

1.  Use the [[str StorageManager]] to read the xml data in a string [[data items|data item]].
2.  Use the [[parse_xml]] function to convert the  xml data with an xml scheme into a set of configured [[data items|data item]].

## Gml

Geographic vector data can also be stored in XML files, often embedded in [GML](https://en.wikipedia.org/wiki/Geography_Markup_Language). GML
is the XML grammar defined by the [Open Geospatial Consortium (OGC)](https://www.ogc.org) to express geographical features.

The GeoDMS can read this type of data from XML/GML files, but it requires some processing (especially for polygon data).
The following examples show how to read and process this type of data to use it in GeoDMS projects:

-   [[Polygon features from XML GML files]] (BAG panden)
-   [[Point features from XML GML files]] (BAG verblijfsobjecten)

Data can be read from a gml file, but it is important to configure the [[composition type|composition]] explicitly. 

An example like:
<pre>
unit&lt;uint32&gt; poly_gml
:  StorageName     = "%projDir%/data/inspire-pv-ps.nlps-nnn.gml"
,  StorageType     = "gdal.vect"
,  StorageReadOnly = "True"
{
}
</pre>

will result in a geometry as well know text, but not as [[polygon]] data.

Configure such a file as follows:

<pre>
unit&lt;uint32&gt; poly_gml
:  StorageName     = "%projDir%/data/inspire-pv-ps.nlps-nnn.gml"
,  StorageType     = "gdal.vect"
,  StorageReadOnly = "True"
{
     attribute&lt;fpoint&gt; geometry (poly);
}
</pre>

where you can replace fpoint by the [[unit for the coordinate system|How-to-configure-a-coordinate-system]] used.
