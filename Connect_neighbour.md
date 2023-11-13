*[[Network functions]] connect_neighbour*

## syntax

- connect_neighbour(*point_dataitem*)
- connect_neighbour(*point_dataitem*, *relation*)

## definition

The connect_neighbour(*point_dataitem*) results in [[relation]] for and towards the [[domain unit]] of the *point_dataitem*, referring to the **nearest point** in the *point_dataitem* **not being the point itself.**

The connect_neighbour function(point_geom, partioning) results in relation for and towards the domain unit of the *point_dataitem*, referring to the
nearest point in the *point_dataitem* not being the point itself and being a point within the *relation*.

## description

The connect_neighbour is a special variant of the first variant of the [[connect]] function.

The connect(*point_dataitem*, *point_dataitem*) function results in a relation to the nearest point, usually the same point. The connect_neighbour function results in the relation to the nearest point, <B>not</B> being the same point.

## applies to

- [[data item]] *point_dataitem* with Point [[value type]].

## conditions

The [[domain unit]] of [[arguments|argument]] *point_dataitem* and *relation* must match.

## since version

7.047

## example
<pre>
attribute&lt;origin&gt; origin_rel (origin) := <B>connect_neighbour(</B>origin/geometry<B>)</B>;
</pre>

| origin/geometry  |**origin_rel** |
|------------------|---------------|
| {401331, 115135} | **2**         |
| {399476, 111803} | **4**         |
| {399289, 114903} | **0**         |
| {401729, 111353} | **1**         |
| {398696, 111741} | **1**         |

*domain origin, nr of rows = 5*

## see also

- [[connect]]