The StorageName [[property]] needs to refer to the name of a primary data source (a file or database).

It can be configured to a [[domain unit]], a [[data item]] or [[container]], based on the [[StorageManager]].

Use placeholders in path names, like %SourceDataDir%, to make the configurations easily transferable to other locations/machines, see
[[Folders and Placeholders]].

For some StorageManagers, the file extension indicates which StorageManager will be used (.dbf, .shp, .tif). This can be overruled with the
[[StorageType]] property.