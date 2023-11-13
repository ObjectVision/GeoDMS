*[[Conversion functions]] upoint*

## concept

1. upoint is a Point(Group) [[value type]] with two coordinates of the 32 bits (4 bytes) unsigned integer value type: [[uint32]].
2. upoint() is a function converting other point [[data items|data item]] or [[units|unit]] to the upoint value type.

This page describes the upoint() function.

## syntax

- upoint(*a*)

## definition

upoint(*a*) converts the coordinates of a point [[item|tree item]] *a* to the **upoint (uint32 coordinates)** [[value type]].

## applies to

- data item or unit with PointGroup value type

## example
<pre>
attribute&lt;upoint&gt; upointA (ADomain) := <B>upoint(</B>A<B>)</B>;
</pre>

| A(*fpoint*)        | **upointA**            |
|-------------------:|-----------------------:|
| {0,0}              | **{0,0}**              |
| {1,1}              | **{1,1}**              |
| {1000000,10000000} | **{1000000,10000000}** |
| {-2.5, 2.5}        | **null**               |
| {99.9, 99.9}       | **{99,99}**            |

*ADomain, nr of rows = 5*