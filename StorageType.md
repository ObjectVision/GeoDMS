The StorageType property determines which [[StorageManager]] is used.

For some StorageManagers the default StorageType is derived from the extension of the filename, configured as [[StorageName]].

When the StorageManager cannot be derived from the StorageName or is different from its default, you need to configure it.

List of available types:
-   for raster data: "gdal.grid", "TIF", "BMP",
-   for feature layers, tables and layer sets: "gdal.vect", "Shp", "DBF"
-   for direct mapping of GeoDMS data: "fss", "cfs"
-   for value \<-> file storage: "str", "strfiles".