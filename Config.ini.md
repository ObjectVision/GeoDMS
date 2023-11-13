The **config.ini** file is placed in the [[ConfigDir|Folders and Placeholders]]

It contains project related, user related and configuration related settings that can be edited directly and/or by the [[GeoDms Gui]]->
MainMenu -> Tools -> Options dialog. There are also editable user and/or machine related settings stored in the windows registry.

Here follows a list of all sections and keys that are read and stored by the GeoDMS. After each key the default value is given, if any.
If a default value is OK, you don't have to include the related key.

<pre>
[general]
DmsProjectDLL=%projdir%/bin/DmsProject.dll ; default OK, Used by GeoDmsGui.exe to find the project specific GuiElements
ConfigPointColRow=0                        ; default OK for the Land Use Scanner. Determines that point functions and point config data is interpreted as [Row,Col]

[configuration]
classifications=Classifications            ; default OK, can refer to multiple containers separated by ';'
geography=geography                        ; default NOT OK for the Land Use Scanner. Should be /Geografie for NLL2.0
units=Units                                ; default OK, can refer to multiple containers separated by ';'

[Case management]
TemplateContainers= ; should be Rekenschemas in NLL2.0, can refer to multiple containers separated by ';'. Used By GeoDmsGui.exe in TfrmNewCase
CaseContainer=Cases ; default OK

[Directories]
orgconfig=@
projdir=..
dataDir=%sourceDataProjDir%/data
CalcCacheDir=%localDataProjDir%/CalcCache
LocalDataProjDir=%LocalDataDir%/%projName%
SourceDataProjDir=%SourceDataDir%/%projName%

[Colors]
NoData=8421504       ; $808080 = clGrey, NoDataColor is Obsolete`
Background=16777215  ; $FFFFFF = clWhiteValid=16711680 ; $FF0000 = clBue`
Invalidated=16711935 ; $FF00FF = clPurpleFailed=255 ; $0000FF = clRed`
Exogenic=65280       ; $00FF00 = clGreenOperator=0 ; $000000 = clBlack`
RampStart=16711680   ; $FF0000 = clBlueRampEnd=255 ; $0000FF = clRed`

[CalcCache]
SwapfileMinSize=4000 ; See CalcCache management

[messages]
Hide0=0              ; Indicator that the user requested suppression of Optional Info on Disclaimer
Hide1=0              ; Indicator that the user requested suppression of a SourceDescr explanation
Hide2=0              ; Indicator that the user requested suppression of the CopyRight notification

</pre>

The following settings that are now stored in the user specific HKEY_CURRENT_USER/Software/ObjectVision/Dms:

<pre>
[HKEY_CURRENT_USER/Software/ObjectVision/Dms]
DmsEditor     = C:\Program Files\Crimson Editor\cedt.exe /L:%L "%F"
ErrBoxHeight  = 508
ErrBoxWidth   = 300
GeneralUrl    = https://www.geodms.nl
HelpFileUrl   = https://www.geodms.nl
LastConfigFile=
LocalDataDir  = C:\LocalData
SourceDataDir = C:\SourceData
StatusFlags   = 134 ; default= SF_SuspendForGUI + SF_ShowStateColors + SF_ToolBarVisible )

StatusFlags can be any combination of the following enumeration values:
const
 SF_AdminMode       =   1;
 SF_SuspendForGUI   =   2;
 SF_ShowStateColors =   4;
 SF_TraceLogFile    =   8;
 SF_TreeViewVisible =  16;
 SF_DetailsVisible  =  32;
 SF_EventLogVisible =  64;
 SF_ToolBarVisible  = 128;
</pre>

Furthermore, a list of recently opened configurations is maintained in HKEY_CURRENT_USER/Software/ObjectVision/Dms/RecentFiles.

## planned changes

To improve source code management, most config.ini settings will be stored elsewhere in a version after GeoDms 5.60. 
- Project related settings will be moved to the container _ConfigSettings_ included in/from the main configuration file.
- User related settings will be moved to the user specific HKEY_CURRENT_USER/Software/ObjectVision/Dms.
- Machine related settings will be moved the machine specific HKEY_LOCAL_MACHINE/SOFTWARE/ObjectVision/Dms.

1.  [general] A reference to the project specific DmsProject.dll must be found before a configuration is loaded (in order to load the SplashScreen first). Also the value for ConfigPointColRow needs to be known before reading of the configuration starts. Therefore, these settings will remain in the configuration specific config.ini.
2.  The setting under [configuration] and [Case Management] are related to a specific configuration and will be read from the _ConfigSettings_.
3.  [Directories] is related to both the machine and the project. However, since %LocalDataDir% and %SourceDataDir% are already stored in the windows registry and LocalDataProjDir and SourceDataProjDir should be defined as a function of these registry settings and possibly %projName% (their default values are usually sufficient), LocalDataProjDir and SourceDataProjDir will be defined in _ConfigSettings _as well. Their default values will remain.
4.  [Directories] projDir sometimes needs to be reset if branches of configurations end up at a higher level in the folder hierarchy and therefore remains in config.ini. Furthermore, projDir may be required in the evaluation of the (default value of) DmsProjectDLL.
5.  [Directories] orgConfig indicates wether a configuration may be overwritten and needs to be changed by the GeoDmsGui.exe when a configuration is saved, but the GeoDMS can then equally well store/change this setting in _ConfigSettings_.
6.  The keys under [Colors] are considered as user related settings.
7.  The Hide0..Hide2 keys under [messages] indicate if a user requested not to show future Disclaimers, SourceData notification and copyright notifications. They are considered both project related (since the message might be different for different projects) and user related (since a new user might not have seen a notification). These flags will be stored as StatusFlags messages in the user specific HKEY_CURRENT_USER/Software/ObjectVision/Dms with a keyname determined by _ConfigSettings_ messages which will default to MessageFlags_%projName%. The default value for this key will be that all notifications should be shown.

The DmsEditor, LocalDataDir and SourceDataDir settings are considered as
machine related and will be moved to
HKEY_LOCAL_MACHINE/SOFTWARE/ObjectVision/Dms
The GeneralUrl and HelpFileUrl are considered as project related and
will be stored in the configuration specific and therefore project
specific /ConfigSettings.

## migration policy

After implementation of the above described migrations, the settings
will continue to be read but not stored from the original locations
during some versions, possibly with the generation of a warning. As long
as the above is not implemented and the config.ini contains user related
settings, project source managers are advised to put config.ini on the
ignore list of their source code constrol system (such as subversion).

## see also

- [[GeoDms Gui]]
- [[GeoDmsRun]]