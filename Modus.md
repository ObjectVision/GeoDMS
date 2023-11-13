*[[Aggregation functions]] modus*

## syntax

- modus(*a*)
- modus(*a*, *relation*)

## definition

- modus(*a*) results in a [[parameter]] with the **most occurring** non [[null]] value of [[attribute]] *a*. 
- modus(*a*, *relation*) results in an attribute with the **most occurring** non null value of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the [[relation]].

## description

An attribute might contain multiple most occurring non null values. In these cases, the modus function results in the lowest most occurring non null value, independent from the sequence of the values in the attribute, see also example 2.

## applies to

- attribute *a* with uint2, uint4, uint8, uint16, uint32 or bool [[value type]]
- *relation* with value type of the goup CanBeDomainUnit

## conditions

1. The values unit of the resulting [[data item]] should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2. The domain of [[argument]] *a* and *relation* must match.

## example 1

<pre>
parameter&lt;uint32&gt; modusLifeStyleCode                      := <B>modus</B>(City/LifeStyleCode); result = 1
attribute&lt;uint32&gt; modusLifeStyleCodeRegion (RegionDomain) := <B>modus</B>(City/LifeStyleCode, City/Region_rel);
</pre>

| City/LifeStyleCode | City/Region_rel |
|-------------------:|----------------:|
| 2                  | 0               |
| 0                  | 1               |
| 1                  | 2               |
| 0                  | 1               |
| 1                  | 3               |
| 1                  | null            |
| null               | 3               |

*domain City, nr of rows = 7*

| **modusLifeStyleCode** |
|-----------------------:|
| **2**                  |
| **0**                  |
| **1**                  |
| **1**                  |
| **null**               |

*domain Region, nr of rows = 5*

## example 2

<pre>
parameter&lt;uint32&gt; modusSeqA := <B>modus(</B>City/SeqA); result = 1
parameter&lt;uint32&gt; modusSeqB := <B>modus(</B>City/SeqB); result = 1
parameter&lt;uint32&gt; modusSeqC := <B>modus(</B>City/SeqC); result = 1
parameter&lt;uint32&gt; modusSeqD := <B>modus(</B>City/SeqD); result = 1
</pre>

| City/LifeStyleCode | City/Region_rel |
|-------------------:|----------------:|
| 2                  | 0               |
| 0                  | 1               |
| 1                  | 2               |
| 0                  | 1               |
| 1                  | 3               |
| 1                  | null            |
| null               | 3               |

*domain City, nr of rows = 7*

| **modusLifeStyleCode** |
|-----------------------:|
| **2**                  |
| **0**                  |
| **1**                  |
| **1**                  |
| **null**               |

*domain Region, nr of rows = 5*

## see also

- [[modus_weighted]]