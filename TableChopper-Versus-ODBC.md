A third (except from [[gdal.vect]] and the [[TableChopper|TableChopper (read ascii file)]]) alternative for reading ASCII files is the
[ODBC](https://en.wikipedia.org/wiki/Open_Database_Connectivity) text driver, but this is not is advised for the following reasons:

- With ODBC the decimal separator is based on the regional settings of the system, which causes problems if the application is used on multiple systems with different regional settings.
- The ODBC text driver requires meta information stored in a schema.ini file in the same directory as the source data files. This is easily forgotten, resulting in unexpected errors.
- The ODBC text driver locks source files exclusively, which is undesirable as it is often useful to view the same data at least in a read-only mode in another application.
- Although the 64 bits versions of the GeoDMS can be used to read ODBC sources, this causes problems on systems with 32 bits versions of Ms Office installed. To use the 64 bits GeoDMS version, 64 bits ODBC drivers need to be installed, conflicting with the 32 bits MsOffice ODBC drivers.