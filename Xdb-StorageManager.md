<pre>
Note !!! : The Xdb StorageManager is not supported at the moment, 
as .txt files in Unix format are not correctly interpreted.
</pre>

## non-delimited fixed-length files

Non-delimited fixed-length ASCII files can be read with the **XDB** [[StorageManager]].

This StorageManager reads data from two different files, with the same file name but with different extensions:

- An .xdb file: a meta file describing the contents of the primary data .txt file.
- The .txt file: a file with the actual values (primary data).

Example:

<pre>
unit&lt;uint32&gt; Table: StorageName = "%SourceDataDir%/ASCII/XDB.xdb"
{
   attribute&lt;int32&gt;   IntegerAtt;
   attribute&lt;float32&gt; FloatAtt;
}
</pre>

Only a reference need to be configured to the .xdb file. The accompanying .txt file needs to be located in the same folder.

Writing data to non-delimited fixed length ASCII files with the XDB storage manager is not supported.

Use the [[gdal_vect StorageManager|gdal.vect]] to read data from and write data to delimited ASCII files. 

Example: XDB.xdb:

<pre>
5 0
IntegerAtt 5 1
FloatAtt 7 9
</pre>

The first row always contains two values separated by a space:
- The first value determines the number of rows with primary data in the .txt file and is used to determine the NrOfRows of the [[domain unit]] used for these data values. In the example, this number is **5**.
- The second value indicates the number of header lines in the .txt file. In the example, no header lines occur so this number is **0**.

In the following lines all occurring [[attributes|attribute]] need to be described, each with the format: name, offset and type, separated by spaces:

- The name is the same name as used for the attribute in the GeoDMS configuration. In the example the general names IntegerAtt and FloatAtt are used.
- The offset indicates the number of positions that are used for the attribute. The start position in each line is always the first position. In the example 5 positions are used for the IntegerAtt and after that 7 positions for the FloatAtt.
- The type is used to determine the [[value type]] of the resulting attribute. The following codes can be configured:
    - 0: uint32
    - 1: int32
    - 8: float64
    - 9: float32

In the example the codes 1 (int32) are used for the IntegerAtt and 9 (Float32) for the FloatAtt.

## the .txt file

The .txt file accompanying the .xdb file looks as follows: XDB.txt:

<pre>
   0        0
   1        1
 256  9999999
-100       -2.5
9999       99.9
</pre>