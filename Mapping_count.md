*[[Aggregation functions]] mapping_count*

## syntax

- mapping_count(*orgGridUnit*, *destGridUnit*, *[[value type]]*)

## definition

- mapping_count(*orgGridUnit*, *destGridUnit*, *value type*) is defined as the **count of the number of cells** of the *orgGridUnit* [[domain unit]] in the *destGridUnit* domain, expressed in the *[[value type]]* [[argument]].

## description

The mapping_count(*orgGridUnit*, *destGridUnit*, *uint32*) function is synonym for *[[pcount]] (mapping(orgGridUnit,destGridUnit))*.

Use the mapping_count function if the number of cells in the orgGridUnit is very large (the mapping function would than also result in a large [[data item]]).

The explicit configuration of the *value type* argument is also useful to limit the size of the resulting data item.

## applies to

Domain units *orgGridUnit* and *destGridUnit* with a Point value type of the group CanBeDomainUnit.

## since version

7.160

## example

<pre>
attribute&lt;uint16&gt; countNr15Cells (GridDomain3000) := <B>mapping_count(</B>GridDomain15, GridDomain3000, uint16<B>)</B>;
</pre>

| countNr15Cells |
|---------------:|
| 40000          |
| 40000          |
| 40000          |
| 40000          |
| 40000          |
| 40000          |
| 40000          |
| 40000          |
| 40000          |

*domain GridDomain3000, nr of rows = 9*

## see also

- [[mapping]]