*[[Geometric functions]] points2sequence*

[[images/Points2Sequence_function.png]]

## syntax

- points2sequence(*point*)
- points2sequence(*point*, *SequenceNr*)
- points2sequence(*point*, *SequenceNr*, *ordinal*)
- points2sequence_p(*point*)
- points2sequence_ps(*point*, *SequenceNr*)
- points2sequence_pso(*point*, *SequenceNr*, *ordinal*)
- points2sequence_po(*point*, *ordinal*)

<!-- -->

- points2polygon(*point*)
- points2polygon(*point*, *SequenceNr*)
- points2polygon(*point*, *SequenceNr*, *ordinal*)
- points2polygon_p(*point*)
- points2polygon_ps(*point*, *SequenceNr*)
- points2polygon_pso(*point*, *SequenceNr*, *ordinal*)
- points2polygon_po(*point*, *ordinal*)

## definition

- points2sequence(*point*, *SequenceNr*, *ordinal*) and its variants result in a [[point]] [[attribute]] with [[composition type|composition]] arc or polygon.
- points2polygon(*point*, *SequenceNr*, *ordinal*) and its variants result in a new point attribute with composition type polygon:

Both function groups are used **to make [[arcs|arc]] / [[polygons|polygon]]**, based on a [[data item]] with points, a [[relation]] from the point towards the [[domain unit]] of the resulting arcs/polygons and a data item indicating the order of the points in the resulting arcs/polygons.

They both have the following [[arguments|argument]]:

- *point* = a data item with the points for the resulting arcs/polygons.
- *SequenceNr* = an optional relation towards the domain unit of the resulting arcs/polygons. If no *SequenceNr* is configured, the resulting data item with arc/polygons is a parameter.
- *ordinal* = an optional data item with the order of each point in the resulting arcs/polygons. If no ordinal is specified, the configuration order is used.

## applies to

- data item *point* with a point value type
- data item *SequenceNr* with as [[values unit]] the domain unit of the resulting arcs/polygons.
- data item *ordinal* with uint32 value type

## conditions

The domain units of all arguments must match.

## since version

5.15; variants available since 7.033

## example

<pre>
attribute&lt;fpoint&gt; geometry (road, arc) := <B>points2sequence(</B>point, SequenceNr, ordinal<B>)</B>;
</pre>

| point            | SequenceNr | Ordinal |
|------------------|-----------:|--------:|
| {399246, 112631} | 0          | 0       |
| {398599, 111866} | 0          | 1       |
| {398599, 111866} | 1          | 0       |
| {399495, 111924} | 1          | 1       |
| {401801, 111524} | 1          | 2       |
| {401529, 114921} | 2          | 0       |
| {398584, 114823} | 2          | 1       |

*domain RoadPointSet, nr of rows = 7*

| **geometry**                                         |
|----------------------------------------------------------|
| **{2 {399246, 112631}{398599, 111866}}**                 |
| **{3 {398599, 111866}{399495, 111924} {401801,111524}}** |
| **{2 {401529, 114921}{398584, 114823}}**                 |

*domain Road, nr of rows = 3*

## see also

- [[sequence2points]]