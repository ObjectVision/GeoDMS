## WMTS

Since version 7.163 the GeoDMS supports web mapping services ([WMTS](https://en.wikipedia.org/wiki/Web_Map_Tile_Service)) as a background layer. See [[WMTS background layer]] for examples on how to configure such a WMS webservice.

Using a WMTS background layer is advised for most projects, as it is a scale dependent and limits the amount of data that need to be shipped for a project.  

## data items
If a coordinate system is used without WMTS services, it is also possible to configure [[data items|data item]] as background layer. 

The background layer usually consists of a (set of) reference layers, they can be scale dependent. In the legend of the map view they are combined to one entry called Background.

The background layerset is configured with the [[DialogData]] property for the [[unit for the coordinate system|How to configure a coordinate system]].

*Example:*

<pre>
unit&lt;fpoint&gt; point_rd := baseunit('m', fpoint)
,  DialogData = "SourceData/reference/level_III/Amsterdam/map_bw;SourceData/reference/level_III/Utrecht/map_bw";
</pre>

In the example two [[data items|data item]] are configured as Background Layer, of which ten are scale dependent. A semicolon(;)
character is used as delimiter between the item names.

