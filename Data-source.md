A typical workflow for a GeoDMS project is to
- [[read data]] from a (set of) data source(s) (files/databases)
- calculate results in memory, using [arrays](https://en.wikipedia.org/wiki/Array_data_structure)
- view results and or [[write data]] to files/databases.

To read data from and to write data to files/databases, so-called [[StorageManagers|StorageManager]] are used.

We advise to configure source [[data items|data item]] in a source data container, these data items can be referred to from any other location in the configuration.

Data can also be explicitly exported with the [[GeoDMS GUI]] with the File > Export Primary Data menu options.

## vector, grid and non spatial data

For spatial data a distinction is made in:

-   [[Vector data]]: [[point]], [[arc]] or [[polygon]] geometry data for [[one-dimensional domains|one dimensional domain]]
-   [[Grid data]]: [[attributes|attribute]] for [[two-dimensional domains|two dimensional domain]]

In [[GDAL]] this same distinction is used, [[gdal.vect]] or [[gdalwrite.vect]] is used for vector data, [[gdal.grid]] or [[gdalwrite.grid]] for grid data.

Non spatial data can be also partly be read from or written to with gdal.vect or gdalwrite.vect but also by some other StorageManagers.