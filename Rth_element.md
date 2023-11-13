*[[Aggregation functions]] rth_element*

## syntax

- rth_element(*a*,*r*)
- rth_element(*a*, *r*, *relation*)

## definition

- rth_element(*a*, *r*) results in a [[parameter|parameter]] with the **r \* (n-1)th element** of the ascending sorted [[data item]] *a* (n = number of elements).
- rth_element(*a*, *r*, *relation*) results in an [[attribute]] with the **r \* (n-1)th elements** of the ascending sorted data item a, grouped by [[relation]] (n = number of elements within a partition). The [[domain unit]] of the result is the [[values unit]] of the relation. The values unit of the result should match the values unit of attribute *a*.

## description

- If r\*(n-1) is not an integer, a weighted mean is calculated from the values that relate to the two nearest indices.
- With r = 0.5, the function results in the [[median]] of data item *a*.

## applies to

- attribute *a* with Numeric, uint2, uint4 or bool [[value type]]
- data item *r* with float32 value type
- *relation* with value type of the group CanBeDomainUnit

## conditions

1. The values unit of the resulting data item should match with regard to value type and [[metric]] with the values unit of attribute *a*.
2.  The domain of [[arguments|argument]] *a* and *relation* must match
3.  The domain of argument *r* must be the same as the result domain or *r* must be a parameter.

## since version

5.61

## example

<pre>
1. parameter&lt;uint32&gt;  rth_elementNrInh := <B>rth_element(</B>City/NrInhabitants, 0.5f<B>)</B>; result = 400
2. attribute&lt;float32&gt; rth_elementNrInhRegion (Region) := 
    <B>rth_element(</B>
       City/NrInhabitants
      ,0.5f
      ,City/Region_rel
   <B>)</B>;
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

| **rth_elementNrInhRegion** |
|---------------------------:|
| **550**                    |
| **512**                    |
| **300**                    |
| **200**                    |
| **null**                   |

*domain Region, nr of rows = 5*

## **see also**

- [[nth_element]]
- [[nth_element_weighted]]