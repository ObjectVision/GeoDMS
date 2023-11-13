When you have downloaded in installed GeoDMS and downloaded and prepared
the Vesta configuration and sourcedata you can begin.

If you do not know how something works, you can read the GeoDMS
tutorials in the [GeoDMS Academy](GeoDMS_Academy "wikilink").

## opening a single strategy

[thumb\|300x300px\|GeoDMS GUI landing
screen](File:StartScreen.png "wikilink") You can open a single
strategy, for example, s2d_D_Restwarmte. By opening the run file in the
GeoDMS GUI from ProjDir\\VestaDV\\Runs\\s2d_D_Restwarmte.dms

You will then see the GeoDMS GUI as displayed here on the right.

If you navigate to SharedInvoer, you'll see a set of parameter settings
which are the input variables for the model and which you can change.
Right click on SharedInvoer and choose Edit Config Source. You will now
be redirected to a text editor in which you can these parameters. If
not, visit [Module 0: Install GeoDMS GUI and setup a
configuration](Module_0:_Install_GeoDMS_GUI_and_setup_a_configuration "wikilink")
and see the section about setting up a configuration file editor.
[thumb\|300x300px\|SharedInvoer.dms in a configuration file
editor](File:SharedInvoer.png "wikilink")

You should see the text editor as in the figure on the right. Here you
can should the study area (StudieGebied_config) and choose whether there
is flip file used, and which Scenario for that study area will be used.
If you change anything, save the file and reopen the project in the
GeoDMS by pressing Alt + R, or choosing *Yes* when returning to the
GeoDMS.

To view and calculate results for the strategy you are in, navigate to
LeidraadResultaten/Exports/ESDL.

Here you can find several items which when opened will generate the
results for that strategy and save the output files.

For example, *Generate_PerPlanRegio_All_metRES* will export all
indicators for the study area and also the entire RES area for all years
(start year, 2030, 2040, 2050).

To view the results for, say 2050 and only the study area, navigate to
/LeidraadResultaten/Exports/ESDL/PerRekenstap/Res2050/PerStudyArea/Regio_selectie.

## calculating all strategies for a scenario-study area combination

First close the GeoDMS GUI completely. Then in windows explorer navigate
to C:\\GeoDMS\\ProjDir\\VestaDV\\Runs\\path and open the file called
*set.bat* with a text editor. Here change the directories to the paths
which your computers uses.

Then, for example for the use case Havenstad, open the file located in
C:\\GeoDMS\\ProjDir\\VestaDV\\Runs called *batch_leidraad_ESDL -
Amsterdam.bat* by double-clicking it.

This should open a windows prompt window and start calculating. This
could take a while depending on the computing power of you machine.