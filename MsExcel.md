## MsExcel Range

Example:
<pre>
unit&lt;uint32> Range
:   StorageName = "DRIVER={Microsoft Excel Driver (*.xls)};DBQ=%projDir%/data/Sheet.xls"
,   StorageType = "ODBC"
,   SqlString   = "SELECT * FROM ImportSection ORDER BY ID"
,   SyncMode    = "All"
{
      attribute&lt;int32&gt;   IntegerAtt;
      attribute&lt;float32&gt; FloatAtt;
      attribute&lt;bool&gt;    BoolAtt;
      attribute&lt;string&gt;  StringAtt;
}
</pre>

Data can be read from a named range of a Sheet in an Ms Excel file. See the MsExcel help for how to name a range. The name of the range is used in the FROM statement in the [[SqlString]] property (ImportSection in the example). For MsExcel sheets, the [[StorageName]] is directly configured to the [[domain unit]] for the imported range.

The configuration of the DRIVER in the StorageName property and [[StorageType]] = "ODBC" is needed for the GeoDMS to recognise the file as an MsExcel ODBC source.

The configured [[attribute]] read the columns from the named range.