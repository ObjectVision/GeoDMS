The GeoDMS calculates and visualises data. Therefore [[tree items|tree item]] can to be configured, referring to source data or calculation results. These items are called **data items**.

From a technical perspective a data item is a reference to a [memory array](https://en.wikipedia.org/wiki/Array_data_structure).

[[StorageManagers|StorageManager]] are used to read data from [files](https://en.wikipedia.org/wiki/Computer_file) and [databases](https://en.wikipedia.org/wiki/Database) into [memory arrays](https://en.wikipedia.org/wiki/Array_data_structure) and to write
data from these arrays to files.

Small amounts of data, like [[parameter]] values or class breaks of [[Classifications|classification]], are often
stored in [[configuration files|Configuration File]].

Configure [[expressions|expression]] to calculate with data items.

## types

Two types of data items are distinguished:

1.  [[parameter]]
2.  [[attribute]]

## multi-dimensional

Most data items are one-dimensional. The GeoDMS also supports the following multi-dimensional (spatial) datastructures:

1.  [[point]] data items: two-dimensional items with for each instance an X and an Y value.
2.  sequence of [[point]] data item: items with for each instance a non fixed number of coordinates, used for [[arc]] and [[polygon]] data items.

Different [[value types|value type]] and [[composition types|Composition]] are used to configure multi-dimensional data structures, see [[units|unit]]. 