*[[Geometric functions]] point function*

[[images/point_function.png]]

## syntax

-  point(*column*, *row*, *pointunit*)

## definition

point(*column*, *row*, *pointunit*) results in a two-dimensional **[[point]] [[data item]]**.

The coordinates are derived from the one-dimensional *column* and *row* data items.

The third [[argument]] is the resulting point [[values unit]].

## description

The default order of column, row can be overruled in the [[config.ini]] with the rule:

*ConfigPointColRow=1*

If this setting is configured, the order in all point functions of the configuration becomes row, column. This row, column order is more common in graphs and geographic coordinate systems.

## applies to

- *column* and *row* [[data items|data item]] with (u)int16, (u)int32, float32 or float64 [[value type]]
- *pointunit* with wpoint, spoint, upoint, ipoint, fpoint or dpoint value type

## conditions

1. The value type of the *column* and *row* data items must match with each other and with the pointunit:
   - uint16 data items for wpoint [[units|unit]];
   - int16 data items for spoint units;
   - uint32 data items for upoint units;
   - int32 data items for ipoint units;
   - float32 data items for fpoint units;
   - float64 data items for dpoint units.
2. The [[domain units|domain unit]] of the *column*, *row* and resulting data items must match.

## since version

5.15

## example
<pre>
attribute&lt;fpoint&gt; pointXY (ADomain) := <B>point(</B>Ycoord, Xcoord, fpoint<B>)</B>;
</pre>

| Ycoord | Xcoord | **pointXY**          |
|-------:|-------:|----------------------|
| 401331 | 115135 | **{401331, 115135}** |
| 399476 | 111803 | **{399476, 111803}** |
| 399289 | 114903 | **{399289, 114903}** |
| 401729 | 111353 | **{401729, 111353}** |
| 398696 | 111741 | **{398696, 111741}** |

*ADomain, nr of rows = 5*