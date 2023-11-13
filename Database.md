Spatial [[vector data]] and non spatial data can be read from different databases with the ODBC and [[gdal.vect]] [[StorageManager]].

The [[SqlString]] property can be used to select the relevant data in a relevant sequence from one or multiple tables/views/queries.

in GeoDMS projects data is read from the following databases:

-   [[Geopackage]]
-   [[MsAccess]] / [[MsExcel]] ([[ODBC]])
-   [[PostgreSQL PostGIS|PostGis]] 
-   [[FileGeoDatabase]]

## issues with databases in GeoDMS applications

1. If databases are not file-oriented, the timestamp mechanism of cannot determine if primary data is changed since the last results are calculated. 

2. Exchanging data with non file-oriented database (e.g. PostgreSQL) between partners in projects is often less easy as with files.

3. The GeoDMS offers functions that are usually also available in databases (selections, aggregations, joins).

Databases are often also in use for editing data, often using a transaction mechanism. As the GeoDMS is intended for attribute/array oriented calculations and not for editing, it does not use such a mechanism and can perform these calculations often faster.