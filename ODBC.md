## database: ODBC

The GeoDMS can read data from a configured ODBC source in two ways:

-   A direct reference to a file, if the source is file based (e.g. [[MsAccess]], [[MsExcel]]).
-   By using a configured [DSN](https://en.wikipedia.org/wiki/Data_source_name) at the local machine.

For file based databases the first option is preferable, as it makes it easier to exchange projects between different machines.

It is not possible to write data directly to an ODBC source. An indirect way is to write data to a [[dbf]] or [[ASCII format|ASCII Files]], formats that can usually be imported in most databases.

## hierarchical structure

The hierarchical structure for the configuration of an ODBC source is presented in the next picture:

[[images/Mr_odbc.gif]]

Three levels can be distinguished in the configuration of an ODBC source:

-   Database level: the database is configured to a [[container]], e.g. with the name SourceData.
-   Table, View/Query level: the Tables, View/Queries from which the data is read are configured as [[subitems|subitem]] of the database level.
-   Attribute level: the relevant [[attributes|attribute]] from the tables/queries are configured as subitems of the configured tables/queries.

The [[SqlString]] [[property]] is used to configure which attributes are selected from which Table, View/Query. Always configure the ORDER BY clause in the SqlString property as the sequence of elements matters in the GeoDMS and and ODBC source only guarantees the same sequence of elements if the ORDER BY clause is configured.

The ODBC StorageManager is used in GeoDMS projects to read data from [[MsAccess]] databases and [[MsExcel]] files. Other ODBC sources can also be configured.

With the ODBC text driver data can also be read from ASCII files, but we advise other StorageManagers for this format.

## Value Types

The [[values types|value type]] of the [[values units|values unit]] in the GeoDMS should match with regard to their type (not their size) with the data
types of the [[attributes|attribute]] in the database. The following values types are allowed for the ODBC data types:
-   (u)int8/16/32/64 for byte, integer and long Integer;
-   float32/64 for single/double;
-   bool for boolean;
-   string for string;
-   Other data types (date, currency etc.) can not (yet) be read from ODBC sources with the GeoDMS.

## ODBC and x64 versions

To use a GeoDMS 64 bits platform version for projects that read data from ODBC sources, 64 bits ODBC drivers are required on your local machine. Installing these drivers is not possible if a 32 bits Ms-Office version is installed.

Recommendations for using the GeoDMS on x64 platforms with 32 bits MS-Office installed:

1.  Plan migration of the use of MsExcel and/or MsAccess files to other file formats, such as [[CSV]]. Consider how you would edit the data. For projects that depend on reading data from MsExcel and/or MsAccess, this can be laborious, since all data in MsExcel and MsAccess need to be exported.
2.  For small chunks of data read by ODBC, consider re-implementing the data directly within configuration files, such as the land use tables.
3.  Use a (virtual) machine without Win 32 MS-Office (consider x64 version of MS-Office and/or Open/Libre Office).
4.  For projects that run fine on a Win32 GeoDMS version and that depend on MsExcel and/or MsAccess: keep using the Win32 version.