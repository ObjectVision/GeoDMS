The values of a string data item can be read from and written to a (set of) ASCII file(s) with the str/strfiles [[StorageManager]]

-   The str StorageManager is used to read parameters values from and write parameters values to a single ASCII file.
-   The strfiles StorageManager is used to read attributes from and write attributes to a set of ASCII files.

## str

Example:
<pre>
parameter&lt;string&gt; StringParam
:  StorageName = "%projDir%/data/file01.txt"
,  StorageType = "str";
</pre>

In this example the data for the parameter StringParam is read from the file file01.txt. Both [[StorageName]] and [[StorageType]] properties need to be configured for this parameter:

-   The StorageName property needs to refer to the filename from which the data is read and or written to.
-   The StorageType property needs to be configured to str.

This StorageManager is also used in the [[TableChopper|TableChopper (read ascii file)]] / [[TableComposer|TableComposer (write ascii file)]] to read/write csv files and in combination with the [[parse_xml]] function to read xml files.

## strfiles

Example:

<pre>
unit&lt;uint32&gt; FileSet: nrofrows = 5
{
   attribut&lt;string&gt; StringAtt
   :  StorageName = "%projDir%/data/StrFiles"
   ,  StorageType = "strfiles";
   attribute&lt;string&gt; FileName:
      ['file01.kml','file02.kml','file03.kml','file04.kml','file05.kml'];
}
</pre>

In this example the data for the five elements of [[attribute|attribute]] StringAtt is read from the text files: file01.kml .. file05.kml. Both
StorageName and StorageType properties need to be configured for the attribute:

-   The StorageName property needs to refer to the foldername in which the files occur, from which the data is read (and or written to).
-   The StorageType property needs to be configured to strfiles.
-   A string attribute with the name FileName needs to be configured in the same container. This attribute needs to contain the names of the files from which the data is read and/or written to. In this example the file names are read from the configuration file with the [..] syntax.