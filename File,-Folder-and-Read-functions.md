File, Folder and Read [[functions|Operators and functions]] operate on files and folders, like [[MakeDir]] or [[storage_name]] or read data in specific formats, like [[ReadLines]]

-   [[ExistingFile]] - *full path name of the filename argument if the file exists and in the alternative argument if the file does not exists*
-   [[CreateFile]] - *copies the source filename to the target filename*
-   [[InitFile]] - *copies the init filename to the target filename, synonym for CreateFile*
-   [[CopyFile]] - *copies the source filename to the target filename*

<!-- -->

-   [[CurrentDir]] - *the folder of the root file of the loaded configuration*
-   [[MakeDir]] - *creates a new folder*
-   [[fullPathName]] - *full path name of the folder_or_filename argument*

<!-- -->

-   [[exec]] - *executes a command argument*
-   [[exec_ec]] - *executes a command with a returning exitcode*
-   [[execDll]] - *executes a function in the dll*

<!-- -->

-   [[storage_name]] - *value of the StorageName property*

<!-- -->

-   [[ReadValue]] - *a single parameter value from a string dataitem*
-   [[ReadArray]] - *an attribute, with row(s) of values from a string dataitem*
-   [[ReadLines]] - *an attribute with the concatenation of the values of the rows*
-   [[ReadElems]] - *an attribute, with column(s) of values from a string dataitem*

<!-- -->

-   [[parse_xml]] - *parses the contents of the argument with XML data into a set of configured attributes, based on the xml_scheme argument.*