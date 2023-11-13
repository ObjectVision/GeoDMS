The [pbf](https://gdal.org/drivers/vector/osm.html) format is used for [OSM](https://github.com/ObjectVision/AccessibilityModelling/wiki/OSM) data. The GeoDMS supports reading pbf data.

See [open options](https://gdal.org/drivers/vector/osm.html#open-options) for a full list of all open [[GDAL options]] for .pbf data.

## Read

Since GeoDMS version 7408, [gdal.vect](https://github.com/ObjectVision/GeoDMS/wiki/Gdal.vect) can be used to read geopackages.

#### example

<pre>
container osm
:  StorageName     = "%projdir%/data/flevoland-latest.osm.pbf"
,  StorageType     = "gdal.vect"
,  StorageReadOnly = "True"
,  SyncMode        = "AllTables"
{
}
</pre>

This configures a .pbf file. The SyncMode [property](https://github.com/ObjectVision/GeoDMS/wiki/Property) indicates all tables are read from the .pbf file. All tables become [domain units](https://github.com/ObjectVision/GeoDMS/wiki/Domain-unit) and [parent items](https://github.com/ObjectVision/GeoDMS/wiki/Parent-item) for all [attributes](https://github.com/ObjectVision/GeoDMS/wiki/Attribute). These attributes can be configured explicitly or are read with a default [value type](https://github.com/ObjectVision/GeoDMS/wiki/Value-type).

#### options

The following example shows how to configure an open option to allow for in memory filesize of 500MB instead of the default 100MB, following the OSM driver.

<pre>
unit&lt;uint32&gt; optionSet := range(uint32, 0, 1);
attribute&lt;string&gt; GDAL_Options (optionSet) : ['MAX_TMPFILE_SIZE=500'];
</pre>

