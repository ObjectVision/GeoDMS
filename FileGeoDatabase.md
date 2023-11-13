The FileGeoDatabase is a database format from [ESRI](https://www.esri.com/en-us/home), mainly used by [ArcGIS](https://www.arcgis.com/index.html). It is a closed and non-documented format, but ESRI offers a Dynamic Link Library (DLL) with an API to open File Geodatabases, which is part of [[GDAL]].

## Read
Gdal.vect can be used to read a FileGeoDatabase. 

Example:
<pre>
container TestFGDB 
:   StorageName     = "%projdir%/data/test_fgdb.gdb"
,   <B>StorageType     = "gdal.vect"</B>
,   StorageReadOnly = "True"
,   SyncMode        = "AllTables"
{
    unit&lt;uint32&gt; Query: SqlString = "SELECT * FROM base_tbl WHERE OBJECTID_1 <= 3 ORDER BY id";
}
</pre>

This configuration results in a set of endogenous items for each table and each field in the test_fgdb File Geodatabase. For each table within
the GeoDMS a [[domain unit]] is created, used as parent and domain unit for the fields in this table. For each field an [[attribute]] is
created with a [[value type]] corresponding to the data type of the field. Geometry is used as default name for the [[feature attribute]] of a table. The [[composition]] type is derived from the meta information of the features.

The configured Query unit results in a new domain unit, based on a selection of the base_tbl. The * character in the [[SqlString]] indicates all fields are read from the table and for each field an endogenous attribute is created in the GeoDMS. Attributes can also be configured explicitly, but only if the values types of the field matches exactly with the configured values types (int32 for integer, float64 for floating points and string for strings).

## limitations in using file Geodatabases

1.  In general this format is not advised. The format is not open and if issues occur they can hardly be debugged.
2.  The GeoDMS only supports reading data from file Geodatabase, not writing to.
3.  The GeoDMS only reads [[vector data]], [[grid data]] can not be read.