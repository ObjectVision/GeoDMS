## install Wine

In order to run GeoDMS on Linux we recommend using wine which can be downloaded [here](https://wiki.winehq.org/). Follow the installation instructions for your specific Linux flavor from the wine download [page](https://wiki.winehq.org/Download). After installation open a terminal and confirm that wine works using for instance `wine --version`.


## configure wine for GeoDMS
Open a terminal and run `winecfg` to open the wine configuration manager. Navigate to the libraries tab and make sure that the following libraries prioritize native- over the build-in implementation:

![image](https://github.com/ObjectVision/GeoDMS/assets/96182097/ffd122a5-c47c-4988-b844-d14a329782df)

## installing GeoDMS

Download the desired GeoDMS version from
[GeoDms Releases](https://github.com/ObjectVision/GeoDMS/releases) and execute the following
command from the directory where you saved the setup program:

`$ wine GeoDms14.3.2-Setup-x64.exe`

**Note: the filename of the setup program will vary for different
versions of GeoDMS**

## Running dms configuration using GeoDmsRun.exe
GeoDmsRun.exe has been experimentally tested. In a terminal navigate to the GeoDMS installation folder and run:

`wine ./GeoDmsRun.exe /home/cicada/Documents/dev/wine_geodms/test_config/GeoDMS-Test/Storage_gdal/cfg/regression.dms export_all`

## known issues
GeoDmsGuiQt.exe does not work at the moment using wine see:
![image](https://github.com/ObjectVision/GeoDMS/assets/96182097/a7f71abf-f1f7-4437-b4ab-1122a9f98830)


When opening .dms configuration files, you will notice errors. These
errors possibly have to do with incompatibilities in the Multi-threading
DLL's supplied with Wine.

## testing & debugging using Wine

It's best to primarily test with the GeoDmsRun.exe, to make sure that
runtime-problems don't stem from GUI-elements. Only when tests using
GeoDmsRun.exe are running smoothly, should you test using GeoDmsGui.exe.

Wine supports extensive debugging features. These can be activated using
the following command-line syntax:

`$ WINEDEBUG=+relay,+seh,+tid,+loaddll wine GeoDmsRun.exe [/LLogFileName] {ConfigFileName} {ItemNames} > {DebugLogFile} 2>&1`

Notes:

-   `[/LLogFileName]` is optional
-   `{ConfigFileName}` and `{ItemNames}` should be provided as normal
    when using GeoDmsRun.exe
-   The `>` part is a Linux shell-directive to redirect the output of
    the process to the file specified after `>`. You could ofcourse
    leave this (and everything following it on the command-line) out, so
    that the output will be to the terminal instead, but that will cause
    a lot of output scrolling by quickly.
    It is also possible to use `>>` in order to *append* to that file
    (using `>` will overwrite each time the command-line is executed),
    but please note that repeated runs of the command-line using the
    append-directive may quickly result in a very big output file!
-   `{DebugLogFile}` is a path to a file that will be used for the
    (rather extensive!) Wine debugging output
-   The final `2>&1` is a Linux shell-directive to redirect both STDERR
    and STDOUT to the same location, being the output file specified as
    `{DebugLogFile}`

You can find more information on the available Debug Channels for Wine
[here](https://wiki.winehq.org/Debug_Channels)