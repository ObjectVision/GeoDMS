*[[Aggregation functions]] modus_weighted*

## syntax

- modus_weighted(*a*, *weight*)
- modus_weighted(*a*, *weight*, *relation*)

## definition

- modus_weighted(*a*, *weight*) results in the [[parameter|parameter]] with the **value of attribute *a* for the maximum sum** of the values of the *weight* [[attribute]].
- modus_weighted(a, *weight*, *relation*) results in an attribute with the **values of attribute *a* for the maximum sum** of the values of the *weight* attribute, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## applies to

- attribute *a* with uint2, uint4, uint8, uint16, uint32 or bool [[value type]] 
- attribute *weight* with Numeric value type
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The values unit of the resulting [[data item]] should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2. The domain unit of [[arguments|argument]] *a*, *weight* and *relation* must match.

## example

<pre>
1. parameter&lt;uint32&gt; modus_wLifeStyleCode                := 
    <B>modus_weighted(</B>City/LifeStyleCode, City/NrInhabitants<B>)</B>; result = 0
2. attribute&lt;uint32&gt; modus_wLifeStyleCodeRegion (Region) :=
    <B>modus_weighted(</B>
       City/LifeStyleCode
     , City/NrInhabitants
     , City/Region_rel
  <B>)</B>;
</pre>

| City/LifeStyleCode | City/NrInhabitants | City/Region_rel |
|-------------------:|-------------------:|----------------:|
| 2                  | 550                | 0               |
| 0                  | 525                | 1               |
| 1                  | 300                | 2               |
| 0                  | 500                | 1               |
| 1                  | 200                | 3               |
| 1                  | 175                | null            |
| null               | null               | 3               |

*domain City, nr of rows = 7*


| **modus_wLifeStyleCode** |
|-------------------------:|
| **2**                    |
| **0**                    |
| **1**                    |
| **1**                    |
| **null**                 |

*domain Region, nr of rows = 5*

## see also

- [[modus]]