The [(Geo)Tiff](https://en.wikipedia.org/wiki/GeoTIFF) format is our advised format for [[grid data]], as it:
- is open, widely used, binary(fast) and supports multiple compression types.
- allows geo-referencing projection information to be embedded with (in an accompanying .tfw world file in the same folder) or within the GeoTiff file.

(Geo)Tiff files can be read and written to with the GeoDMS.

## Read
The GeoDMS supports two ways of reading (Geo)Tiff files:
- gdal.grid: for most (Geo)Tiff files, we advice to use gdal.grid (see next subparagraph), except for:
    - uint2, uint4 and boolean data.
    - palette based (Geo)Tiff files
- tiff StorageManager

### gdal.grid
The following example shows how to read a (Geo)Tiff file with [[gdal.grid]]. 

The (GeoTiff) file contains the data of a tile of the [AHN](https://www.ahn.nl), in this case: 26az2.tiff.

#### example
<pre>
unit&lt;spoint&gt; AHNTile
:   StorageName = "%SourceDataDir%/AHN/26az2.tif" 
,   StorageType = "gdal.grid" 
,   DialogData  = "Geografie/rdc" 
{    
   attribute&lt;float32&gt; GridData;    
   attribute&lt;float32&gt; ReadData (subdomain); 
}
</pre>

The primary data is read from the 26az2.tif file. The [[projection]] information can also be read from this file, or from a 26az2.tfw file in the same folder. The DialogData property needs to refer to the [[unit for the coordinate system|how to configure a coordinate system]] (RD, LatLong
etc) to inform the GeoDMS on how to relate the projection information from the GeoTiff to the coordinate system of the configuration.

The [[geographic grid domain]] AHNTile is implicitly defined by the projection information and the number of rows/columns of the GeoTiff. 

The _GridData_ [[attribute]] has this AHNTile as [[domain unit]] and is read for the whole grid.

The _ReadData_ attribute reads a subset of the data for another domain unit, in the example subdomain. This geographic domain unit needs to be configured and can be used to read for example the AHN data for a building or the land use for a country from a European land use file. If only a (small) selection of a GeoTiff file is needed, using such a _ReadData_ attribute as it is much faster than reading the GridData and making the selection afterwards.

Data read from a GeoTiff is automatically [[tiled|tile]] in the GeoDMS.

#### options

[[GDAL options]] can be configured for reading (Geo)Tiff files with gdal.grid. 

See: <https://gdal.org/drivers/raster/gtiff.html#open-options> for a full list of all open options.

### tiff StorageManager

#### example 
The following example shows how to read a (Geo)Tiff file with the tiff [[StorageManager]].

The (GeoTiff) file is one tile of the [AHN](https://www.ahn.nl), in this case: 26az2.tiff.

<pre>
unit&lt;spoint&gt; AHNTile
:   StorageName = "%SourceDataDir%/AHN/26az2.tif" 
,   DialogData  = "Geografie/rdc" 
{
   attribute&lt;float32&gt; GridData;
   attribute&lt;float32&gt; ReadData (subdomain); 
}
</pre>

The configuration is quite similar to the [[gdal.grid]] variant, only the StorageType does not have to be configured. 

As [[GDAL]] does not support [[Sub Byte Element]] GeoTiff files, we advice to use the Tiff StorageManager to read boolean, uint2 and uint4 data. 

### TrueColor

Both the gdal.vect and the tiff StorageManager can read TrueColor (Geo)Tiff files.

The essence of a TrueColor Tiff is that the GridData contains a color value (rgb code) for each grid cell. The GeoDMS supports TrueColor Bitmap/GeoTiff images with 24 bits per pixel (1 byte for red, green and blue values). The [[value type]] for the grid data values in the GeoDMS needs to be: uint32.

#### example:

<pre>
unit&lt;spoint&gt; referentie_tiff
:  StorageName = "%projDir%/data/tif/kaart.tif"
,  DialogData  = "Geography/point_rd"
{
   attribute&lt;uint32&gt; GridData: DialogType ="BrushColor";
}
</pre>

The property DialogType = "BrushColor" is configured to inform the GeoDMS the grid values are interpreted as colors and can be used directly to visualise grid cells in the map view.

### palette based

In this variant the GridData [[attribute]] contain index numbers for each grid cell. These index numbers are related to colors in a palette. The palette is also stored in the GeoTiff file. Based on the number of entries in the palette (related to the number of different colors in the file), the griddata of a palette based GeoTiff is usually configured with value type uint4 (for 16 colors) or uint8 (for a maximum of 255 colors).

Palette based can at the moment only be read with the tiff StorageManager.

#### example:

<pre>
unit&lt;uint4&gt; colorindex;
{
   attribute&lt;uint32&gt; palette := Grid/PaletteData,  DialogType = "BrushColor";
}
unit&lt;spoint&gt; referentie_tiff
:   StorageName = "%projdir%/data/tiff_pa.tif"
,   DialogData  = "Geography/point_rd"                    
{
   attribute&lt;colorindex&gt;  GridData;
   attribute&lt;units/color&gt; PaletteData (colorindex);
}
</pre>

The subitem named GridData refers to the index numbers. The subitem named PaletteData is an attribute with a domain unit that is unifiable with the values unit of the GridData attribute. The PaletteData is also read from or written to the Tiff file.

The property DialogType = "BrushColor" is configured for the palette subitem of the palette domain to inform the GeoDMS that the palette values are to be interpreted as colors when visualizing the grid cells in a map view.

Other palettes (not directly read from the Tiff file) can also be used, e.g. defined explicitly in the GeoDMS configuration.

### SqlString

the SqlString [[property]] is in use for (Geo)Tiff files for 2 purposes:

1) Aggregations
If a SqlString = "number" is configured, a relation is made (if possible) between the grid data domain and the result domain based on the configured projection. This relation is used to count the number of occurrences of the configured class number in the original Tiff file for the new desired grid cell.

2) Multiple bands
The following example shows how to read multiple bands (only with gdal.grid)
<pre> 
unit&lt;uint32&gt; grid_uint8_nth_band
:  StorageName     = "%SourceDataDir%/intput/tiff_uint32.tif"
,  StorageType     = "gdal.grid"
,  DialogData      = "units/point_rd"
,  StorageReadOnly = "True"
{
   attribute&lt;uint8&gt; Red     : SqlString="BANDS=1";
   attribute&lt;uint8&gt; Blue    : SqlString="BANDS=2";
   attribute&lt;uint8&gt; Green   : SqlString="BANDS=3";
   attribute&lt;uint8&gt; Magenta : SqlString="BANDS=4";
}
</pre>

## Write

The GeoDMS supports two ways of writing (Geo)Tiff files:
- gdalwrite.grid: for most (Geo)Tiff files, we advice to use gdalwrite.grid (see next subparagraph), except for:
    - uint2, uint4 and boolean data.
    - palette based (Geo)Tiff files
- tiff StorageManager

### gdalwrite.grid

**!!! We noticed some issues with writing GeoTiff files with gdalwrite.grid. We advices to use the tiff StorageManager instead of gdalwrite.grid at the moment.**

The following example shows how to write a (Geo)Tiff file with [[gdalwrite.grid]].

#### example
<pre>
unit&lt;spoint&gt; Export_AHNTile := AHNTile
: StorageName = "%LocalDataProjDir%/AHN/26az2.tif" 
, StorageType = "gdalwrite.grid"
{
   attribute&lt;float32&gt; GridData := AHNTile/Export_AHNTile;
}
</pre>

#### options

[[GDAL options]] can be configured for writing (Geo)Tiff files with gdal.grid.

See: <https://gdal.org/drivers/raster/gtiff.html#creation-options> for a full list of all creation options.

### tiff StorageManager

The following example shows how to write a (Geo)Tiff file with the tiff.

#### example 
<pre>
unit&lt;spoint&gt; AHNTile
: StorageName = "%LocalDataProjDir%/AHN/26az2.tif" 
{
   attribute&lt;float32&gt; GridData:= AHNTile/Export_AHNTile;
}
</pre>

In the same way as with reading, the configuration for writing GeoTiff files is quite similar to the [[gdalwrite.grid]] variant, only the StorageType does not have to be configured.

As [[GDAL]] does not support [[Sub Byte Element]] GeoTiff files, we advice to use the Tiff StorageManager to write boolean, uint2 and uint4 data. 

### palette based

In this variant the GridData [[attribute]] contain index numbers for each grid cell. These index numbers are related to colors in a palette. The palette is also stored in the GeoTiff file. Based on the number of entries in the palette (related to the number of different colors in the file), the griddata of a palette based GeoTiff is usually configured with value type uint4 (for 16 colors) or uint8 (for a maximum of 255 colors).

Palette based can at the moment only be written with the tiff StorageManager.

#### example:

<pre>
unit&lt;uint4&gt; colorindex
{
   attribute&lt;units/color&gt; palette := Grid/PaletteData, DialogType = "BrushColor";
}
unit&lt;spoint&gt; Read:
   StorageName     = "%projdir%/data/tiff_pa.tif", 
   DialogData      = "units/point_rd",
   StorageReadOnly = "True"
{
   attribute&lt;colorindex&gt;  GridData;
   attribute&lt;units/color&gt; PaletteData (colorindex);
}
container Write: StorageName = "%localDataProjDir%/regr_results/tiff_pa.tif", ExplicitSuppliers = "Read"
{
   attribute&lt;colorindex&gt;  GridData    (Grid)       := Grid/GridData;
   attribute&lt;units/color&gt; PaletteData (colorindex) := Grid/PaletteData;
}
</pre>

