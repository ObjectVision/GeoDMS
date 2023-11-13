*[CalcCache](CalcCache "wikilink") [Managing
Files](CalcCache_Managing_Files "wikilink")*

## introduction

As [CalcCaches](CalcCache "wikilink") should only be deleted as a
whole, a way of managing is also to use multiple smaller
[CalcCaches](CalcCache "wikilink") for different alternatives/scenarios
within a project.

By default the CalcCache folder name is derived from the project's
folder name. It can be overruled by the environment variable
GEODMS_DIRECTORIES_CALCCACHEDIR (starting from GeoDMS version 7.029).
This offers two ways of working with multiple CalcCaches:

1.  Copying/renaming the project folder;
2.  Running the GeoDmsGui.exe or GeoDmsRun.exe from a batch file in
    which you control the location of the
    [CalcCache](CalcCache "wikilink") folder, see example.

The following two commands can be used to run
[GeoDMS](GeoDMS "wikilink") version 7.031, installed on it's default
location with as project: MicroTst.dms and with as CalcCacheDir:
*C:\\Localdata\\MyNewCalcCache*. The easiest way to execute these
commands is by creating a batch file with these commands and run the
batch file.

`SET GEODMS_DIRECTORIES_CALCCACHEDIR=C:\localdata\MyNewCalcCache`

`"C:\Program Files (x86)\ObjectVision\GeoDms7031\GeoDmsGui.exe"  "C:\GeoDms\tst\Operator\cfg\MicroTst.dms"`

The result will be an activated [GeoDMS GUI](GeoDMS_GUI "wikilink").
Intermediate (results) will be stored in a subfolder of the CalcCache
folder: *C:\\Localdata\\MyNewCalcCache*. This also applies when a new
project is opened with this [GUI](GeoDMS_GUI "wikilink") (File \> Open
Configuration File). The CalcCacheDir for this new project will then
still be *C:\\Localdata\\MyNewCalcCache*, as long the
[GUI](GeoDMS_GUI "wikilink") is running within this context .