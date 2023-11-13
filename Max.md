*[[Aggregation functions]] max(imum)*

## syntax

- max(*a*)
- max(*a*, *relation*)

## definition

- max(*a*) results in a [[parameter]] with the **maximum** of the non [[null]] values of [[attribute]] *a*.
- max(*a*, *relation*) results in an attribute with the **maximum** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## description

The max function is not defined for boolean [[data items|data item]], use the [[all]] function instead.

If there are no values to be aggregated for a group, the resulting value will be the minimum value for the [[value type]] of attribute *a* (in the example: 0, the minimum value for the value type: uint32).

## applies to

- attribute *a* with Numeric, Point or string value type
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The values unit of the resulting data item should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2. The domain of [[argument]] *a* and *relation* must match.

## example
<pre>
parameter&lt;uint32&gt; maxNrInh := <B>max(</B>City/NrInhabitants<B>)</B>; result = 550
parameter&lt;uint32&gt; maxCity  := <B>max(</B>City/CityName<B>)</B>     ; result = ‘Utrecht’

attribute&lt;uint32&gt; maxNrInhRegion    (Region) := <B>max(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
attribute&lt;uint32&gt; maxCityNameRegion (Region) := <B>max(</B>City/CityName, City/Region_rel<B>)</B>;
</pre>

| City/NrInhabitants | City/CityName | City/Region_rel |
|-------------------:|---------------|----------------:|
| 550                | Amsterdam     | 0               |
| 525                | Rotterdam     | 1               |
| 300                | Utrecht       | 2               |
| 500                | DenHaag       | 1               |
| 200                | Eindhoven     | 3               |
| 175                | Haarlem       | null            |
| null               | null          | 3               |

*domain City, nr of rows = 7*

| **maxNrInhRegion** | **maxCityNameRegion** |
|-------------------:|-----------------------|
| **550**            | **Amsterdam**         |
| **525**            | **Rotterdam**         |
| **300**            | **Utrecht**           |
| **200**            | **Eindhoven**         |
| **0**              |                       |

*domain Region, nr of rows = 5*

## see also

-   [[min]]
-   [[max_index]]