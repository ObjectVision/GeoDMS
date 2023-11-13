ASCII files are often used as (exchange) formats for tabular data. The GeoDMS uses the gdal.vect [[StorageManager]] to read and some
GeoDMS StorageManagers to read and write ASCII files.

ASCII files are interpreted as [UTF8](https://nl.wikipedia.org/wiki/UTF-8) encoded, functions to translate UTF8 to other code tables are being developed.

The following types of ASCII files are supported:

1.  [[csv]]: (comma-separated values file)
2.  [[xml]]: these files can be read with the [[str storageManager]] and converted to meaningful arrays with the [[parse_xml]] function.