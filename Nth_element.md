*[[Aggregation functions]] nth_element*

## description

This operator returns the value of the *n*th element of ranked attribute *a*, which can be done in partition.

## syntax

- nth_element(*a*, *n*)
- nth_element(*a*, *n*, *relation*)

## definition

- nth_element(*a*, *n*) results in a [[parameter|parameter]] with the **nth element of the ascending sorted attribute** *a*.
- nth_element(*a*, *n*, *relation*) results in an [[attribute]] with the **nth elements of the ascending sorted attribute** *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## applies to

- attribute *a* with numeric (uint2, uint4, uint8, uint32 or bool [[value type]])
- attribute *n* with uint32 value type
- *relation* with [[value type]] of the group CanBeDomainUnit

## conditions

1.  The values unit of the resulting [[data item]] should match with regard to value type and [[metric]] with  the values unit of attribute *a*
2.  The domain of [[argument]] *a* and *relation* must match,
3.  The domain of argument *n* must be the same as the resulting domain or *n* must be a parameter.

## since version

5.61

## example

```
1. parameter<uint32>  nth_elementNrInh                := nth_element(City/NrInhabitants, 2); result = 300
2. attribute<float32> nth_elementNrInhRegion (Region) := 
     nth_element(
          City/NrInhabitants
        , 0
        , City/Region_rel
     );
```

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


| **nth_elementNrInhRegion** |
|---------------------------:|
| **550**                    |
| **500**                    |
| **300**                    |
| **200**                    |
| **null**                   |

*domain Region, nr of rows = 5*




## see also

- [[nth_element_weighted]]
- [[rth_element]]