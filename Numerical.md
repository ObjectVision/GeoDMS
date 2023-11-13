Numerical data has meaning as a [measurement](https://en.wikipedia.org/wiki/Measurement), such as a the surface of an area or the length of a road. It can also be a count, such as the number of inhabitants in a municipality or the number of cattle of a farmer.

Two types of numerical data are distinguished:

### continuous

Continuous data represent [measurements](https://en.wikipedia.org/wiki/Measurement) their possible values cannot be counted and can only be described using intervals on the real number line. In the GeoDMS float32/float64 [[value types|value type]] are used for this type of data.

It is advised to configure [[values units|values unit]] with [[metric]] describing this type of data, for interpreting your data and to prevent illogical calculations

### discrete

Discrete data represent values that can be counted. The data is usually represented by positive integer values. In the GeoDMS we mainly use uint32/uint64 value types for this type of data (if the number of possible values is small, also uint8 and uint16 value types can be used).

Usually the name of the item already describes the type of count but also for this type of data it is often useful to configure values units with metric
describing this type of data to prevent illogical calculations.