*[Tools/Downloads](Tools_and_Downloads "wikilink") BRK Extract*

## introduction

The BRK extract is used to convert the downloaded .gml files from
[PDOK](http://www.pdok.nl) to for [GeoDMS](GeoDMS "wikilink") useable
.fss files.

## components

The BRK extract consists of simply one tool:

-   Gml2FSS -> To make the BRK available in a format suitable for
    modelling.

## requirements

-   [GeoDMS](GeoDMS "wikilink") version 7.130 or later, see
    [Setup](http://wiki.objectvision.nl/index.php/GeoDms_Setups)
-   Internal ram: 16 Gb or more (as the BRK is a very large dataset)
-   Hard disk space: 70 Gbyte or more
-   Access rights to your LocalDataDir (C:\\LocalData) and SourceDataDir
    (default C:\\SourceData). If not, adjust these
    [placeholders](Folders_and_Placeholders "wikilink") within the
    [GeoDMS GUI](GeoDMS_GUI "wikilink") > Tools > Options.

## how to use this script

1\. Download the BRK file from [PDOK](http://www.pdok.nl)

2\. Extract the .gml files from the .zip file to a local folder, with
naming convention: %SourceDataDir%/BRK/date:

-   The %SourceDataDir% is a
    [placeholder](Folders_and_Placeholders "wikilink") for the
    SourceData folder on your local disk. By default the path:
    C:/SourceData is used. You can choose another path for your
    SourceData, but then you have to configure this new path in the
    GeoDMSGUI > Tools > Options > SourceDataDir control.
-   Use for the date [placeholder](Folders_and_Placeholders "wikilink")
    the date of your download with as format: yyyymmdd.

3.Open the Gml2FSS.dms file from the script (in the cfg subfolder of
your chosen project folder) in a text editor, configure the date of your
download (see 2) in line 68 for the parameter date (yyyymmdd) and save
your file. Now open Gml2FSS.dms using GeoDMS and navigate to
*/Brondata/BRK/import/perceel_fss/src/AKRKADASTRALEGEMEENTECODE* and
double click on it (this might take a while to calculate!). This is
necessary to generate the perceel.gfs file, this file contains the
geometry of the dataset.

4.Open the newly-generated perceel.gfs file in a text editor. You'll
find this file in %SourceDataDir%/BRK/date and edit the following lines.

at the top:

` `<GeometryType>` 1 `</GeometryType>` `

into:

    <!--   <GeometryType> 1 </GeometryType> -->

and change nearly at the bottom:

`     `</PropertyDefn>
`  `</GMLFeatureClass>` `
</GMLFeatureClassList>

into:

</PropertyDefn>
`      `<GeomPropertyDefn>
`         `<Name>`begrenzing `</Name>
`         `<ElementPath>`begrenzingPerceel `</ElementPath>
`         `<Type>`Polygon `</Type>
`      `</GeomPropertyDefn>
`   `</GMLFeatureClass>` `
</GMLFeatureClassList>

6\. Make sure that in the %SourceDataDir%/CBS/2018/ the file
gem_2018.shp is present. This file contains the municipal borders and a
.dbf file with a field called GM_CODE. This file can be downloaded
[here](https://www.cbs.nl/nl-nl/dossier/nederland-regionaal/geografische-data/wijk-en-buurtkaart-2018).
7. Now open Gml2FSS.dms using [GeoDMS](GeoDMS "wikilink") again and
double click on *MaakPerceelFSSBestand*, this will generate the perceel
data in a fss storage including CBS municipal codes. Located in:
SourceDataDir/BRK/date/perceel_new.fss. This process will take quite
some time. 8.Using the *read_target* item you can view the results, if
all went correct, rename perceel_new.fss into perceel.fss.

## licensing

The BRK extract is available under [GNU-GPL version 3
license](http://www.gnu.org/licenses/gpl-3.0.html)

## download

The BRK extract scripts can be downloaded
[here](https://www.geodms.nl/downloads/BRK/BRK_Extract_2018oct.zip).
Extract the files from the zip file in your GeoDMS project
[folder](Folders_and_Placeholders "wikilink").