*[[Conversion functions]] ipoint*

## concept

1. ipoint is a Point(Group) [[value type]] with two coordinates of the 32 bits (4 bytes) signed integer value type: [[int32]].
2. ipoint() is a function converting other point [[data items|data item]] or [[units|unit]] to the ipoint value type.

This page describes the ipoint() function.

## syntax

- ipoint(*a*)

## definition

ipoint(*a*) converts the coordinates of a point [[item|tree item]] *a* to the **ipoint (int32 coordinates)** value type.

## applies to

- data item or unit with PointGroup value type

## example

<pre>
attribute&lt;ipoint&gt; ipointA (ADomain) := <B>ipoint(</B>A<B>)</B>;
</pre>

| A(*fpoint*)        | **ipointA**            | 
|-------------------:|-----------------------:|
| {0,0}              | **{0,0}**              |
| {1,1}              | **{1,1}**              |
| {1000000,10000000} | **{1000000,10000000}** |
| {-2.5, 2.5}        | **{-2, 2}**            |
| {99.9, 99.9}       | **{99,99}**            |

*ADomain, nr of rows = 5*