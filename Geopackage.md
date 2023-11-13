A [geopackage (GPKG)](https://en.wikipedia.org/wiki/GeoPackage) is an open, non-proprietary, platform-independent and standards-based data format for geographic information system implemented as a [SQLite database](wikipedia:SQLite "wikilink") container.

The format has become popular lately and is for instance used for basisregistraties such as the [BRT](https://www.pdok.nl/downloads/-/article/basisregistratie-topografie-brt-topnl).

Geopackages can be read and written to with the GeoDMS, an example is the snapshot from the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG) that we make available as geopakacge for the <https://geoparaat.nl> project.

See https://gdal.org/drivers/vector/gpkg.html#vector-gpkg for a full list of all open and creation [[GDAL options]].

## Read

Since GeoDMS version 7408, [[gdal.vect]] can be used to read geopackages.

#### example:
<pre>
container snapshot_result
:  StorageName     = "%LocalDataProjDir%/snapshot_gpkg_20220101.gpkg"
,  <B>StorageType     = "gdal.vect"</B>
,  StorageReadOnly = "True"
,  SyncMode        = "allTables",
,  DialogData      = "geography/point_rd";
</pre>

This configures a whole geopackage. The SyncMode [[property]] indicates all tables are read from the geopackage. All tables become [[domain units|domain unit]] and [[parent items|parent item]] for all [[attributes|attribute]]. These attributes can be configured explicitly or are read with a default [[value type]].

The configuration of the [[DialogData]] [[property]] informs the GeoDMS the [[values unit]] for all [[geometry]] items will be: *geography/point_rd*. 

## Write

Since GeoDMS version 7408, [[gdalwrite.vect]] can be used to write geopackages.

#### examples:
<pre>
<I>1. single layer:</I>

unit&lt;uint32&gt; location := SourceData/location 
, StorageName     = "%localDataProjDir%/regr_results/test.gpkg"
, <B>StorageType     = "gdalwrite.vect"</B>
, StorageReadOnly = "False"
{
   attribute<dpoint> geometry := SourceData/location/geometry;
   attribute<string> name     := SourceData/location/name;
}

<I>2. multiple layers:</I>

container snapshot_result
:  StorageName     = "%LocalDataProjDir%/snapshot_gpkg_20220101.gpkg"
,  <B>StorageType     = "gdalwrite.vect"</B>
,  StorageReadOnly = "False"
{
   unit&lt;uint32&gt; ligplaats := selectie/ligplaats
   {
      attribute&lt;string&gt;         identificatie       := selectie/ligplaats/identificatie;
      attribute&lt;geometries/rdc&gt; geometry     (poly) := selectie/ligplaats/geometry_mm[geometries/rdc];
      attribute&lt;string&gt;         nummeraanduiding_id := selectie/ligplaats/nummeraanduiding_id;

      container meta
      {
          attribute&lt;string&gt;                status     (ligplaats) 
             := selectie/ligplaats/meta/status, DisableStorage = "True";
          attribute&lt;ligplaats_status_code&gt; statusCode (ligplaats) 
             := rlookup(Status, ligplaats_status_code/values);
      }
   }

   unit&lt;uint32&gt; ligplaats_status_code := unique(ligplaats/meta/status)
   {
      attribute&lt;uint32&gt; code  := id(.);
      attribute&lt;string&gt; label := values;
   }
...
}
</pre>

This example shows a part of the configuration of a BAG snapshot written to a geopackage.

All domain units configured become tables in the geopackage, and all direct or indirect [[subitems|Subitem]] with the same domain unit are stored as attributes in these tables.

By configuring the [[DisableStorage]] = "True" property for the meta status attribute, the attribute is not written to the geopackage.