*[[Aggregation functions]] correlation*

## syntax

- corr(*a*, *b*)
- corr(*a*, *b*, *relation*)

## definition

- corr(*a*, *b*) results in the **correlation** coefficient of [[attributes|attribute]] *a* and *b*.
- corr(*a*, *b*, *relation*) results in the **correlation** coefficient of attributes *a* and *b*, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the relation.

The [[value type]] of the resulting [[data item]] is float32 or float64.

## applies to

- attribute *a* and *b* with Numeric [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The value type of the [[arguments|argument]] *a* and *b* must match.
2. The domain of arguments *a*, *b* and *relation* must match.

## example

<pre>
1. parameter&lt;float32&gt; corrNrInhabitantsTemp                := 
    <B>corr(</B>float32(City/NrInhabitants), City/avgDailyTemperature<B>)</B>; result = -0.89948
2. attribute&lt;float32&gt; corrNrInhabitantsTempRegion (Region) :=
    <B>corr(</B>
       float32(City/NrInhabitants)
      ,City/avgDailyTemperature
      ,City/RegionNr
   <B>)</B>;
</pre>

| City/NrInhabitants | City/avgDailyTemperature | City/Region_rel |
|-------------------:|-------------------------:|----------------:|
| 550                | 12                       | 0               |
| 525                | 11                       | 1               |
| 300                | null                     | 2               |
| 500                | 11                       | 1               |
| 200                | 14                       | 3               |
| 175                | null                     | null            |
| null               | 14                       | 3               |

*domain City, nr of rows = 7*

| **corrNrInhabitantsTempRegion** |
|--------------------------------:|
| **null**                        |
| **null**                        |
| **null**                        |
| **null**                        |
| **null**                        |

*domain Region, nr of rows = 5*