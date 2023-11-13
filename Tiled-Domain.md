A tiled domain is a [[domain unit]] with an additional [tiling](https://en.wikipedia.org/wiki/Tiled_rendering).

For one-dimensional domain units the word segmented is the synonym for a tiled domain.

A tiled domain can be configured with the [[TiledUnit]] function.

Domain units with [[value types|value type]] of more that **2** bytes can be tiled, so:

- all Points Type value types: [[spoint]], [[ipoint]], [[wpoint]], [[upoint]]
- [[uint32]]
- [[int32]]
- [[uint64]]
- [[int64]]

thus not (u)int16, (u)int8, nor [[uint4]], [[uint2]] or [[bool]] can be tiled.

## tiles as a set of Ranges

Each domain unit has a range of valid values, determined by the inclusive Lower Bound and exclusive Upper Bound.

For Point value types this result in a rectangular box of valid values.