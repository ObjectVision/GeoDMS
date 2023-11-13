*[[Configuration examples]] CBS gemeente/wijk/buurt kaart*

In many applications the CBS gemeente/wijk/buurt maps are used to visualise data at the municipality, quarter or neighborhood level or to use data available at these levels.

The data can de downloaded from the CBS website, e.g. for the year 2022 from the downloads paragraph of: 
https://www.cbs.nl/nl-nl/dossier/nederland-regionaal/geografische-data/wijk-en-buurtkaart-2022

After downloading, unzip the files to a folder, e.g. _%SourceDataDir%/CBS/2022_. 

The data is available in [[ESRI Shapefile]] (example 1) and in [[Geopackage]] format (example 2).

## example 1: Shapefiles

<pre>
unit&lt;float32&gt; m             := baseunit('meter', float32);
unit&lt;fpoint&gt;  point_rd_base : Format = "EPSG:28992";
unit&lt;fpoint&gt;  point_rd      := range(point_rd_base, point(300000[m], 0[m]), point(625000[m], 280000[m]));

container SourceData
{
   container CBS_2022
   {
      unit&lt;uint32&gt; gemeenten: StorageName = "%SourceDataDir%/CBS/2022/gemeente_2022_v1.shp"
      ,  StorageType     = "gdal.vect"
      ,	 StorageReadOnly = "True"
      {
         attribute&lt;point_rd&gt; geometry (polygon);
      }

      unit&lt;uint32&gt; wijk: StorageName = "%SourceDataDir%/CBS/2022/wijk_2022_v1.shp"
      ,  StorageType     = "gdal.vect"
      ,  StorageReadOnly = "True"
      {
         attribute&lt;point_rd&gt; geometry (polygon);
      }

      unit&lt;uint32&gt; buurt: StorageName = "%SourceDataDir%/CBS/2022/buurt_2022_v1.shp"
      ,  StorageType     = "gdal.vect"
      ,  StorageReadOnly = "True"
      {
         attribute&lt;point_rd&gt; geometry (polygon);
      }
   }
}
</pre>

## example 2: GeoPackage
<pre>
   container CBS_2022_gpkg: StorageName = "%SourceDataDir%/CBS/2022/wijkenbuurten_2022_v1.gpkg"
   ,  StorageType     = "gdal.vect"
   ,  StorageReadOnly = "True"
   ,  SyncMode        = "AllTables"
   ,  DialogData      = "point_rd";
</pre>

In this GeoPackage, the gemeente, wijk & buurt data is available. Use the _SyncMode = "AllTables"_ configuration to read all items from the geopackage. The [[DialogData]] [[property]] is used to inform the GeoDMS that all geographic coordinates of the polygons have as [[values unit]] point_rd (rijksdriehoekmeting).  
Note that the naming in the gpkg differs from the names of the shape files: In this GeoPackage, the plural names are used: gemeenten, wijken & buurten, and other descriptive field names are used, e.g. Gemeentenaam instead of GM_NAAM.
