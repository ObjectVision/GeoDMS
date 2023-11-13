With the ASCII grid storage manager the GeoDMS can read data from and write data to the [ASCII grid](https://en.wikipedia.org/wiki/Esri_grid#ASCII) format. Especially for larger grids it is not an advised format as reading and writing ASCII data is slow (we advice to use the [[GeoTiff]] format).

## configuration

The number of rows and columns, the [[projection]] information and the no data value of an ASCII grid can be read from the header of the file.

Example, projection information derived from header:

<pre>
unit&lt;uint32&gt; GridDomain: StorageName = "%projdir%/data/testgrid.asc"
{
   attribute&lt;uint32&gt; GridData;
}
</pre>

The subitem GridData is the attribute with the data from the ASCII grid file.

The [[grid domain]] can also be explicitly configured. Configuration with an explicitly configured GridDomain, the configuration of the ASCII grid file would be:

<pre>attribute&lt;uint32&gt; GridData (GridDomain): StorageName = "%projdir%/data/testgrid.asc";</pre>
