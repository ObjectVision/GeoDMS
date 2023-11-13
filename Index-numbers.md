GeoDMS operations work on [arrays](https://en.wikipedia.org/wiki/Array_data_structure) that contain the values of an attribute for each element of the domain of that attribute. Each domain element is identified by an ordinal, which identifies the position of the value for that element in each array for
that domain. This ordinal can be a number and is then usually zero-based or a (row, col) raster-point identification (or (col, row), depending on
the [ConfigPointColRow] setting in the [[config.ini]].

By default the GeoDMS uses zero based numbers (both one- and two-dimensional) for index numbers. The start and end position can be overruled with the [[range]] function

Index numbers are used in [[relations|relation]].

## id function

Index numbers can be calculated with the [[id]] function. The [[values unit]] of an index number is the [[domain unit]] for which the index numbers are calculated.