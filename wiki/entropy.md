*[[Aggregation functions]] entropy*

## syntax

- entropy(*a*)
- entropy(*a*, *relation*)

## definition

- entropy(*a*) results in a [[parameter]] with the **total Shannon entropy** (in bits) of the non-[[null]] values of [[attribute]] *a*.
- entropy(*a*, *relation*) results in an attribute with the total Shannon entropy (in bits) of the non-null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## description

The total Shannon entropy of a set of N observations is defined as:

```
entropy(a) = N · H(a)
           = -∑ nᵢ · log₂(nᵢ / N)
```

where nᵢ is the count of each distinct non-null value and N = ∑ nᵢ is the total number of non-null observations.

This equals N times the average (per-element) Shannon entropy H(a). See [[average_entropy]] for the average Shannon entropy H(a).

For a uniform distribution over k distinct values, `entropy(a)` equals `N · log₂(k)`.

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
parameter<float64> entropyLifeStyleCode                      := entropy(City/LifeStyleCode);
// result ≈ 8.757

attribute<float64> entropyLifeStyleCodePerRegion (Region) := entropy(City/LifeStyleCode, City/Region_rel);
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
`entropy = -(2·log₂(2/6) + 3·log₂(3/6) + 1·log₂(1/6)) ≈ 8.757`

| **entropyLifeStyleCodePerRegion** |
|----------------------------------:|
| **0**                             |
| **0**                             |
| **0**                             |
| **0**                             |
| **0**                             |

*domain Region, nr of rows = 5*

Each region has only one unique non-null value (or no non-null data), so entropy = 0 for all regions. Region 3 has City 6 with null LifeStyleCode (excluded) and City 4 with LifeStyleCode=1 (only one unique value → entropy 0). Region 4 has no cities at all, so N=0 and entropy = 0.

## see also

- [[average_entropy]] - the Shannon entropy per element (H = entropy / N), i.e. the standard Shannon entropy formula
- [[modus]] - the most frequently occurring value
- [[unique_count]] - number of distinct non-null values
