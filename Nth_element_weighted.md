*[[Aggregation functions]] nth_element_weighted*

## syntax

- nth_element_weighted(*rank*, *targetweight*, *weight*)
- nth_element_weighted(*rank*, *targetweight*, *weight*, *relation*)

## definition

- nth_element_weighted(*rank*, *targetweight*, *weight*) results in a [[parameter|parameter]] with a **rank value**. To calculate this rank value, first the sort order of the *weight* [[argument]] is determined by sorting the *rank* [[attribute]] ascending. The *weight* values are now cumulated according to this sort order until the value of the parameter with the *targetweight* is reached. The rank value for this element is the result of the nth_element_weighted function. 
- nth_element_weighted(*rank*, *targetweight*, *weight*, *relation*) results in an [[attribute]] with **rank values**, calculated as described above, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the values unit of the relation.

## applies to

- attribute *rank* with Numeric [[value type]] [[data item]] 
- attribute *targetweight* with float32 or float64 value type
- attribute *weight* with float32 or float64 value type
- *relation* with value type of the group CanBeDomainUnit

## conditions

- The [[values unit]] of the [[arguments|argument]] *targetweight* and *weight* must match and only contain positive values.
- The domain unit of the arguments *rank*, *weight* and *relation* must match. 
- In variant 2 the domain of the *targetweight* argument must match with the domain of the [[values unit]] of the *relation* ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or parameters can be used as data items of any domain).

## since version

5.95

## example

<pre>
1. parameter&lt;uint32&gt; nth_element_weighted_NrInh :=
   <B>nth_element_weighted(</B>
       City/Ranking
      ,1800f
      ,float32(City/nrInhabitants)
   <B>)</B>; result = 9

2. attribute&lt;float32&gt; nth_element_weighted_NrInhRegion (Region) := 
   <B>nth_element_weighted(</B>
       City/Ranking
      ,400f
      ,float32(City/nrInhabitants)
      ,City/Region_rel
    <B>)</B>;
</pre>

| City/Ranking | City/NrInhabitants | City/Region_rel |
|-------------:|-------------------:|----------------:|
| 1            | 550                | 0               |
| 3            | 525                | 1               |
| 6            | 300                | 2               |
| 9            | 500                | 1               |
| 10           | 200                | 3               |
| 21           | 175                | null            |
| 50           | null               | 3               |

*domain City, nr of rows = 7*

|**nth_element_weighted_NrInhRegion** |
|------------------------------------:|
|**1**                                |
|**3**                                |
|**null (6 in versions before 7.022)**|
|**null**                             |
|**null**                             |

*domain Region, nr of rows = 5*

## see also

- [[nth_element]]
- [[rth_element]]