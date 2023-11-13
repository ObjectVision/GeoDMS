*[[Relational functions]] rjoin*

## syntax

- rjoin(*foreign_key*, *primary_key*, *values*)

## definition

rjoin(*foreign_key*, *primary_key*, *values*) results in an [[attribute]] with the <B>values of [[argument]] *values* for the [[domain unit]] of argument *foreign_key*</B>.

The resulting [[data item]] has the same [[values unit]] as the *values* argument and the same domain unit as the *foreign_key* argument.

## description

The rjoin function is advised if an attribute is available to relate domain (e.g. a region code), not being a [[relation]] in GeoDMS terms. If such a relation is available, use the faster [[lookup]] function.

A rjoin function is rewritten to a [[rlookup]] function (to create a relation) and a lookup function (to use the relation), see the example.

We advise that the *primary_key* argument can be used as [primary key](https://en.wikipedia.org/wiki/Primary_key) for the domain unit of this argument. If multiple instances of this argument occur, the resulting value will be based on the first [[index number|index numbers]] found.

## applies to

- *foreign_key*: an attribute which can serve as primary key for the domain unit of the *primary_key* argument, e.g. a region code.
- *primary_key*: an attribute which can serve as primary key for it's own domain unit and with the same values unit as the *foreign_key* attribute.
- *values*: attribute with the requested values to be looked up.

## conditions

1. The values unit of the arguments *foreign_key* and *primary_key* must match.
2. The domain unit of the arguments *primary_key* and *values* must match.

## example

<pre>
attribute&lt;degrees&gt; rjoinTemperature (City) := <B>rjoin(</B>City/RegionCode, Region/RegionCode, Region/Temperature<B>)</B>;

This is rewritten within the GeoDMS to:

attribute&lt;Region&gt;  Region_rel  (City) := rlookup(City/RegionCode, Region/RegionCode)
attribute&lt;degrees&gt; Temperature (City) := Region/Temperature[Region_rel];;
</pre>

| City/RegionCode |**rjoinTemperature**|
|----------------:|-------------------:|
| 100             | **12**             |
| 200             | **11**             |
| 300             | **null**           |
| 200             | **11**             |
| 400             | **14**             |
| null            | **null**           |
| 400             | **14**             |

*domain City, nr of rows = 7*

| Region/RegionCode | Region/Temperature |
|------------------:|-------------------:|
| 100               | 12                 |
| 200               | 11                 |
| 300               | null               |
| 400               | 14                 |
| 500               | 13                 |

*domain Region, nr of rows = 5*