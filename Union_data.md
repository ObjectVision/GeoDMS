*[[Relational functions]] union_data*

## syntax

- union_data(*[[domain unit]]*, *a*, *b*, .. *n*)

## definition

union_data(*domain unit*, *a*, *b*, .. *n*) results in a new [[attribute]] with as [[domain unit]] the first [[argument]]: *domainunit*.

The resulting attribute contains **the values of the [[data items|data item]] *a*, *b*, .., *n***.

## description

The first argument *domainunit*, is often made with the [[union_unit]] function.

The union_data function results in an attribute with as [[values unit]] the values units of data items *a*, *b*, ..*n* and as [[domain unit]] the first argument of the function (*domainunit*). In other words, it appends b to a and then c to a and b, etc.

union_data(*newdomain*, *att_old_domain*) can be used to convert attributes from a source to a target domain unit.

## applies to

- [[unit]] *domainunit* with [[value type]] from the group CanBeDomainUnit
- data items *a*, *b*, ... *n* with Numeric, Point, uint2, uint4, bool or string value type

## conditions

- The values unit of data items *a*, *b*, ... *n* must match.

## example

<pre>
unit&lt;uint32&gt; HollandCity := union_unit(NHCity, ZHCity)
{
   attribute&lt;string&gt; name := <B>union_data(</B>., NHCity/name, ZH/City/name<B>)</B>;
}
</pre>

| **HollandCity/name** |
|----------------------|
| **Amsterdam**        |
| **Haarlem**          |
| **Alkmaar**          |
| **Rotterdam**        |
| **DenHaag**          |
| **Leiden**           |
| **Dordrecht**        |
| **Leiden**           |

*domain HollandCity, nr of rows = 8*

| NHCity/name |
|-------------|
| Amsterdam   |
| Haarlem     |
| Alkmaar     |

*domain NHCity, nr of rows = 3*

| ZHCity/name |
|-------------|
| Rotterdam   |
| DenHaag     |
| Leiden      |
| Dordrecht   |
| Leiden      |

*domain ZHCity, nr of rows = 5*

## see also

- [[union_unit]]