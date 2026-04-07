*[[Aggregation functions]] average_entropy*

## syntax

- average_entropy(*a*)
- average_entropy(*a*, *relation*)

## definition

- average_entropy(*a*) results in a [[parameter]] with the **Shannon entropy** (in bits) of the non-[[null]] values of [[attribute]] *a*.
- average_entropy(*a*, *relation*) results in an attribute with the Shannon entropy (in bits) of the non-null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## description

The Shannon entropy of a set of N observations is defined as:

```
average_entropy(a) = H(a) = -∑ pᵢ · log₂(pᵢ)
```

where pᵢ = nᵢ / N is the relative frequency of each distinct non-null value and N = ∑ nᵢ is the total number of non-null observations.

This is also known as the *average* Shannon entropy, because it equals the [[entropy]] divided by N (the total count):

```
average_entropy(a) = entropy(a) / N
```

For a uniform distribution over k distinct values, `average_entropy(a)` equals `log₂(k)`.

The result is 0 when all observations have the same value (no uncertainty), or when N = 0 (empty partition).

## applies to

- attribute *a* with any scalar [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The domain of [[argument]] *a* and *relation* must match.

## since version

14.4.0

## example

```
parameter<float64> avgEntropyLifeStyleCode                   := average_entropy(City/LifeStyleCode);
// result ≈ 1.459

attribute<float64> avgEntropyLifeStyleCodePerRegion (Region) := average_entropy(City/LifeStyleCode, City/Region_rel);
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

For the total: non-null values are [2, 0, 1, 0, 1, 1], so N = 6, counts: 0→2, 1→3, 2→1.
`average_entropy = -(2/6·log₂(2/6) + 3/6·log₂(3/6) + 1/6·log₂(1/6)) ≈ 1.459`

| **avgEntropyLifeStyleCodePerRegion** |
|-------------------------------------:|
| **0**                                |
| **0**                                |
| **0**                                |
| **0**                                |
| **0**                                |

*domain Region, nr of rows = 5*

Each region has only one unique non-null value (or no non-null data), so average_entropy = 0 for all regions. Region 3 has City 6 with null LifeStyleCode (excluded) and City 4 with LifeStyleCode=1 (only one unique value → average_entropy 0). Region 4 has no cities at all, so N=0 and average_entropy = 0.

## see also

- [[entropy]] - the total Shannon entropy (N · H), i.e. the sum of individual information contributions
- [[modus]] - the most frequently occurring value
- [[unique_count]] - number of distinct non-null values
