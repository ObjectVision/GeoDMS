To run the [[GeoDMSRun]], activate the Command Prompt Window. The GeoDMSRun.exe can then be activated from the [command line](https://en.wikipedia.org/wiki/Command-line_interface). The following parameters can/need to be specified:

-   (Optional) */Llogfilename* (since version 5.55)
-   (Optional) any combination of /C1 /S1 /C2 /S2 /C3 /S3 to Clear or Set Multiple Threading level 1, 2, or 3. If these parameters are not     configured, the default settings (see the GUI, Tools> Options> Advanced dialog) are used. S3 will be used for tile pipelining in the near future, at the moment this setting this parameter has no effect.
-   (Required) *ConfigFileName*
-   zero or more [[tree items|tree item]] names (space separated) to be calculated. Multiple item names can be specified, calculated results will be saved in storage's were configured.

### examples

1: Update the result item in the *C:\prj\test\cfg\stam.dms* configuration

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" "C:\prj\test\cfg\stam.dms" /result          `
</pre>

2: Update the *Arithmetics/plus/test_attr* and *Arithmetics/sub/test_attr* items in the *C:\prj\test\cfg\operator.dms* configuration

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" "C:\prj\test\cfg\operator.dms" 
  /Arithmetics/plus/test_attr /Arithmetics/sub/test_attr       
</pre>

3: Update the results item in the *C:\prj\test\cfg\stam.dms* configuration with log information stored in the *C:\tmp\log.txt* file.

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" "/LC:\tmp\log.txt" "C:\prj\test\cfg\stam.dms" /result
</pre>


4: Update the *result* item in the *C:\prj\test\cfg\stam.dms* configuration with Multi Tasking [[StatusFlags]] 1 and 2 set (even if
these are not the default settings).

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" /S1 /S2 "C:\prj\test\cfg\stam.dms" /result
</pre>

Quotes in the examples are needed for the file names, as spaces may occur in these names. In the [[tree item]] names
quotes are not needed, as spaces are not allowed in these names. Note that: The value for [[LocalDataDir]] is read
from the registry key Software\ObjectVision\DMS\LocalDataDir (default: C:\LocalData) which can be changed in the Tools -> Options
dialog from the [[GeoDMS GUI]]. 

The GeoDMS GUI settings about logging as saved in the registry is ignored for the GeoDMSRun, use the /L option to enable logging.

Commands can also be combined in a [batch file](https://en.wikipedia.org/wiki/Batch_file).

### statistics

With the GeoDMS run it is also possible to get similar statistics as the Statistics detail page of a [[data item]], presented in a command box or written to a file.

-   use the command **@statistics** after the name of the configuration and before the item name to get the statistics in the command box
-   use the command **@file** after the item name with a **filename** to store the statistics in a file

see the following two examples:

1: Generate the statistics for the results/att item in the *C:\prj\test\cfg\stam.dms* configuration in a command
box

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" "C:\prj\test\cfg\stam.dms"  @statistics /results/att
</pre>

2: Generate the statistics for the results/att item in the *C:\prj\test\cfg\stam.dms* configuration and write the results to the file: D:\log\stat_att.txt

<pre>
"C:\Program Files\ObjectVision\GeoDms7317\GeoDmsRun.exe" "C:\svn\GeoDMS\dev\webgen\cfg\dms.dms" <BR> @statistics /results/att @file D:\log\stat_att.txt
</pre>