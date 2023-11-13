*[[Network functions]] capacitated_connect*

## syntax

- capacitated_connect(*point_dataitem_dest*, *capacity_dest*, *point_dataitem_org*, *value_org*)

## definition

The capacitated_connect function works in a similar way as the first variant of the **[[connect]]** function, only with the extra condition that the nearest point is found in which the value of the *value_org* argument of the origin [[domain unit]] is **less than or equal to the *capacity_dest*** [[argument]] of the destination domain unit.

The result is a [[data item]] for the domain unit of the *point_dataitem_org* with as [[values unit]] the domain unit of the *point_dataitem_org* data item.

## description

The *point_dataitem_dest* argument should contain unique geometries. Use the [[unique]] function to make a domain unit with unique geometries.

## applies to

- data items *point_dataitem_dest* and *point_dataitem_org* with fpoint or dpoint [[value type]]
- data items *capacity_dest* and *value_org* with float64 value type

## conditions

1. The value type of all arguments must match.
2. The domain unit of arguments *point_dataitem_dest* and *capacity_dest* must match.
3. The domain unit of arguments *point_dataitem_org* and *value_org* must match.

## since version

7.159

## example
<pre>
attribute&lt;dest&gt; dest_rel (org) := <B>capacitated_connect(</B>dest/geometry, dest/capacity, org/geometry, org/temp<B>)</B>;
</pre>

| org/geometry     | org/temp | **dest_rel** |
|------------------|---------:|-------------:|
| {401331, 115135} | 60°      | **2**        |
| {399476, 111803} | 60°      | **1**        |
| {399289, 114903} | 60°      | **2**        |
| {401729, 111353} | 60°      | **1**        |
| {398696, 111741} | 60°      | **1**        |

*domain org, nr of rows = 5*

| dest/geometry    | dest/capacity |
|------------------|--------------:|
| {401331, 115131} | 40°           |
| {399138, 112601} | 90°           |
| {398600, 114903} | 60°           |
| {401729, 112156} | 40°           |

*domain dest, nr of rows = 4*
