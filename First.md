*[[Aggregation functions]] first*

## syntax

- first(*a*)
- first(*a*, *relation*)

## definition

- first(*a*) results in a [[parameter]] with the **first** of the non [[null]] values of [[attribute]] *a*.
- first(*a*, *relation*) results in an [[attribute]] with the **first** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## applies to

- attribute *a* with Numeric, Point, string or bool [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1.  The values unit of the resulting data item should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2.  The domain of [[argument]] *a* and *relation* must match.

## example

<pre>
parameter&lt;uint32&gt; firstNrInh    := <B>first(</B>City/NrInhabitants<B>)</B>;  result = 550
parameter&lt;string&gt; firstCityName := <B>first(</B>City/CityName<B>)</B>;       result = ‘Amsterdam’
parameter&lt;bool&gt;   firstIsCap    := <B>first(</B>City/IsCapital<B>)</B>;      result = True

attribute&lt;uint32&gt; firstNrInhRegion    (Region) := <B>first(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
attribute&lt;string&gt; firstCityNameRegion (Region) := <B>first(</B>City/CityName,      City/Region_rel<B>)</B>;
attribute&lt;bool&gt;   firstIsCapital      (Region) := <B>first(</B>City/IsCapital,     City/Region_rel<B>)</B>;
</pre>

| City/NrInhabitants | City/CityName | IsCapital | City/Region_rel |
|-------------------:|---------------|-----------|----------------:|
| 550                | Amsterdam     | True      | 0               |
| 525                | Rotterdam     | False     | 1               |
| 300                | Utrecht       | False     | 2               |
| 500                | DenHaag       | False     | 1               |
| 200                | Eindhoven     | False     | 3               |
| 175                | Haarlem       | False     | null            |
| null               | null          | False     | 3               |

*domain City, nr of rows = 7*

| **firstNrInhRegion** | **firstCityNameRegion** | **firstIsCapitalRegion** |
|---------------------:|-------------------------|--------------------------|
| **550**              | **Amsterdam**           | **True**                 |
| **525**              | **Rotterdam**           | **False**                |
| **300**              | **Utrecht**             | **False**                |
| **200**              | **Eindhoven**           | **False**                |
| **null**             | **null**                | **False**                |

*domain Region, nr of rows = 5*

## see also

- [[last]]