The GeoDMS stores settings, in use for multiple projects, in the Window Registry. In earlier versions of the GeoDMS these settings were also partly stored in a [[config.ini]] file.

The settings are in use for all projects and installed GeoDMS versions on your local machine.

## registry path

The path for most of the settings in the registry is: 
<pre>
Computer\HKEY_CURRENT_USER\Software\ObjectVision\%ComputerName%\GeoDMS.
</pre>

This means the settings are user and machine specific. Only the set of recent files is located in:
<pre>
Computer\HKEY_CURRENT_USER\Software\ObjectVision\DMS\RecentFiles 
</pre>

In earlier versions settings were also stored in Computer\\HKEY_CURRENT_USER\\Software\\ObjectVision\\DMS. You might find some settings here, but new GeoDMS versions do not use these anymore.

## settings

-   **DmsEditor**: path to the [[Configuration File Editor]]. This path can be set/edited with the [[GUI|GeoDMS GUI]], Tools > Options > General Settings > External programs > DMS editor.
-   **ErrBoxHeight**: the height of the last size of the error box. The size of the Error dialog can be changed from the control in the the right corner, this size is stored.
-   **ErrBoxWidth**: the width of the last size of the error box. The size of the Error dialog can be changed from the control in the the right corner, this size is stored.
-   **FuncList**: an url referring to where the list of operators and functions can be found (default: <https://www.geodms.nl/operators-a-functions>). The url is activated with the [[GUI|GeoDMS GUI]], Help > Index menu.
-   **GeneralUrl**: an url referring to root file of the GeoDMS documentation (default: <https://www.geodms.nl>), currently not in use.
-   **HelpFileUrl**: an url referring to the root file for the help documentation (default: <https://www.geodms.nl>). The url is activated with the [[GUI|GeoDMS GUI]], Help > Online menu.
-   **LastConfigFile**: the path to the last opened configuration, a new  GeoDMS applicatiion starts with this last opened configuration. Open another configuration with the [[GUI|GeoDMS GUI]], File > Open configuration dialog to open a new configuration.
-   **LocalDataDir**: the path to the [[LocalDataDir]], where temporary results are stored. This path can be set/edited with the [[GUI|GeoDMS GUI]], Tools > Options > General Settings > LocalDataDir.
-   **MemoryFlushThreshold**: a percentage used to indicate the size of the internal ram memory that might be used before data is flushed to disk.The percentage can be set/edited with the [[GUI|GeoDMS GUI]], Tools > Options > Current configuration > Treshold for Memory Flushing wait procedure. 
      It is advised to keep this setting to it's default value.
-   **SourceDataDir**: the path to the [[SourceDataDir]], where source data for one or more projects are stored. This path can be set/edited with  the [[GUI|GeoDMS GUI]], Tools > Options > General Settings > SourceDataDir.
-   **SatusFlags**: a value indicating multiple status flags (e.g. if parallel processing is in use). These settings can be set/edited with the [[GUI|GeoDMS GUI]], Tools > Options > General Settings > User modes.
-   **SwapfileMinSize**: before GeoDMS 8, data items were stored in the CalcCache if they apply to certain  conditions. One of the conditions was the minimum data size, which can be set/edited with the [[GUI|GeoDMS GUI]], Tools > Options > Current configuration > Minimum size for DataItem specific swapfiles in CalcCache.
      This setting is not in use for the GeoDMS 8 serie.
<!-- -->
-   **RecentFiles**: the set of recently opened configuration files.

Multiple other entries might be stored in the registry. These are configuration settings from a specific configuration that can be used in other projects as well. Use the following syntax to configure such settings:

<pre>
container ConfigSettings 
{
   container Overridable 
   {
      parameter&lt;string&gt; CBSDir: ['%sourceDataDir%/CBS'];
   }
}
</pre>

The values for these settings can be edited with the [[GUI|GeoDMS GUI]], Tools > Options > Tab: Configuration