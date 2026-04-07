*[[Aggregation functions]] frequency_table*

## syntax

- frequency_table(*a*)
- frequency_table(*a*, *relation*)

## definition

- frequency_table(*a*) results in a [[parameter]] with a string listing all non-[[null]] values of [[attribute]] *a* together with how often each value occurs, separated by "; ".
- frequency_table(*a*, *relation*) results in an attribute with such strings, one per partition defined by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*. Each partition string contains the value-count pairs for the non-null values of *a* belonging to that partition.

## description

The result per partition is a string of the form `value1: count1; value2: count2; ...`, where:

- values are listed in ascending order (the order defined by the [[values unit]] of attribute *a*),
- only values with a non-zero count are included,
- null values in *a* are **excluded** from the counts.

To include null values in the frequency table, use [[frequency_table_with_null]] instead.

## applies to

- attribute *a* with any scalar [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The domain of [[argument]] *a* and *relation* must match.

## since version

14.4.0

## example

```
parameter<string> freqLifeStyleCode := frequency_table(City/LifeStyleCode);
// result = "0: 2; 1: 3; 2: 1"

attribute<string> freqLifeStyleCodePerRegion (Region) := frequency_table(City/LifeStyleCode, City/Region_rel);
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

| **freqLifeStyleCodePerRegion** |
|-------------------------------|
| **"2: 1"**                    |
| **"0: 2"**                    |
| **"1: 1"**                    |
| **"1: 1"**                    |
| **""**                        |

*domain Region, nr of rows = 5*

City 6 (LifeStyleCode = null) is excluded. City 5 (Region_rel = null) is excluded from all groups.

## see also

- [[frequency_table_with_null]] - variant that includes null values of *a* in the frequency table
- [[as_unique_list]] - like frequency_table but only lists the distinct values, without the counts
- [[modus]] - returns only the most frequently occurring value
- [[unique_count]] - returns the number of distinct non-null values
