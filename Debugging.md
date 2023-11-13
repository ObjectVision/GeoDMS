From here it is assumed you are able to fully compile the GeoDMSRun.exe executable for batch file processing by following the steps at [[Compilation|Compiling the GeoDMS c   code]]. Debugging is an essential part of software development, no different for GeoDMS. There are two ways to debug a program, debugging the command line program and debugging the GUI.

## Debugging GeoDMSRun.exe

In MSVC2019 set GeoDMSRun as startup project. Now open its properties and go to the Configuration Properties >> Debugging tab and in Command arguments fill in the full path to the .dms file you want to debug including the item you are interested in to run.

## Debugging GeoDMSGUI.exe

Debugging with MSVC2019 and the GUI is more dynamic compared to debugging GeoDMSRun.exe. First of all make sure you copy GeoDMSGui.exe from the official installation into your output folder ie bin/x64/Release. Next again in Configuration Properties >> Debugging at the Command section add $(OutDir)/GeoDmsGui.exe. And at the Command Arguments section fill in /noconfig to make sure an empty GUI is started with no configuration file.