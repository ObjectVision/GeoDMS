*[[Arithmetic functions]] cumulate*

## syntax

-   cumulate(*a*)
-   cumulate(*a*, *relation*)

## definition

-   cumulate(*a*) results in an [[attribute]] with the **cumulation** of the non [[null]] values of attribute *a*.
-   cumulate(*a*, *relation*) results in an attribute with the **cumulation** of the non null values of attribute *a*, grouped by the *[[relation]]*. The [[domain unit]] of the resulting attribute is the values unit of the relation attribute.

## description

1.  For domain units with [[index numbers]] starting from 0 (default), *cumulate(const(1, domainunit, uint32)) - 1* is synonym for *[[id]](domain unit)*. It is advised to use the id function in such a case.
2.  There is no *id(domainunit, relation)* function, use *cumulate(const(1, domainunit, uint32), relation) - 1* to calculate index numbers for each entry in the relation domain.

## applies to

-   Attribute a with Numeric [[value type]]
-   [[Relation attribute|relation]] with value type of the group CanBeDomainUnit

## conditions

1.  The [[values unit]] of the resulting [[data item]] should match with regard to value type and [[metric]] with  the values unit of attribute a.
2.  The domain unit of [[arguments|argument]] a and relation must match.

## since version

6.061

## example

<pre>
1. attribute&lt;uint32&gt; cumulateNrInh       (City) := <B>cumulate(</B>City/NrInhabitants<B>)</B>;
2. attribute&lt;uint32&gt; cumulateNrInhRegion (City) := <B>cumulate(</B>City/NrInhabitants, City/Region_rel<B>)</B>;
</pre>

| City/NrInhabitants | City/Region_rel | **cumulateNrInh** | **cumulateNrInhRegion** |
|-------------------:|----------------:|------------------:|------------------------:|
| 550                | 0               | **550**           | **550**                 |
| 525                | 1               | **1075**          | **525**                 |
| 300                | 2               | **1375**          | **300**                 |
| 500                | 1               | **1875**          | **1025**                |
| 200                | 3               | **2075**          | **200**                 |
| 175                | null            | **2250**          | **null**                |
| null               | 3               | **2250**          | **200**                 |

*domain City, nr of rows = 7*