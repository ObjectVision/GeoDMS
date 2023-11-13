Since version 7.400 GeoDMS uses GDAL 3.

We are aware of the following issues related to the following formats:

## [[gdal.vect]]

- When writing a subitem structure, append subitem name to avoid collisions of identical item names.

## [[dbf]]

- [found in 7.408; fixed in 7.411] Boolean data items seem to be written as numeric data items. Reading the written data results in the error: *column: BoolAtt, configured type: Bool, database type: Int32*
- [found in 7.408; fixed in 7.411] Attribute field names with more than 10 characters result in unclear "invalid index" error
- [found in 7.408; fixed in 7.409] Attributes with [[value type]] uint2/uint4 are not supported, errors:
    * gdal.vect Error: Cannot read attribute data of type 1 into attribute of type 12/13*
- [found in 7.408; fixed in 7.409] written numeric null values are reread as 0 values.
-  [behavior accepted, to be documentend] When writing data to a file that is locked (e.g. opened in MsExcel), no error dialog is presented. The error in the Detail Page is *gdal Error: error(1): D:/LocalData/storage/regr_results/gdalwrite_vect/gdal.dbf is not a directory,* which should be something like*: data can not be written as resulting file is locked.*

## [[ESRI Shapefile]]

- How to deal with multiple geometries as subitems of a unit for which a [[StorageManager]] is configured? What is the geometry to be written to the .shp file and how to treat the other geometries in the .dbf file?
- [found in 7.408; fixed in 7.411] Index files are made if the attribute<string> GDAL_LayerCreationOptions(optionSet) : ["SPATIAL_INDEX=YES"\] is configured. This works fine if a geometry item is updated, it does not work if the parent of the geometry item is requested in map view.

## [[csv]]

- [found in 7.408; fixed in 7.409] reading float32 from a .csv file with a .csvt file, results in incorrect integer conversion, such as -2.5 -> -2.0
- [[null]] values and empty strings are represented the same in a .csv result and become indistinguishable
- [found in 7.410] all fields read as string, even when user explicitly configures datatype.

## GeoJSON

- [found in 7.410; fixed in 7.411] writing multiple attributes results in error: *Committing Data (writing to storage) Failed, FailReason  gdal Error: error(1): Invalid index : -1*

## Tiff

- reading tiff files with uint32 data results in values between 0 and 255, even if in tiff file higher files occur
- reading tiff files with boolean data results in error: *GridData::ReadData Error: Cannot convert 1 bits DMS data from/to 8 bits Raster file data*
- reading tiff files with palette data does not read the palette data
- writing GeoTiff files results in error: *Committing Data (writing to storage) Failed, FailReason Check Failed Error: height == GetWidth(),  C:\\dev\\geodms7400\\stg\\dll\\src\\gdal\\gdal_grid.cpp(219):*

## bmp

- reading some bmp files results in error: *file.bmp not recognized as a supported file format*.
- writing bmp files with bmpw results in error: *Committing Data (writing to storage) Failed, FailReason Check Failed Error: height == GetWidth(),     C:\\dev\\geodms7400\\stg\\dll\\src\\gdal\\gdal_grid.cpp(219):*

## png

- reading png files with palette data does not read the palette data