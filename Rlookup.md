*[[Relational functions]] rlookup*

## syntax

- rlookup(*foreign_key*, *primary_key*)

## definition

rlookup(*foreign_key*, *primary_key*) results in a <B>[[relation]] from the [[domain unit]] of the *foreign_key* [[argument]] towards the domain unit of the *primary key* argument</B> 

## applies to

- *foreign_key*: an [[attribute]] which can serve as [primary key](https://en.wikipedia.org/wiki/Primary_key) for the domain unit of the *primary_key* argument, e.g. a region code.
- *primary_key*: an attribute which can serve as primary key for it's own domain unit and with the the same [[values unit]] as the *foreign_key* attribute.

## description

We advise that the second argument can be used as primary key for the domain unit of this argument.

If multiple instances of the second argument occur, the resulting value will be the first [[index number|index numbers]] found.

## applies to

- attributes *foreign_key* and *primary_key* with Numeric, Point, uint2, unit4, bool or string [[value type]]

## conditions

The values unit of the arguments *foreign_key* and *primary_key* must match.

## example

<pre>
attribute&lt;Region&gt; Region_rel (City) := <B>rlookup(</B>City/RegionCode, Region/RegionCode<B>)</B>;
</pre>

| City/RegionCode |**Region_rel**|
|----------------:|-------------:|
| 100             | **0**        |
| 200             | **1**        |
| 300             | **2**        |
| 200             | **1**        |
| 400             | **3**        |
| null            | **null**     |
| 400             | **3**        |

*domain City, nr of rows = 7*

| Region/RegionCode |
|------------------:|
| 100               |
| 200               |
| 300               |
| 400               |
| 500               |

*domain Region, nr of rows = 5*