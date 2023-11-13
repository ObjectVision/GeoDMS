*[[Recent Developments]]: Editing PP2*

Multiple improvements, including solving potential dead-locks, have been made to the [parallel processing](https://en.wikipedia.org/wiki/Multithreading_(computer_architecture))(PP2) functionality. This allows the GeoDMS to perform multiple calculation steps simultaneously.

Since version 7.174 we advice to enable this option for interactive working with the GeoDMS GUI. For batch processing, especially if you run multiple processes simultaneously, we advice to disbale the option. The option can be set from the Tools > Options > General Settings dialog (in future versions we plan to make it possible to overrule the default setting with a batch parameter).

Since version 7.196 PP2 enabled (as well as PP1 enabled) is the default setting when installing the GeoDMS.

## related issues

- [issue 1021](http://mantis.objectvision.nl/view.php?id=1021)