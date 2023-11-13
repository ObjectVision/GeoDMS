Dbf files are [dBase](https://nl.wikipedia.org/wiki/DBASE) files, used as part of the [[ESRI shapefile]] or as separate data file.

In reading and writing dbf files, the [[value types|value type]] of the [[values units|values unit]] of the [[attributes|attribute]] should match with the data types of the attributes in the dbf file.

The following value types can be used for dbf data:
- (u)int8/16/32/64 for byte, integer and long integer
- float32/64 for single/double
- bool for boolean
- string for string

Other data/value types can not be read from (e.g. date) or written to (e.g. all Point Group types) dbf storages.

## Read
The GeoDMS supports two ways of reading dbf files:

- gdal.vect: we advice to use the gdal.vect StorageManager to read dbf files as it supports [[segmented|tile]] data, see the next subparagraph.
- dbf StorageManager

### gdal.vect
The following example shows how to read a .dbf file with [[gdal.vect]].

#### example:
<pre>
unit&lt;uint32&gt; table: StorageName = "%projDir%/data/DBF.dbf"
,  <B>StorageType     = "gdal.vect"</B>
,  StorageReadOnly = "True"
{
  attribute&lt;int32&gt;   IntegerAtt;
  attribute&lt;float32&gt; FloatAtt;
  attribute&lt;bool&gt;    BoolAtt;
  attribute&lt;string&gt;  StringAtt;
}
</pre>

### dbf storagemanager
The following example shows how to read a .dbf file with the dbf storagemanager.

#### example:
<pre>
unit&lt;uint32&gt; table: StorageName = "%projDir%/data/DBF.dbf"
,  StorageReadOnly = "True"
{
   attribute&lt;int32&gt;   IntegerAtt;
   attribute&lt;float32&gt; FloatAtt;
   attribute&lt;bool&gt;    BoolAtt;
   attribute&lt;string&gt;  StringAtt;
}
</pre>

## Write
The GeoDMS supports two ways of writing dbf files:
- gdalwrite.vect: we advice to use the gdalwrite.vect StorageManager to write dbf files see the next subparagraph.
- dbf StorageManager

_Be aware: the names of attributes written to a .dbf file may not exceed 10 characters, as the .dbf file does not support longer fields._

We advise to write attributes to dbf file that do not have [[null]] values, as strange results might occur. Use the [[MakeDefined]] function to convert null values to a specific missing data value for your dbf file.  

### gdal.vect
The following example shows how to write a .dbf file with the [[gdalwrite.vect]].

#### example:
<pre>
unit &lt;uint32&gt; pc6_export := src/pc6
,  StorageName     = "%localDataProjDir%/export_table.dbf"
,  <B>StorageType     = "gdalwrite.vect"</B>
,  StorageReadOnly = "false"
{
   attribute&lt;uint32&gt;  IntegerAtt := const(1, .);
   attribute&lt;float32&gt; FloatAtt   := const(1f, .);
   attribute&lt;string&gt;  StringAtt  := const('A', .);
   attribute&lt;bool&gt;    BoolAtt    := const(true, .);
}
</pre>

### dbf storagemanager
The following example shows how to write a .dbf file with the dbf storagemanager.

#### example:
<pre>
unit &lt;uint32&gt; pc6_export := src/pc6
,  StorageName     = "%localDataProjDir%/export_table.dbf"
,  StorageReadOnly = "false"
{
   attribute&lt;uint32&gt;  IntegerAtt := const(1, .);
   attribute&lt;float32&gt; FloatAtt   := const(1f, .);
   attribute&lt;string&gt;  StringAtt  := const('A', .);
   attribute&lt;bool&gt;    BoolAtt    := const(true, .);
}
</pre>
