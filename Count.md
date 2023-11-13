*[[Aggregation functions]] count*

## syntax

- count(*a*)
- count(*a*, *relation*)

## definition

- count(*a*) results in an uint32 parameter with **the number of** non [[null]] values of [[attribute]] *a*.
- count(*a*, *relation*) results in a an uint32 attribute with **the number** of non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## applies to

- attribute *a* with Numeric or Point value type 
- *relation* with value type of the group CanBeDomainUnit

## conditions

The domain unit of [[arguments|argument]] *a* and *relation* must match.

## example

<pre>
1. parameter&lt;uint32&gt; countNrInh                := <B>count(</B>City/NrInhabitants<B>)</B>; result = 6
2. attribute&lt;uint32&gt; countNrInhRegion (Region) := <B>count(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
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

| **countNrInhRegion** |
|---------------------:|
| **1**                |
| **2**                |
| **1**                |
| **1**                |
| **0**                |

*domain Region, nr of rows = 5*

## see also

- [[pcount]]