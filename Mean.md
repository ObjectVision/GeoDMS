*[[Aggregation functions]] mean*

## syntax

- mean(*a*)
- mean(*a*, *relation*)

## definition

- mean(*a*) results in a [[parameter|parameter]] with the **average** of the non [[null]] values of [[attribute]] *a*.
- mean(*a*, relation) results in a an [[attribute]] with the **average** of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## applies to

- attribute *a* with Numeric [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1.  The values unit of the resulting [[data item]] should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2.  The domain unit of [[argument]] *a* and *relation* must match.

## example

<pre>
parameter&lt;uint32&gt; meanNrInh                := <B>mean(</B>City/NrInhabitants<B>)</B>; result = 375
attribute&lt;uint32&gt; meanNrInhRegion (Region) := <B>mean(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
</pre>

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

| **meanNrInhRegion** |
|---------------------|
| **550**             |
| **512**             |
| **300**             |
| **200**             |
| **null**            |

*domain Region, nr of rows = 5*