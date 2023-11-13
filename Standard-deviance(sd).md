*[[Aggregation functions]] standard deviance*

## syntax

- sd(*a*)
- sd(*a*, *relation*)

## definition

- sd(*a*) results in a [[parameter]] with the **standard deviance** of the non [[null]] values of [[attribute]] *a*.
- sd(*a*, *relation*) results in an attribute with the **standard deviance** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## description

The standard deviation is calculated as the square of the sum of the differences of each value related to the mean value. This square is divided by the number of values (n) and from this division the square root is taken.

This way of calculating the standard deviation is different from e.g. the ms Excel STDEV function, which divides the sum of the differences by n -1.

## applies to

- attribute *a* with Numeric [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1.  The value type of [[argument]] *a* and the resulting [[data item]] must match.
2.  The domain of arguments *a* and *relation* must match.

## example

<pre>
1. parameter&lt;float32&gt; sdNrInh                := <B>sd(</B>float32(City/NrInhabitants)<B>)</B>;result = 155.46
2. attribute&lt;float32&gt; sdNrInhRegion (Region) := <B>sd(</B>float32(City/NrInhabitants), City/Region_rel<B>)</B>;
</pre>

| City/NrInhabitants | City/Region_rel |
|-------------------:|----------------:|
| 550                | 0               |
| 525                | 1               |
| 300                | 2               |
| 500                | 1               |
| 200                | 3               |
| 175                | null            |
| null               | 3               |

*domain City, nr of rows = 7*

| **sdNrInhRegion** |
|------------------:|
| **0**             |
| **12.5**          |
| **0**             |
| **0**             |
| **null**          |

*domain Region, nr of rows = 5*

## **see also**

- [[(var)iance|variance]]