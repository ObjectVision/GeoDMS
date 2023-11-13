[[images/GUI/qt.png]]

The GeoDMS GUI is used to visualise source data, calculation results and model logic. It presents both the model results as well as the steps needed to generate the results.

The GUI contains a set of viewers for [maps](wikipedia:Map#Geographic_maps "wikilink"), [tables](wikipedia:Table_(information) "wikilink") and
[graphs](wikipedia:Wikipedia:Graphs_and_charts "wikilink"). Classifications and visualisation styles can partly be edited in the
GUI, but will not be stored persistently (see the section [[How To Model]] for storing these settings persistently).

It also contains viewers for descriptive/meta information and process schemes to show the model logic.

## software

The current GUI (started in 2023) is developed, like the [[GeoDMS Engine]] in C++, using the [Qt](https://doc.qt.io) library.

Earlier versions of the GUI were developed in Delphi. Somes issues related to Delphi (like the flickering views) were difficult to solve, the Qt library offers many interesting functions for improving the current GUI.  

The GUI component is an executable called: GeoDmsGuiQt.exe. The component is part of the GeoDMS software to be installed from the [releases](https://github.com/ObjectVision/GeoDMS/releases) section.

## how to use

See [[User Guide]] section GeoDMS GUI.