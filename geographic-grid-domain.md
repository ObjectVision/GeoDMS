The GeoDMS calculates and visualizes [geographic](https://github.com/ObjectVision/GeoDMS/wiki/Geography) data.

To combine grids with other [[grid|grid data]] or [[vector data]], they need refer to the same [geographical coordinate system](https://en.wikipedia.org/wiki/Geographic_coordinate_system).

[[Grid data]] is related to a [geographical coordinate system](https://en.wikipedia.org/wiki/Geographic_coordinate_system) by it's domain unit ([[grid domain]]), defining the [[cardinality]] (for grid domains the number of rows and columns). Additionally a geographic grid domain needs to be aware of:
- the [[values unit]] of the geographical [[coordinate system]]
- [[projection information|projection]] on how to relate the local grid coordinates to world coordinates, in a specific coordinate system.

_A geographic domain unit is a [[grid domain]] related to a a rectangle part of the world._

## example

The following picture shows the relevant parameters for the configuration or a geographic grid domain.

[[images/Grid_domain.png]]

The following information is needed to configure the grid domain for this example:  
- the [[values unit]] for the coordinate system is [rdc_m](https://nl.wikipedia.org/wiki/Rijksdriehoeksco%C3%B6rdinaten), expressing coordinates in meters 
- the cardinality is defined by the number or rows (3.250) and number of columns (2.700)
- the projection information:
    - pixel size in the x-direction (in meter): 100
    - pixel size in the y-direction (in meter): -100
    - x-coordinate (in meter) of the center of the upper left pixel: 10.000
    - y-coordinate (in meter) of the center of the upper left pixel: 625.000

The GeoDMS supports two ways of configuring this information:

### 1) projection and cardinality read from data source

The projection information and cardinality of a geographical domain unit can be read from a data source with a GeoDMS raster [[StorageManager]], see for example [[GeoTiff]]. Use the [[DialogData]] [[property]] to relate the [[unit]] to the used coordinate system.

This way of configuring is easier and less error-prone as the explicit configuration with GeoDMS functions (2). 

### 2) explicit configuration with GeoDMS functions

In two cases it is needed to configure the projection information and cardinality explicitly (2):

1. if another geographic domain unit is needed as for which the data is stored in the data source, a selection that can be read from a Tiff source with a ReadData attribute.
2. if a fixed domain is needed and the source files need to be checked if their projection information and cardinality corresponds with this configured domain.

The following examples present the configuration of the geographic grid domain for the example.

<details><summary>
example (default order of Y, X in point functions, see [[XY order]]):
</summary>

<pre>
container grid
{
   // configure the units for the coordinate system
   unit&lt;float32&gt; m:= baseunit('m', float32);
   unit&lt;fpoint&gt;  rdc_meter_base
      : DialogData = "ngr_layer" 
      , SpatialReference<sup>1</sup> = "EPSG:28992";
   unit&lt;fpoint&gt; rdc_meter := range(rdc_meter_base, point(300000[m], 0[m]), point(625000[m],280000[m]));

   // configure the cardinality for the grid domain
   parameter&lt;int16&gt; nrofrows := int16(3250);
   parameter&lt;int16&gt; nrofcols := int16(2700);

   // configure the grid domain
   unit&lt;spoint&gt; rdc_100m
   := range(
         gridset(
            rdc_meter
           ,point(float32(-100), float32(100), rdc_meter)
           ,point(float32(625000), float32(10000), rdc_meter)
           ,spoint
         )
         ,point(int16(0), int16(0))
         ,point(nrofrows, nrofcols)
      );
}
1) before version 8.70, use the property Format in stead of SpatialReference.
</pre>
</details>

<details><summary>
example (X, Y order in point functions, see [[XY order]]):
</summary>

<pre>
container grid
{
   // configure the units for the coordinate system
   unit&lt;float32&gt; m:= baseunit('m', float32);
   unit&lt;fpoint&gt;  rdc_meter_base
      : DialogData = "ngr_layer" 
      , SpatialReference<sup>1</sup> = "EPSG:28992";
   unit&lt;fpoint&gt; rdc_meter := range(rdc_meter_base, point(0[m], 300000[m]), point(280000[m],625000[m]))

   // configure the cardinality for the grid domain
   parameter&lt;int16&gt; nrofrows := int16(3250);
   parameter&lt;int16&gt; nrofcols := int16(2700);

   // configure the grid domain
   unit&lt;spoint&gt; rdc_100m
   := range(
         gridset(
            rdc_meter
           ,point(float32(100), float32(-100), rdc_meter)
           ,point(float32(10000), float32(625000), rdc_meter)
           ,spoint
         )
         ,point(int16(0), int16(0))
         ,point(nrofcols, nrofrows)
      );
}
1) before version 8.70, use the property Format in stead of SpatialReference.
</pre>
</details>

#### explanation:

- The rdc_meter unit defines the unit for the coordinate system (rijksdriehoek coordinates).
- The [[expression]] for the geographic grid domain consists of two components:

1. a [[range]] function resulting in a range of rdc_meter coordinates. The range function has three [[arguments|argument]]:
    1. the result of the configured [[gridset]] function.
    2. the origin coordinate of the grid, expressed in the local grid coordinates.
    3. the number of columns and number of rows, expressed in the values type of the local grid coordinates.
2. a [[gridset]] function, defining the relation from the geographic coordinates to the local grid coordinates. This function has four arguments:
    1. the unit of the coordinate system, in this example rdc_meter. 
    2. the gridsize, in this case 100 meter in both X and Y directions.
       The gridsize in both directions is configured with a point function in the values typeof the rdc_meter coordinates.
        1. In X direction, the coordinate values are increasing so a positive value of 100 is configured in this direction.
        2. In Y direction, the coordinates values are decreasing so a negative value of -100 is configured in this direction.
    3. the LeftTop(origin) coordinate in in the values type of the rdc_meter.
    4. the values type of the resulting gridset, in this case spoint.

Each attribute configured with this rdc_100m unit as domain unit should always have 3,250 rows and 2,700 columns.

In the [[gridset]] functions, do not use arguments that are read from large primary data files, see the [[gridset]] documentation. 