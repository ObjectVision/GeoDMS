_[[User Guide GeoDMS GUI]]_ - Local machine Options dialog

The Local machine Options dialog can be activated from the Settings main menu option.

## dialog

The Local machine Options dialog can be activated from the Settings main menu option. The settings in this dialog are related to the interface between the GeoDms and the local machine/computer. 

[[images/GUI/local_machine_options.png]]

The following options can be set:

**Paths**
* **Local Data**: The folder configured here is used as the base folder for (temporary) results. See [[Folders and Placeholders]] for more information on directories and placeholders in the GeoDMS. The default setting for the Local Data is C:\LocalData. This default setting can be overruled here.
* **Source Data**: The folder configured here is used as the base folder for source data that is often used in multiple projects. See [[Folders and Placeholders]] for more information on directories and placeholders in the GeoDMS. The default setting for the Source Dir is C:\SourceData. This default setting can be overruled here.

**Configuration File Editor**
* **Application**: The path to an external text editor, used to view or edit configuration files. This [[Configuration File Editor]] is opened with the Edit > Open in Editor menu option. In order to open the editor with the relevant configuration file on the relevant line, set the Parameters option
* **Parameters**: Parameters are needed to open the Configuration File Editor with the relevant configuration file on the relevant line. The following 
parameters can be set (see also the blue button with the i):
   - %F: file, "" are needed for paths with spaces
   - -n%L: line number in the file
* **Set default parameters for the editor**: For Notepad ++ and Crimson Editor we have an advised set of parameters that can be set with this button. Based on the chosen application the correct set of parameters is chosen. 

**Parallel Processing**<br>
Parallel processing is a type of computation in which many calculations are carried out simultaneously by using multiple cores that perform tasks in separate threads. The current options are:
* **Suspend view update to favor gui**: keeps the GUI responsive by suspending further calculation steps whenever user events are queued, and resume these when idle.
* **Data-segment production as separate tasks**: split up operations into separate tasks per data-segment.
* **Multiple operations simultaneously**:performs multiple operations simultaneously.
* **Pipelined operations**: pipeline data-segment processing by delaying data-segment production to the tasks that requests such segments.

It is advised to use the default settings in most cases. For specific configurations/machines it might be useful to deviate from these setting.

**Treshold for memory flushing wait procedure**<br>
If your machine is using (almost) all it's memory, it becomes less responsive. With this treshold you can set a percentage of the total memory used at which the GeoDMS starts flushing the memory to stay responsive. In general if you increase the value, the GeoDMS calculates faster, but becomes less responsive for calculations with large datasets. In most cases we advice to use a value between 80% and 90%.

**Logging**<br>
* **Write TraceLog file**:
This option activates the generation of a log file (called trace.log) by the GeoDms. The trace.log file logs information about the calculation processes and can be used for debugging purposes. The log file is saved in the project specific subfolder of the configured Local Data placeholder.

