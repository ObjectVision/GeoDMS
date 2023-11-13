*[[Conversion functions]] dpoint*

## concept

1. dpoint is a Point(Group) [[value type]] with two coordinates of the 64 bits (8 bytes) floating point value type: [[float64]].
2. dpoint() is a function converting other point [[data items|data item]] or [[unit|units]] to the dpoint value type.

This page describes the dpoint() function.

## syntax

- dpoint(*a*)

## definition

dpoint(*a*) converts the coordinates of a point [[item|tree item]] *a* to the **dpoint (float64 coordinates)** value type.

## applies to

- data item or unit with PointGroup value type

## example

<pre>
attribute&lt;dpoint&gt; dpointA (ADomain) := <B>dpoint(</B>A<B>)</B>;
</pre>

| A(*dpoint*)        | **dpointA**            |
|-------------------:|-----------------------:|
| {0,0}              | **{0,0}**              |
| {1,1}              | **{1,1}**              |
| {1000000,10000000} | **{1000000,10000000}** |
| {-2.5, 2.5}        | **{-2, 2}**            |
| {99.9, 99.9}       | **{99,99}**            |

*ADomain, nr of rows = 5*