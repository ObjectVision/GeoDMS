[[Geographic|Geography]] [[coordinate systems|coordinate system]] are configured with a set of related [[units|unit]].

## examples
The following examples shows how to configure the Dutch coordinate system ([Rijksdriehoekmeting](https://nl.wikipedia.org/wiki/Rijksdriehoekscoördinaten)):

_Until 8.7.0 the format property was used instead of SpatialReference._

1) default order of Y, X in point functions, see [[XY order]]: 
<pre>
unit&lt;float32&gt; m := baseunit('m', float32);
unit&lt;fpoint&gt; point_rd_base
 :  DialogData       = "ngr_layer"
 ,  SpatialReference = "EPSG:28992"; 

unit&lt;fpoint&gt; point_rd := range(point_rd_base, point(300000[m],0[m]), point(625000[m],280000[m]));
</pre>


2) X, Y order in point functions, see [[XY order]]:
<pre>
unit&lt;float32&gt; m := baseunit('m', float32);
unit&lt;fpoint&gt; point_rd_base
 :  DialogData       = "ngr_layer"
 ,  SpatialReference = "EPSG:28992"; 

unit&lt;fpoint&gt; point_rd := range(point_rd_base, point(0[m],300000[m]), point(280000[m],625000[m]));
</pre>

### explanation

Three units are configured:

1. m: a [[baseunit]] for items with as [[metric]]: meter. The coordinates in the Rijksdriehoekmeting are expressed in meters.
2. point_rd_base: the coordinate sytem base unit. The SpatialReference property for this unit refers to [EPSG:28992](https://en.wikipedia.org/wiki/EPSG_Geodetic_Parameter_Dataset), the EPSG code for the Rijksdriehoekmeting. This SpatialReference property is in use for descriptive purposes and can be used for coordinate transformations. The DialogData property for this unit refers to the background layer, in the case a [[WMS layer|WMS background layer]].
3.  point_rd: the coordinate system unit configured with an [[expression]], defining the range of allowed values. The first argument of this [[range]] function refers to the coordinate system base unit, relating the set of allowed values to the coordinate system used, in this case: Rijksdriehoekmeting.

If coordinate systems are configured in this way, [[vector data]] with point_rd as [[values unit]] for it's [[feature attributes|feature attribute]] and [[grid data]] related to point_rd can be combined in the same map view. The GeoDMS supports [[operators and functions]] for coordinate conversions, especially from and to coordinates in the Rijksdriehoekmeting, see [[Geometric functions]].