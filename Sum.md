*[[Aggregation functions]] sum*

## syntax

- sum(*a*)
- sum(*a*, *relation*)
- sum_x(a) or sum_x(a, relation) where x in {(|u)int(8|16|32|64), float64, (i|u|d)point }

## definition

- sum(*a*) results in a [[parameter]] with the **sum** of the non [[null]] values of [[attribute]] *a*.
- sum(*a*, *relation*) results in an attribute with the **sum** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the [[relation]].

## applies to

- attribute *a* with Numeric [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The values unit of the resulting data item should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2. The domain unit of argument *a* and  *relation* must match.

## example

<pre>
parameter&lt;uint32&gt; sumNrInh                := <B>sum(</B>City/NrInhabitants<B>)</B>; result = 2250
attribute&lt;uint32&gt; sumNrInhRegion (Region) := <B>sum(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
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


| **sumNrInhRegion** |
|-------------------:|
| **550**            |
| **1025**           |
| **300**            |
| **200**            |
| **0**              |

*domain Region, nr of rows = 5*