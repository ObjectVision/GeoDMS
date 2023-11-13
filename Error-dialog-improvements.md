*[[Under Study]] Error dialog improvements*

Errors are used to inform the user a model result can not be calculated. Errors are presented in the treeview (red items), the detail pages, the event log and in dialog boxes. In this page we focus on improvements of errors in dialog boxes.

An error box should appear when:
- a syntax error in a geodms source file disables a valid interpretation of intended tree-items.
- expanding a container 
- requested data can not be read, calculated, visualized or written

(data of layer controls of) items with Integrity check Failures are shown with red background and with failure annotation and only initiate an error dialog when written or exported.

Meta Info level Failures that disable the proper evaluation of a template instantiation, for each evaluation or result unification only result in red items

- FIX: now expanding such container may incorrectly initiate an error dialog

## Improvements

- We will make a list of all potential errors and check the labeling for more down to earth labels
- Based on this list we also try to inform the user more on the probable cause of the error (modelling error or GeoDMS software error). This is always an indication.
- Hyperlinks (with markup) for clickable references to geodms source code
- For indirect expression, more information on the resulting (part of) the direct expression that is generated, see also [[Error tracking in indirect expressions]]
- repeat part of the syntax that probably causes the error, where possible
- context information only visible after clicking on a show-context button

## See also

- [[Event log improvements]]