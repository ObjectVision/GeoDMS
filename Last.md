*[[Aggregation functions]] last*

## syntax

- last(*a*)
- last(*a*, *relation*)

## definition

- last(*a*) results in a [[parameter]] with the **last** of the non [[null]] values of [[attribute]] *a*.
- last(*a*, *relation*) results in an attribute with the **last** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

## applies to

1.  attribute *a* with Numeric, Point, string or bool [[value type]]
2.  relation with value type of the group CanBeDomainUnit

## conditions

1.  The values unit of the resulting [[data item]] should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2.  The domain of [[argument]] *a* and *relation* must match.

## example

<pre>
parameter&lt;uint32&gt; lastNrInh    := <B>last(</B>City/NrInhabitants<B>)</B>;  result = 175
parameter&lt;string&gt; lastCityName := <B>last(</B>City/CityName<B>)</B>;       result = ‘Haarlem’
parameter&lt;bool&gt;   lastIsCap    := <B>last(</B>City/IsCapital<B>)</B>;      result = False

attribute&lt;uint32&gt; lastNrInhRegion    (Region):= <B>last(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
attribute&lt;string&gt; lastCityNameRegion (Region):= <B>last(</B>City/CityName,      City/Region_rel<B>)</B>;
attribute&lt;bool&gt;   lastIsCapital      (Region):= <B>last(</B>City/IsCapital,     City/Region_rel<B>)</B>;
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


| **lastNrInhRegion** | **lastCityNameRegion** | **lastIsCapitalRegion** |
|--------------------:|------------------------|-------------------------|
| **550**             | **Amsterdam**          | **True**                |
| **500**             | **DenHaag**            | **False**               |
| **300**             | **Utrecht**            | **False**               |
| **200**             | **Eindhoven**          | **False**               |
| **null**            | **null**               | **False**               |

*domain Region, nr of rows = 5*

## see also
- [[first]]