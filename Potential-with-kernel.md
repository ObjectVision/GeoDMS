*[[Configuration examples]] Potential with Kernel*

The potential function performs a [neighborhood operation](https://geogra.uah.es/patxi/gisweb/NOModule/NOtheorylesson.htm).
It computes an output grid where the resulting value for each cell is the sum of the values of all the input cells within a specified neighborhood around that cell.

This specified neighborhood is defined by the [[kernel]] (also known as distance matrix). In ArcGIS the operation is known as [focalsum](http://webhelp.esri.com/arcgisdesktop/9.3/index.cfm?TopicName=focalsum) (part of focal statistics).

Within the GeoDMS [[potential calculations|Neighbourhood Potential]] are implemented using a [[Convolution]] by [Fast Fourier Transformation (FFT) algorithm](https://nl.wikipedia.org/wiki/Fast_Fourier_transform).
This makes potential calculations much faster than [focalsum](http://webhelp.esri.com/arcgisdesktop/9.3/index.cfm?TopicName=focalsum) calculations, especially with large kernels.

## in use for

Neighborhood operations are for instance used to:

-   model the attractivity of a [[land use]] type in a suitability map based on the distance to for example an airport or a shopping area. a distance decay function can be used in the kernel to model the diminishing attractivity.
-   spread out densities of target groups, for instance inhabitants above 65 years old. They are often concentrated on a few locations (old age or nursing homes). The potential operation can be used to get a clear view of the density of an area and help to prevent in tracing back data to individual persons (often not allowed under
    [AVG](https://nl.wikipedia.org/wiki/Algemene_verordening_gegevensbescherming)/[GDPR](https://en.wikipedia.org/wiki/General_Data_Protection_Regulation) regulations).

## example

The example originates from a performance contest of the [Spatial Analysis department of the Vrije Universiteit Amsterdam](https://spinlab.vu.nl). In the contest a 1 km world wide grid and a 50 km kernel were used.

<pre>
container contest
{   
   unit&lt;fpoint&gt;  WorldBaseUnit;
   unit&lt;float32&gt; potential;

   container Kernel
   {
      unit&lt;uint32&gt; Dist2Range;
      unit&lt;spoint&gt; pot50km := range(spoint, point(-50s, -50s), point(51s, 51s))
      {
         attribute&lt;Dist2Range&gt; distMatr  := dist2(point(0s, 0s, .), Dist2Range);
         attribute&lt;Potentiaal&gt; AbsWeight := distMatr <= 2500 ? 1s : 0s;
      }
   }
   container SourceData
   {
      unit&lt;wpoint&gt; wetlands
      :   Descr           = "Arc/Info binary grid"
      ,   StorageName     = "%sourcedatadir%/SpaceToGo/wetlands1"
      ,   StorageType     = "gdal.grid"
      ,   DialogData      = "WorldBaseUnit"
      ,   StorageReadOnly = "True"
      {
          unit&lt;wpoint&gt;     World1kmGrid := TiledUnit(point(1024w, 1024w,.));
          attribute&lt;uint8&gt; ReadData  (World1kmGrid);
          attribute&lt;bool&gt;  IsWetLand (World1kmGrid) := ReadData == 1b;
      }
  }
  container Result
  {
    <B>attribute&lt;float32&gt; Potential50km (SourceData/wetlands/World1kmGrid) := 
       potential(
            float32(SourceData/wetlands/IsWetLand)
           ,float32(kernel/pot50km/AbsWeight)</B>
       )
     ,  StorageName = "%LocalDataProjDir%/Potential50km_float32.tif";
   }
}</B>

</pre>

## explanation

In the example two [[grid domains|grid domain]] are in use
 
1.  A [[geographic|geography]] grid with values from which the attribute *isWetland* is derived, read from an [[Arc Info Binary Grid]]. The files for this grid are stored in a *%SourceDataDir%/SpaceToGo* folder.
2.  A non geographic grid domain (the values are not related to a location on the earth surface), a so-called [[kernel]], in the example named: *pot50km*.

The actual [[potential]] calculations is perfomed in the bold line. Both [[arguments|argument]] are converted to float32 [[value types|value type]], as this is required for the potential function.

The calculated results are stored in a [[GeoTiff]] file called: *potential50km_tiff*.

## performance

Using the [focalsum](http://webhelp.esri.com/arcgisdesktop/9.3/index.cfm?TopicName=focalsum) function in ArcGIS, this example from the contest took more than 24 hours to calculate. In the GeoDMS, on a similar machine, it took a few minutes to calculate the same results with the potential function.