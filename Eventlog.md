_[[User Guide GeoDMS GUI]]_ - eventlog

## eventlog

[[images/GUI/eventlog_with_messages.png]]

The eventlog component shows messages to inform on the progress in the calculation process and warnings and error that might occur during the process.
Colors are used to indicate the contents or the severity type of the message.

## tools

At the right side of the eventlog, 4 tools are available:

|tool|description|
|----|-----|
| [[images/GUI/tools/TB_copy.bmp]]| copy the selected eventlog messages to the clipboard.|
| [[images/GUI/tools/EL_selection.bmp]]| turn eventlog filter on/off, see next paragraph |
| [[images/GUI/tools/EL_clear.bmp]]| clear all eventlog messages.|
| [[images/GUI/tools/EL_scroll_down.bmp]]| scroll contents of message to bottom and keep up with the new log messages.|

## filter
[[images/GUI/eventlog_with_options.png]]

The eventlog filter is used to make a selection of relevant messages in the eventlog. The left side contains categories like _Calculation Progress_ and _Storage_. The relevant message categories can be selected with the check boxes. The eventlog filter also functions as a legend, indicating which text colors are used for which category.

The _Filter on text part_ of the eventlog is used to filter messages on text parts (case insensitive). The **Filter** button becomes active if a (new) text is typed into the textbox. With the button the filter is applied in the text box. The **Clear** button can be used to clear the contents of the textbox (if the box contains characters).

The _Filter message contents_ block is used for additional filter settings, with two subblocks:
* **Clear eventlog before**: This option can be set to clear the whole eventlog if a new configuration is opened (open configuration) and/or if the current configuration is reopened (reopen configuration).
* **Add to each message**: This option filters the contents of each message, the data/time, the thread number and the message category can be added or removed from each message.
     

