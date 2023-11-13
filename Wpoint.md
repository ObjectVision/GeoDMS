*[[Conversion functions]] wpoint*

## concept

1. wpoint is a Point(Group) [[value type]] with two coordinates of the 16 bits (2 bytes) unsigned integer value type: [[uint16]].
2. wpoint() is a function converting other point [[data items|data item]] or [[units|unit]] to the wpoint value type.

This page describes the wpoint() function.

## syntax

- wpoint(*a*)

## definition

wpoint(*a*) converts the coordinates of a point [[item|tree item]] *a* to the **wpoint (uint16 coordinates)** value type.

## applies to

-   data item or unit with PointGroup value type

## example
<pre>
attribute&lt;wpoint&gt; wpointA (ADomain) := <B>wpoint(</B>A<B>)</B>;
</pre>

| A(*fpoint*)        |**wpointA**  | 
|-------------------:|------------:|
| {0,0}              | **{0,0}**   |
| {1,1}              | **{1,1}**   |
| {1000000,10000000} | **null**    |
| {-2.5, 2.5}        | **null**    |
| {99.9, 99.9}       | **{99,99}** |

*ADomain, nr of rows = 5*