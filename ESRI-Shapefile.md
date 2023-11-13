[ESRI Shapefiles](https://en.wikipedia.org/wiki/Shapefile) are used to store [[vector data]] and related [[attribute]] data.

The ESRI Shapefile format always consists of (at least) three accompanying files, with the extensions:
- .shp: containing the geographical coordinates ([[feature attribute]]).
- .shx: containing a spatial index.
- .dbf: containing tabular information describing the shapes. If a logical entity (like countries) consists of multiple shapes, the relation between the shapes and the logical entity is usually stored in this .dbf file.

Optionally a .prj file is als available, containing the [projection](https://en.wikipedia.org/wiki/Map_projection) information.
This file is at the moment not (yet) used by the GeoDMS (projection information is configured explicitly in the GeoDMS).

The GeoDMS supports at the moment (multi)[[point]], [[arc]] and [[polygon]] types ([Shape types](https://en.wikipedia.org/wiki/Shapefile): 1,3, 5 and 8).

## Read

The GeoDMS supports two ways of reading ESRI Shapefiles:

- gdal.vect: for most shape files, we advice to use [[gdal.vect]] (see next subparagraph) to read csv files as it:
    - is faster
    - is more generic 
    - is shorter to configure
    - supports open options 
    - supports segmented data (If the file has more than 50.000 records, [[tiled domain]] with segments of maximum 50.000 entries will be made)
- shapefile/[[dbf]] [[StorageManager]]

### gdal.vect

#### examples 
<details><summary>how to configure reading point, polygon and arc data with gdal.vect:</summary>

<pre>
unit&lt;uint32&gt; location
:  StorageName = "%projDir%/vectordata/points.shp"
,  <B>StorageType = "gdal.vect"</B>
{
   attribute&lt;point_rd&gt; geometry;
   attribute&lt;string&gt;   name;
}

unit&lt;uint32&gt; region
:  StorageName = "%projDir%/vectordata/region.shp"
,  <B>StorageType = "gdal.vect"</B>
{
   attribute&lt;point_rd&gt; geometry (<B>polygon</B>);
   attribute&lt;string&gt;   name;
}

unit&lt;uint32&gt; road
:  StorageName = "%projDir%/vectordata/road.shp"
,  <B>StorageType = "gdal.vect"</B>
{
   attribute&lt;point_rd&gt; geometry (<B>arc</B>);
   attribute&lt;string&gt;   name;
}
</pre>

The .shp file is configured as [[StorageName]] for the [[domain unit]].

The .dbf and .shx files are not explicitly configured, but need to be available in the same folder as the .shp file. 

The difference between the configuration of points, arcs and polygons is the configured [[composition type|composition]].

If your dbf file already contains an attribute named geometry, use another attribute name for the data from the .shp file, for instance geom.
</details>

#### options

[[GDAL options]] can be configured for reading different ESRI Shapefiles. 

See: <https://gdal.org/drivers/vector/shapefile.html#open-options> for a full list of all open options.

### shapefile/dbf StorageManager

Shapefiles can also still be read with the GeoDMS specific shapefile/dbf StorageManagers (mainly for backward compatibility).

#### examples 
<details><summary>how to configure reading point, polygon and arc data with shapefile/dbf StorageManager:</summary>

<pre>
unit&lt;uint32&gt; location: StorageName = "<B>%projDir%/data/location.dbf</B>"
{
   attribute&lt;point_rd&gt; geometry: StorageName = "<B>%projDir%/data/location.shp</B>";
   attribute&lt;string&gt;   name;
}

unit&lt;uint32> region: StorageName = "<B>%projDir%/data/region.dbf</B>"
{
   attribute&lt;point_rd&gt; region (<B>polygon</B>): StorageName = "<B>%projDir%/data/region.shp</B>";
   attribute&lt;string&gt;   name;
}

unit&lt;uint32&gt; road:  StorageName = "<B>%projDir%/data/road.dbf</B>"
{
   attribute&lt;point_rd&gt; geometry (<B>arc</B>): StorageName = "<B>%projDir%/data/road.shp</B>";
   attribute&lt;string&gt;   name;
}
</pre>

The dbf file is configured as StorageName of the domain unit. The number of elements is read from this file. 

The feature attribute with the coordinates is configured as subitem of the domain unit. This attribute is read from the explicitly configured .shp file. The .shx file is not configured, the file needs to be available in the same folder as the .shp file.

The difference between the configuration of points, arcs and polygons is the configured [[composition type|composition]].
</details>

## Write

The GeoDMS supports two ways of writing ESRI Shapefiles:

- gdalwrite.vect: for most shape files, we advice to use [[gdalwrite.vect]] (see next subparagraph) to write csv files as it:
    - is faster
    - is more generic 
    - is shorter to configure
    - supports open options 
- shapefile/dbf StorageManager

### gdalwrite.vect

#### examples
<details><summary>how to configure writing point, polygon and arc data with gdal.vect:</summary>

<pre>
unit&lt;uint32&gt; location := SourceData/location
,  StorageName = "%LocalalDataProjDir%/export/points.shp"
,  <B>StorageType = "gdalwrite.vect"</B>
{
   attribute&lt;point_rd&gt; geometry := SourceData/location/geometry;
   attribute&lt;string&gt;   name     := SourceData/location/name;
}

unit&lt;uint32&gt; region := SourceData/region
:  StorageName = "%projDir%/vectordata/region.shp"
,  <B>StorageType = "gdalwrite.vect"</B>
{
   attribute&lt;point_rd&gt; geometry (<B>polygon</B>) := SourceData/region/geometry;
   attribute&lt;string&gt;   name               := SourceData/region/name;
}

unit&lt;uint32&gt; road
:  StorageName = "%projDir%/vectordata/road.shp"
,  <B>StorageType = "gdalwrite.vect"</B>
{
   attribute&lt;point_rd&gt; geometry (<B>arc</B>) := SourceData/road/geometry;
   attribute&lt;string&gt;   name           := SourceData/road/name;
}
</pre>

The [[feature attribute]] is written to the .shp file (in the examples _geometry_).

All other attributes (with supported .[[dbf]] [[value types|value type]]) are written to a filename with the same name and a .dbf extension. This applies to both the direct as the indirect [[subitems|Subitem]]. If multiple subitems with a PoinGroup [[value type]] occur, an error is generated. Set the [[DisableStorage]] [[property]] to True for the attributes you do not want to be written to the.dbf file.

A index file is also written together with the .shp file, with the same name but with the .shx extension.

The difference between the configuration of points, arcs and polygons is the configured [[composition type|composition]].
</details>

#### options

[[GDAL options]] can be configured for writing different ESRI Shapefiles. 

See: <https://gdal.org/drivers/vector/shapefile.html#layer-creation-options> for a full list of all creation options.

<details><summary>example on how to configure a write option:</summary>

<pre>
container with_index
{
   unit&lt;uint32> optionSet := range(uint32, 0, 1);
   attribute&lt;string> <B>GDAL_LayerCreationOptions (optionSet) : ["SPATIAL_INDEX=YES"]</B>;

   unit&lt;uint32&gt; location := SourceData/location
   ,  StorageName = "%LocalalDataProjDir%/export/points.shp"
   ,  StorageType = "gdalwrite.vect"
   {
      attribute&lt;point_rd&gt; geometry := SourceData/location/geometry;
      attribute&lt;string&gt;   name     := SourceData/location/name;
   }
}
</pre> 

This example shows how to export an ESRI Shapefile with a spatial index file (.qix) extensie.
</details> 

### shapefile/dbf StorageManager

Shapefiles can also still be written with the GeoDMS specific shapefile/dbf StorageManagers (mainly for backward compatibility).

#### examples 
<details><summary>how to configure writing point, polygon and arc data with shapefile/dbf StorageManager:</summary>

The example shows how to export an ESRI Shapefile with a spatial index file (.qix) extensie. Furthermore the resulting .dbf file is resized to an optimal size, this we advice especially for all ESRI Shapefiles to keep the size of the .dbf file limited. 

<pre>
container export
{
   unit&lt;uint32&gt optionSet := range(uint32, 0, 2);
   attribute&lt;string&gt; GDAL_LayerCreationOptions (optionSet) : ["SPATIAL_INDEX=YES","RESIZE=YES"];

   unit&lt;uint32&gt; location := SourceData/location, StorageName = "<B>%projDir%/data/location.dbf</B>"
   {
      attribute&lt;point_rd&gt; geometry := SourceData/Location/geometry, StorageName = "<B>%projDir%/data/location.shp</B>";
      attribute&lt;string&gt;   name     := SourceData/Location/name;
   }
   
   unit&lt;uint32> region := SourceData/region, StorageName = "<B>%projDir%/data/region.dbf</B>"
   {
      attribute&lt;point_rd&gt; region (<B>polygon</B>) := SourceData/region/geometry,  StorageName = "<B>%projDir%/data/region.shp</B>";
      attribute&lt;string&gt;   name             := SourceData/region/name;
   }

   unit&lt;uint32&gt; road := SourceData/road,  StorageName = "<B>%projDir%/data/road.dbf</B>"
   {
      attribute&lt;point_rd&gt; geometry (<B>arc</B>) := SourceData/road/geometry, StorageName = "<B>%projDir%/data/road.shp</B>";
      attribute&lt;string&gt;   name           := SourceData/road/name;
   }
}
</pre>

The dbf file is configured as StorageName of the domain unit. The attributes (with supported .[[dbf]] [[value types|value type]]) are written to the .dbf file with this filename.

The feature attribute is written to the explicitly configured .shp file. 

An index file is also written together with the .shp file, with the same name but with the .shx extension.

The difference between the configuration of points, arcs and polygons is the configured [[composition type|composition]].
</details>

