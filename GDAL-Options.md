Each [[GDAL]] driver has a set of specific options to modify the behavior of the driver. Since version 7.408 these options can be configured in the GeoDMS.

Consult the [vector drivers](https://gdal.org/drivers/vector) and/or [raster drivers](https://gdal.org/drivers/raster/) specification of the driver to see what options are supported.

Options are configured as [[parameter]] (1 option) or [[attribute]] (1 or more options). If configured as attribute the [[domain unit]] is usually named: optionSet.   

An important distinction is made between open/read options for reading data with GDAL and creations options for writing data with GDAL.

## open/read options

Open/read options are configured with [[tree item name]]: _GDAL_Options_.  

The following examples show how to configure the option for reading a [[csv]] file without a header:

<pre>
1. parameter&lt;string&gt; <B>GDAL_Options</B>: [ 'HEADERS=NO'];

2. unit&lt;uint32&gt; optionSet := range(uint32, 0, 1);
   attribute&lt;string&gt; <B>GDAL_Options</B> (optionSet) : [ 'HEADERS=NO'];
</pre>

## create/write options

Create/write options are configured with [[tree item name]]: _GDAL_LayerCreationOptions_.  

The following examples show how to configure the option for writing a [[csv]] file with a comma as separator:

<pre>
1. parameter&lt;string&gt; <B>GDAL_LayerCreationOptions</B>: [ 'SEPARATOR=COMMA'];

2. unit&lt;uint32&gt; optionSet := range(uint32, 0, 1);
   attribute&lt;string&gt; <B>GDAL_LayerCreationOptions</B> (optionSet) : [ 'SEPARATOR=COMMA'];
</pre>

## configuration options
GDAL configuration options are configured with [[tree item name]]: _GDAL_ConfigurationOptions_.
For instance setting SQLite journal file to 'OFF':
<pre>
1. parameter&lt;string&gt; <B>GDAL_ConfigurationOptions</B>: ['OGR_SQLITE_JOURNAL=OFF'];
</pre>

## combining options
Options can be combined, for multiple options always use an [[attribute]]. The following example shows the configuration of reading a geoTiff with LZW compression, predictor level 2 and multithreading:

<pre>
unit&lt;uint32&gt; optionSet := range(uint32, 0, 3);
attribute&lt;string&gt; GDAL_Options (optionSet): ['NUM_THREADS=ALL_CPUS', 'COMPRESS=LZW', 'PREDICTOR=2'];
</pre>