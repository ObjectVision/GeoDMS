Some Elements of the GeoDmsGUI can easily be customized to provide a
visual appearance for specific projects.

Customizable GuiElements are:

-   the SplashScreen, displayed during loading a project configuration,
    usually an image with an widht of 350 pixels and a hight of 200
-   the AboutBox, accessible from the main menu Help->About, usually
    presenting the developers of the project, and version info.
-   the TitleCaption, the text shown in the tile bar of the main
    window.
-   the ProgramIcon, which is shown in the start-bar and on the desktop,
    usually a small image of Earth.
-   the ToolbarImage, a small image (say: 100 x 35 pixels) that will be
    shown in the Toolbar at the right.

They can be defined in a Dynamic Link Library, usually
`%projdir%/bin/DmsProject.dll`.

The GeoDmsGui.exe opens the DLL before reading a configuration file in
order to display the SplashScreen.
The usual name and location of the DLL can be overridden by the
DmsProjectDll key in the general section of the
[Config.ini](Config.ini "wikilink")