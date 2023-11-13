A feature [[attribute]] is a an attribute referring to a [[geographic|geography]] location. It refers to <B>one or a sequence of coordinates</B>.
The feature type for a country is usually the coordinates of the boundary, for a road the start/end location and one or more intermediates and for an address in the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG) the coordinate of the location. 

The [[value type]] of a feature attribute is a value type from the PointGroup.

Feature attributes are in use for [[vector data]], referring to [[point]], [[arc]] or [[polygon]] geometries.

Although the concept feature attribute contains the word attribute, it can also be used for [[parameters|parameter]]. 

For feature attributes it is advised to use the name: [[geometry]]. If another name is used, the feature attribute need to be configured for the vector [[domain unit]] with the following syntax:

<pre>
unit&lt;uint32&gt; house
: DialogType = "<B>map</B>"
, DialogData = "<B>location</B>"
{ 
   attribute&lt;point_rd&gt; location;
}
</pre>

In this example the name location is used as feature attribute. As this is not the default name geometry, the configuration with the [[DialogType]] and DialogData properties is necessary.

If the feature attribute is called (G)(g)eometry, the properties: DialogType and DialogData do not have to be configured (since version 7.206).

## composition type

The distinction between point, arc or polygon data is made by configuring the [[composition type|composition]]. The composition type options are:
- not configured for point coordinates
- _arc_ for arc coordinates
- _poly(gon)_ for coordinates