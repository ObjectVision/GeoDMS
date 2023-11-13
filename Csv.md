A comma-separated values (csv) file stores tabular data (numbers and text) in plain text. 

It is a common data exchange format that is widely supported.

## Read

The GeoDMS supports two ways of reading csv files:

- gdal.vect: for most csv files, we advice to use gdal.vect to read csv files, see next subparagraph.
- [[TableChopper|TableChopper (read ascii file)]]: we advice to use the TableChopper to read csv files:
    - If your csv file contains other separators than comma or semicolon, or
    - If your csv is very large (for performance reasons)

### gdal.vect

The following example shows how to read a .csv file with the [[gdal.vect]] [[StorageManager]].

#### example
<pre>
unit&lt;uint32&gt; pc6
:  StorageName     = "%SourceDataDir%/CBS/pc6_data.csv"
,  <B>StorageType     = "gdal.vect"</B>
,  StorageReadOnly = "True"
{
}
</pre>

All [[attributes|attribute]] from the csv files are read. The name is default derived from the first (header) row, the default [[value type]] for all
attributes will be string. Use [[conversion functions]] to cast the data to requested values units.

In case a [.csvt](https://gdal.org/drivers/vector/csv.html) file is available, values that match the value types of the data van be directly configured. A .csvt can be written with [[GDAL|GDAL update]], see the write examples.

In earlier GeoDMS versions a _geometry_ attribute was added, usually containing null values. This attribute can be ignored.

Gdal.vect supports comma and semicolon separated csv files, for reading data no separator has to be configured.

#### options

[[GDAL Options]] can be configured for reading different csv files. 

See: <https://gdal.org/drivers/vector/csv.html#open-options> for a full list of all open options.

<details><summary>Examples on how to configure read options:</summary>

<pre>
container noheader
{
   unit&lt;int32&gt;       optionSet := range(uint32, 0, 1);
   attribute&lt;string&gt; GDAL_Options (optionSet) : <B>['HEADERS=NO']</B>;

   unit&lt;uint32&gt; pc6_ignore_header
   :  StorageName     = "%SourceDataDir%/CBS/pc6_data.csv"
   ,  StorageType     = "gdal.vect";
   ,  StorageReadOnly = "True";
}

container emptyvalues
{
   unit&lt;int32&gt;       optionSet := range(uint32, 0, 1);
   attribute&lt;string&gt; GDAL_Options (optionSet) : <B>['EMPTY_STRING_AS_NULL=YES']</B>;

   unit&lt;int32&gt; pc6_empty_string_as_null
   :  StorageName     = "%SourceDataDir%/CBS/pc6_data.csv"
   ,  StorageType     = "gdal.vect";
   ,  StorageReadOnly = "True";
}
</PRE>

The first example configures a source file in which the header is ignored. The resulting field names will then be: *field_1, field_2.. field_n*.

The second example configures how empty cells are treated. By default the become empty strings, by configuring the option: EMPTY_STRING_AS_NULL=YES, they become [[Null values|Null]]

Multiple GDAL_Options can be configured in your optionSet, use a comma as separator.

We advice to use different containers for configuring csv files with different open options.

</details>

## Write

The GeoDMS supports two ways of writing csv files:

- gdalwrite.vect: since GeoDMS version 7408, we advice to use the gdalwrite.vect to write most csv files, see next subparagraph.
- [[TableComposer|TableComposer (write ascii file)]]: we advice to use the TableComposer to write csv files.
    - If you need more flexibility in the contents of the header line(s) and/or body text, or
    - If you want another separator as semicolon or comma, or 
    - If your csv is very large (for performance reasons)

### gdalwrite.vect

The following example shows how to write a .csv file with the [[gdalwrite.vect]] StorageManager.

#### example

<pre>
unit &lt;uint32&gt; pc6_export := src/pc6
,  StorageName     = "%localDataProjDir%/export_semicolon.csv"
,  <B>StorageType     = "gdalwrite.vect"</B>
,  StorageReadOnly = "false"
{
   attribute&lt;uint32&gt;  IntegerAtt := const(1, .);
   attribute&lt;float32&gt; FloatAtt   := const(1f, .);
   attribute&lt;string&gt;  StringAtt  := const('A', .);
   attribute&lt;bool&gt;    BoolAtt    := const(true, .);
}
</pre>

Attributes of all value types, except for values types of the point group, are written to a csv file. This applies to both the direct as the indirect [[subitems|Subitem]].

The resulting file will contain one header line with the name of each attribute. By default a semicolon is used as seperator and all attributes will be double quoted.

#### options

[[GDAL Options]] can be configured for writing different csv files. 

See: <https://gdal.org/drivers/vector/csv.html#layer-creation-options> for a full list of all creation options.

<details><summary>Examples on how to configure write options:</summary>

<pre>
container comma
{
   unit&lt;uint32> optionSet := range(uint32, 0, 1);
   attribute&lt;string> GDAL_LayerCreationOptions (optionSet) : <B>["SEPARATOR=COMMA"]</B>;

   uni&lt;<uint32> pc6_export := src/pc6
   ,  StorageName     = "%localDataProjDir%/export_comma.csv"
   ,  StorageType     = "gdalwrite.vect"
   ,  StorageReadOnly = "false"
   {
      attribute&lt;uint32&gt;  IntegerAtt := const(1, .);
      attribute&lt;float32&gt; FloatAtt   := const(1f, .);
      attribute&lt;string&gt;  StringAtt  := const('A', .);
      attribute&lt;bool&gt;    BoolAtt    := const(true, .);
   }
}

container geometry_as_wkt
{
   unit&lt;uint32&gt; optionSet := range(uint32, 0, 3);
   attribute&lt;string> GDAL_LayerCreationOptions (optionSet) : 
      <B>["GEOMETRY=AS_WKT", "GEOMETRY_NAME=GEOMETRY", "CREATE_CSVT=YES"]</B>;

   unit&lt;uint32&gt; poly := EsriShape/Polygon
   ,  StorageName     = "%localDataProjDir%/poly.csv"
   ,  StorageType     = "gdalwrite.vect"`
   ,  StorageReadOnly = "false"`
   {
      attribute&lt;fpoint&gt; geometry (poly) := EsriShape/Polygon/Geometry;
      attribute&lt;string&gt; label           := EsriShape/Polygon/Label;
   }
}
</pre>

The first example show how to configure a csv file with a comma as separator.

The second example shows how you can also write a [[vector|Vector]] [[geometry]] to a csv file (point, line and polygon). The data will be written als
well-known textformat (WKT).

The second option in this examples configures the name in the csv file with the WKT. The third option indicates that a .csvt is also written 
with exported attribute names.

We advice to use different containers for configuring csv files with different creation options.
</details>