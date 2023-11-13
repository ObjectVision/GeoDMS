Before Installing, make sure your system meets the [[System Requirements]].

Installing a GeoDMS application is performed in two steps:

1.  Download and install the [[software|GeoDMS Setups]].
2.  Obtain and organise the project specific (original) configuration, data and documentation.

## Download and install the GeoDMS software

To install the GeoDMS (GUI and dll's) see the [[releases page|https://github.com/ObjectVision/GeoDMS/releases]].

*Win32 or X64*
GeoDMS versions are available for 32 bits and 64 bits platforms. For Windows 32 bits operating systems, always install a GeoDMS version for a Win32 platform. For Windows 64 bits operating systems, both Win32 and X64 bits versions can be installed. In general it is advised to use a GeoDMS version for the 64 bits platform on a 64 bits operating systems, as it can address much larger data items.

There is one exception to this rule, if a project uses [ODBC](https://nl.wikipedia.org/wiki/Open_DataBase_Connectivity) sources
(to read for instance .mdb or .xls files) and on the local machine a 32 bits version of MsOffice is installed..

After downloading, activate the setup program and follow the instructions. The software will be installed by default in a %ProgramFies%/ObjectVision/GeoDMS\<*versionnr*\> directory.

After installing the software on some 64 bits platforms, the Program Compatibility Assistant might pop up. In these cases choose the option:
This program installed correctly. If you notice problems with the installation afterwards, please inform us.

## Obtain and organise the project specific (original) configuration, data and documentation

The project specific files can be obtained from an archive file, e.g. zip or rar. Extract all files from the archive file, including the directory structure, in a free to choose base directory for your project. This directory is called the project directory. The source for the project specific files can also be our or any (git or svn) server. In that case an git or svn client such as [Tortoise](http://tortoisesvn.net/downloads.html) can be used to
download a specific version of these project files.

After extracting and or downloading, the project directory consists of subdirectories with the configuration, the data and the documentation files. Temporary calculation results and export are stored in subdirectoires of the localDataProjDir. For more information on the these directories, see
[[Folders and Placeholders]].

## Performance Configuration

Telling MS Windows what to do and not to do with SourceData and LocalData can make a significant difference in performance.

<details><summary>read more</summary>

MsMpEng.exe and other virus protection programs are known to scan intermediate data files stored in LocalData and therewith spending more CPU time than the GeoDMS that produced them and sometimes even cause data-missing errors by quarantaining data files during a calculation process.

SouceData, such as _C:\SourceData_

-   Activate Disk Compression by View Folder Properties->Advanced Attributes and check "Compress contents to save disk space".
-   Disable Indexing service by View Folder Properties->Advanced Attributes and clear "Allow files in this folder to have contents indexed ... "

LocalData folders, such as _C:\LocalData_

-   Exclude from Indexing service(s): View Folder Properties->Advanced Attributes and clear "Allow files in this folder to have contents indexed ... "

<!-- -->

-   Exclude from virus scanners, such as in Settings-> Update & Security -> Virus and threat protection->Virus and threat protection settings->Exclusions->Add C:\\LocalData.

To avoid memory allocation errors during calculations, set the initial size of the Pagefiles to a substantial amount, such as 2x available RAM.
- Windows 10:
  -   Windows Settings->System->About (in left menu)->Advanced system settings (in right menu)->Tab Advanced -> click on \[Settings...\]
    under "Performance" -> Tab Advanced -> click on \[Change...\] under "Virtual memory", select any fast drive (preferably a SSD) and
    select Custom size with a large Initial size

Furthermore,

-   Go to GeoDmsGui -> Main Menu ->Tools->Options->Advanced and set "Minimum size for ... swapfiles in CalcCache" to 2000000000 bytes (2\*10^9) and Threshold for Memory Flushing wait to 95%.

</details>

## Installation Issues

### Microsoft Defender SmartScreen Protection

Since GeoDMS version 7.315, valid setup execurtables are EV code-signed with Verified Publisher: Object Vision B.V.

When executing setups for GeoDms versions prior to 7.315, Windows may prompt you with the following warning:

[[images/WindowsProtect1.png]]

First, check that the setup was downloaded directly from the https://github.com/ObjectVision/GeoDMS domain which provides a htpps protection that downloads originate from Object Vision's server.

Then, click the "More info" link. For versions prior to 7.314, you should see:

[[images/WindowsProtect2.png]]

For 7.314, you should see: Verified Publisher: Open Source Developer, Martinus Hilferink

Click the "Run anyway" button for installing the software.

In case you are still unable to install GeoDMS, this might be related to anti-virus and/or firewall related software, either add an exception yourself, or contact your system administrator.

### missing dlls

After installing and starting the GeoDMS, errors relating to missing dlls may occur, see: [[Known Issues]].