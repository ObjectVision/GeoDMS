## folders

For a GeoDMS project folders (and subfolders) with the following content are relevant:

- installed [[Software]]
- project-specific configuration, data and documentation files
- project-specific (intermediate) results and exports.

## placeholders

In GeoDMS configurations placeholders with logical names like [[SourceDataDir]] and [[LocalDataDir]] are used for folders.

They make configurations clear and easily transferable to other locations/machines. It is advised to use these placeholders, although physical path names are also allowed.

All placeholders used in a storageName or as the second argument of the [[expand]] function, starting and ending with a % character will be expanded, e.g. [[StorageName]] = "%SourceDataDir%/brondata.csv".

The last column in the next table presents the expanded values for a project folder: _C:/prj/nl_later_.

<table>
<tbody>
<tr class="odd">
<td><strong><sup>Placeholder</sup></strong></td>
<td><strong><sup>Description</sup></strong></td>
<td><strong><sup>Default Value</sup></strong></td>
<td><strong><sup>Expanded Value (example)</sup></strong></td>
</tr>
<tr class="even">
<td><strong><sup>%currentDir%</strong></td>
<td><sup>The folder of the root configuration file that is opened by the GUI. For shipped configurations, this is usually the cfg subfolder of the projDir.</td>
<td><strong></strong></td>
<td><sup>C:/prj/nl_later\cfg</td>
</tr>
<tr class="odd">
<td><sup><strong>%exeDir%</strong></sup></td>
<td><sup>The folder in which the software is installed.</sup></td>
<td><sup>non-overridable</sup></td>
<td><sup>C:/Program Files/ObjectVision/GeoDMS<versionnr></sup></td>
</tr>
<tr class="even">
<td><sup><strong>%programFiles32%</strong></sup></td>
<td><sup>The folder that contains the (32-bit) programs, known as the value of %env:Program Files (x86)% or if that is empty (such as on 32-bit platforms): the value of %env:Program Files% (see %env:xxx% below).</sup></td>
<td><sup>non-overridable</sup></td>
<td><sup>c:/Program Files</sup></td>
</tr>
<tr class="odd">
<td><sup><strong>%projDir%</strong></sup></td>
<td><sup>The full name of the project-specific folder containing the (original) configuration, data and documentation folders. non-overridable</sup></td>
<td><sup>%currentDir%\..</sup></td>
<td><sup>C:/prj/nl_later</sup></td>
</tr>
<tr class="even">
<td><sup><strong>%projBase%</strong></sup></td>
<td><sup>Parent of the projDir. non-overridable</sup></td>
<td><sup>%projDir%\..</sup></td>
<td><sup>C:/prj</sup></td>
</tr>
<tr class="odd">
<td><sup><strong>%configDir%</strong></sup></td>
<td><sup>The directory in which the [[config.ini]] and a set of configuration files are located. By default this is a subfolder of the projDir with as name the project name.</sup></td>
<td><sup>%currentDir%\%projName%</sup></td>
<td><sup>C:/prj/nl_later/cfg/nl_later</sup></td>
</tr>
<tr class="even">
<td><sup><strong>%dataDir%</strong></sup></td>
<td><sup>The base folder containing the project-specific primary data files, often also organised in subfolders.</sup></td>
<td><sup>%sourceDataProjDir%\data</sup></td>
<td><sup>C:/prj/nl_later/data</sup></td>
</tr>
<tr class="odd">
<td><sup><strong>%[[LocalDataDir]]%</strong></sup></td>
<td><sup>The default base folder used for the project-specific localDataProjDirs. The local data folder is written to the Windows Registry (HKE_CURRENT_USER/Software/ObjectVision/DMS).</sup></td>
<td><sup>C:/LocalData</sup></td>
<td><sup>C:/LocalData</sup></td>
</tr>
<tr class="even">
<td><sup><strong>%LocalDataProjDir%</strong></sup></td>
<td><sup>The folder used for the project-specific (intermediate) results and exports. Views are exported to this folder unless otherwise configured in an ExportSettings container of a view definition.</sup></td>
<td><sup>%localDataDir%/%projName%</sup></td>
<td><sup>C:/LocalData/nl_later</sup></td>
</tr>
<tr class="odd">
<td><sup><strong>%[[SourceDataDir]]%</strong></sup></td>
<td></td>
<td><sup>c:/SourceData</sup></td>
<td></td>
</tr>
<tr class="even">
<td><sup><strong>%sourceDataProjDir%</strong></sup></td>
<td></td>
<td><sup>%sourceDataDir%/%projName%</sup></td>
<td></td>
</tr>
<tr class="odd">
<td><sup><strong>%CalcCacheDir%</strong></sup></td>
<td><sup>The folder used for the for project-specific (intermediate) results.</sup></td>
<td><sup>%localDataProjDir%/ CalcCache</sup></td>
<td><sup>C:/LocalData/nl_later/<br />
CalcCache</sup></td>
</tr>
<tr class="even">
<td><sup><strong>%DateTime%</strong></sup></td>
<td><sup>Resolves %DateTime% placeholder to current time in local system time using the format: YYYY-m-D_H-M-S (available since GeoDMS 8.044)</sup></td>
<td><sup>non-overridable</sup></td>
<td></td>
</tr>
</tbody>
</table>

When a configuration is opened, the GeoDMS considers the folder that contains the root configuration file as currentDir. The physical paths for the other logical folders in the configuration are derived from this path but can be overruled by entries in the ConfigSettings container. 

There are also other placeholders that refer to other parts of the execution environment and sometimes based on the context of usage:

| Placeholder           | Description   |
|-----------------------|---------------|
| **%projName%**        | folder name of %projName%|
| **%env:xxx%**         | is expanded to the value of the environment variable with the name xxx.|
| **%storageBaseName%** | is expanded to the base name of a storage (excluding the filename extension)|
| **%geoDmsVersion%**   ||
| **%osversion%**       ||
| **%username%**        ||
| **%computername%**    ||
| %XXX%                 | (for XXX not matching any of the above) if XXX is defined as parameter<string> in /ConfigSettings/Overridable, take the value of that parameter. If the user overrides that value in the GUI in Tools->Options->Config Settings, the override value is taken (which is stored as machine-specific registry setting under Software/ObjectVision/Overridable).|