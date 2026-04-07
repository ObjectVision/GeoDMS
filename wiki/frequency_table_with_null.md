*[[Aggregation functions]] frequency_table_with_null*

## syntax

- frequency_table_with_null(*a*)
- frequency_table_with_null(*a*, *relation*)

## definition

- frequency_table_with_null(*a*) results in a [[parameter]] with a string listing **all** values of [[attribute]] *a* — including [[null]] values — together with how often each value occurs, separated by "; ".
- frequency_table_with_null(*a*, *relation*) results in an attribute with such strings, one per partition defined by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*. Each partition string contains the value-count pairs for all values of *a* (including null) belonging to that partition.

## description

The result per partition is a string of the form `value1: count1; value2: count2; ...`, where:

- values are listed in ascending order (the order defined by the [[values unit]] of attribute *a*),
- only values with a non-zero count are included,
- null values in *a* are **included** in the frequency table and are shown as `<null>: count`.

This function is identical to [[frequency_table]] except that null values in *a* are counted and included in the result string. Elements mapped to a null partition (null *relation* value) are still excluded from all groups.

## applies to

- attribute *a* with any scalar [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The domain of [[argument]] *a* and *relation* must match.

## since version

14.4.0

## example

```
parameter<string> freqLifeStyleCodeWithNull := frequency_table_with_null(City/LifeStyleCode);
// result = "0: 2; 1: 3; 2: 1; <null>: 1"

attribute<string> freqLifeStyleCodeWithNullPerRegion (Region) := frequency_table_with_null(City/LifeStyleCode, City/Region_rel);
```

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

| **freqLifeStyleCodeWithNullPerRegion** |
|---------------------------------------|
| **"2: 1"**                            |
| **"0: 2"**                            |
| **"1: 1"**                            |
| **"1: 1; &lt;null&gt;: 1"**          |
| **""**                                |

*domain Region, nr of rows = 5*

City 6 (LifeStyleCode = null, Region_rel = 3) is included in Region 3's count as `<null>: 1`. City 5 (Region_rel = null) is excluded from all groups.

## see also

- [[frequency_table]] - variant that excludes null values of *a* from the frequency table
- [[as_unique_list]] - like frequency_table but only lists the distinct values, without the counts
- [[modus]] - returns only the most frequently occurring value
- [[unique_count]] - returns the number of distinct non-null values
