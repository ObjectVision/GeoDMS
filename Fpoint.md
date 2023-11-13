*[[Conversion functions]] fpoint*

## concept

1. fpoint is a Point(Group) [[value type]] with two coordinates of the 32 bits (4 bytes) floating point value type: [[float32]].
2. fpoint() is a function converting other point [[data items|data item]] or [[units|unit]] to the fpoint value type.

This page describes the fpoint() function.

## syntax

- fpoint(*a*)

## definition

fpoint(*a*) converts the coordinates of a point [[item|tree item]] *a* to the **fpoint (float32 coordinates)** value type.

## applies to

- data item or unit with PointGroup value type

## example
<pre>
attribute&lt;fpoint&gt; fpointA (ADomain) := <B>fpoint(</B>A<B>)</B>;
</pre>

| A(*fpoint*)        |**fpointA**             |
|-------------------:|-----------------------:|
| {0,0}              | **{0,0}**              |
| {1,1}              | **{1,1}**              |
| {1000000,10000000} | **{1000000,10000000}** |
| {-2.5, 2.5}        | **{-2.5, 2.5}**        |
| {99.9, 99.9}       | **{99.9,99.9}**        |

*ADomain, nr of rows = 5*