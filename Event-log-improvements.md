*[[Under Study]] Event log improvements*

The main function of the eventlog is to:
- log user actions
- inform the user on the progress of processes to calculate model results.
- report on unexpected events that affect such processes.

The current implementation of the eventlog can be improved for this purpose.

## General

Errors and warning now result in many logged messages.

It would be nice to present overview information on the whole process. After data is requested in a view, the eventlog can start with messages such as:
- Start processing
- Update meta information for xxx (throttle
- Update primary data
- ... *messages* ...
- Finished processing (*system back to idle*)

## Throttling

To avoid wasting resources on overflowing a user with logging data, while still periodically reporting progress, a (per category) time since last report in combination with severity or skip-count may be used to decide (further) suppression of messaging, provided that:

- time since action that ended idle-time is used to scale time since last report
- (if an end goal is known) distance to end goal: idem 
- weight of the message

## Events

Event categories:

- Reading item yyy from data source xxx
- Writing item yyy to data sink xxx
- Warnings
- Progress of operations
- HTTPS requests
- Resource usage (requires a separate design, see link below).

Not: FailReasons, as they are accessible at the failed items or displayed in an Failure Dialog when they blocked a requested operation.

## Improvements

1.  Errors > minimalize redundant labels (e.g. logging ThrowingItemErrror > error)
2.  A new tab in Tools > Options > EventLog, in which you can enable or
    disable message categories, with as extra options:
    1.  show data/time for all messages
    2.  show thread number,
    3.  error number Furthermore it would be nice to have a summary of
        the process between Start processing and Finished processing
        with duration and maximum RAM usage.
3.  Toggle option to sort by thread number, and show/hide duplicates.
4.  Using segments for progress information on time consuming operators (e.g.: processing segment 1 from 100)
5.  More operators showing progress information (e.g. for_each with large numbers of items)
6.  Empty Working Pages > more clear label like Empty RAM and deduplicating
7.  Reconsider current Component type, now: TListBox. Short term alternatives: TEdit or TWebBrowser, of owner drawn listbox; medium term alternative: QtWebView (QtWebKit) would enable hyperlinks, better text selection and ordering.
8.  Fix issue of not repainting EventLog after appending new messages.
9.  Copy-text function (1 line, all lines, some lines (requires a multi-select TListBox), selected text (requires an alternative component).
10. Clear function (Alt 22)
11. Markup functions: bold, links to items: requires new GUI framework or intensification of TWebBrowser.
12. NOT: Resource usage: design which targets to trace, how to trace them and how to present a snapshot of linked resources in a separate lemma.
13. Do not [[display errors|Error dialog improvements]] that came from evaluating the calculation rule of items.
14. Do not [[display errors|Error dialog improvements]] that disable the calculation of an item due to item not found, value type or domain ncompatibility error etc.