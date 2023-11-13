*[[Aggregation functions]] variance*

## syntax

- var(*a*)
- var(*a*, *relation*)

## definition

- var(*a*) results in a [[parameter]] with the **variance** of the non null values of [[attribute]] *a*.
- var(*a*, *relation*) results in an attribute with the **variance** of the non [[null]] values of attribute a, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation].

## description

The variance is calculated as the square of the sum of the differences of each value and the mean value of the distribution. This square is divided by the number of values (n).

This way of calculating the variance is different from e.g. the ms Excel VAR function, which divides the sum of the differences by n-1.

## applies to

- attribute *a* with Numeric [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1.  The [[value type]] of [[argument]] a and the resulting [[data item]] must match.
2.  The domain of arguments a and relation must match.

## example

<pre>
1. parameter&lt;float32&gt; varNrInh                := <B>var(</B>float32(City/NrInhabitants)<B>)</B>; result = 24166.67
2. attribute&lt;float32&gt; varNrInhRegion (Region) := <B>var(</B>float32(City/NrInhabitants), City/Region_rel<B>)</B>;
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


| **varNrInhRegion** |
|-------------------:|
| **0**              |
| **156.25**         |
| **0**              |
| **0**              |
| **null**           |

*domain Region, nr of rows = 5*

## see also

[[Standard deviance(sd)]]