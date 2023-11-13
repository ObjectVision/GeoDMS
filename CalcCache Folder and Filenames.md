*[CalcCache](CalcCache "wikilink") Folder and Filenames*

## introduction

Each version, configuration and environment (32 or 64 bits) uses it's
own [CalcCache](CalcCache "wikilink").

[CalcCache](CalcCache "wikilink") results are stored in a folder
structure, based on the hierarchy of a configuration.
[Containers](container "wikilink") in the configuration become
subfolders, cached calculations results become files, with names derived
from their related [tree item](tree_item "wikilink") names. The
following picture shows the root folder of the 64 bits CalcCache of a
[Vesta](Vesta "wikilink") configuration.

[<File:CCfolder.png>](File:CCfolder.png "wikilink")

*example of the CalcCache folder in the Windows Explorer*

## naming

The root folder contains the file: *CacheInfo.dmsdata*. This file
contains meta information on the stored files, including time stamps
used to find out if these files are still valid. Small [data
itemss](data_item "wikilink") and the
[cardinality](cardinality "wikilink") of some [units](unit "wikilink")
are also stored in this file.

The names of the subfolders in the root folder are based on the parent
names of the [tree item](tree_item "wikilink") for which the
calculation results are stored. Each cached file in a subfolder has a
name according to the following convention:

*[tree item](tree_item "wikilink") name_unique serial number.dmsdata.*

For tiled data items a subfolder is made with the name convention:
*name_unique serial number.* Within this subfolder each tile is stored
as a separate file, with the name convention:
*unique_tilenumber.dmsdata*.