*[[Aggregation functions]] min_index*

## syntax

- min_index(*a*)
- min_index(*a*, *relation*)

## definition

- min_index(*a*) results in a [[parameter|parameter]] with the [[index number|index numbers]] of the **minimum** value of the non [[null]] values of [[attribute]] *a*.
- min_index(*a*, *relation*) results in an [[attribute]] with the index numbers of the **minimum** values of the non null values of attribute *a*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation* attribute.

## description

The min_index function is not defined for string [[data items|data item]].

## applies to
- attribute *a* with Numeric, Point or boolean [[value type]]
- *relation* with [[value type]] of the group CanBeDomainUnit

## conditions

The [[values unit]] of the resulting data item should be the domain unit of [[argument]] *a*.

## since version

7.184

## example

<pre>
parameter&lt;City&gt; min_index_NrInh                := <B>min_index(</B>City/NrInhabitants<B>)</B>; result = 5
attribute&lt;City&gt; min_index_NrInhRegion (Region) := <B>min_index(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
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

| **min_index_NrInhRegion** |
|--------------------------:|
| **0**                     |
| **3**                     |
| **2**                     |
| **4**                     |
| **null**                  |

*domain Region, nr of rows = 5*

## see also

- [[max_index]]
- [[min]]