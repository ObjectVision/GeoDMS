The SpatialReference [[property]] is used to configure a [[coordinate unit|How to configure a coordinate system]].

By configuring this property, the GeoDMS becomes aware of the coordinate system and how to interpret the values.
With GDAL it is easy to [[convert]] coordinates between coordinate systems.
 
In a GeoDMS Map view only coordinates of one coordinate system can be combined. If an EPSG code is configured, it is shown in the title of the Map View in which these coordinates are used. 

_Until 8.7.0 the format property was used instead of SpatialReference._

## example
The following example shows how to configure the Dutch coordinate system ([Rijksdriehoekmeting](https://nl.wikipedia.org/wiki/Rijksdriehoekscoördinaten)):

<pre>
unit&lt;float32&gt; m := baseunit('m', float32);
unit&lt;fpoint&gt; point_rd_base: <B>SpatialReference = "EPSG:28992"</B>; 
</pre>