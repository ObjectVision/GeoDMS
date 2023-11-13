*[[Aggregation functions]] min(imum)*

## syntax

- min(*a*)
- min(*a*, *relation*)

## definition

- min(*a*) results in a [[parameter]] with the **minimum** of the non [[null]] values of [[attribute]] *a*.
- min(*a*, *relation*) results in an attribute with the **minimum** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## description

The min function is not defined for boolean [[data items|data item]], use the [[any]] function instead.

If there are no values to be aggregated for a group, the resulting value will be the maximum value for the [[value type]] of attribute *a* (in the example: 4294967294, the maximum value for the value type: uint32).

## applies to

- attribute *a* with Numeric, Point or string value type
- *relation* with value ype of the group CanBeDomainUnit

## conditions

1. The values unit of the resulting data item should match with regard to value ype and [[metric]] with the values unit of attribute *a*.
2. The domain of [[argument]] *a* and *relation* must match.

## example

<pre>
parameter&lt;uint32&gt; minNrInh := <B>min(</B>City/NrInhabitants<B>)</B>; result = 175
parameter&lt;string&gt; minCity  := <B>min(</B>City/CityName<B>)</B>;      result = ‘Amsterdam’

attribute&lt;uint32&gt; minNrInhRegion    (Region) := <B>min(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
attribute&lt;string&gt; minCityNameRegion (Region) := <B>min(</B>City/CityName, City/Region_rel<B>)</B>;
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

| **minNrInhRegion** | **minCityNameRegion** |
|-------------------:|-----------------------|
| **550**            | **Amsterdam**         |
| **500**            | **DenHaag**           |
| **300**            | **Utrecht**           |
| **200**            | **Eindhoven**         |
| **4294967294**     | **ÿÿÿÿ**<sup>1</sup>  |

*domain Region, nr of rows = 5*

*1) ÿÿÿÿ represents the max value for strings*

## see also

-   [[max]]
-   [[min_index]]