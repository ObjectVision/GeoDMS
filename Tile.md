## Tiled/segmented data

The GeoDMS calculates with memory arrays. These arrays can be split up in a set of smaller arrays for two reasons:

1. If the array size exceeds the available internal memory (especially in 32 bits mode)
2. performance

For one-dimensional [[domain units|domain unit]], the resulting data is called **segmented** (do not confuss the term **segmented** with [[segment]]).

For two-dimensional domain units (grid) the resulting data is called **tiled**.

The resulting domain unit is called a tiled domain. Use The [[TiledUnit]] function to configure an explicit tiled domain.