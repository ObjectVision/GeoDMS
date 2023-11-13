*[[Ordering functions]] median*

## definition

The [median](https://en.wikipedia.org/wiki/Median) of an [[attribute]] in the GeoDMS can be calculated with the [[rth_element]] function, with as second [[argument]] the value 0.5.

The median function in the GeoDMS calculates the middle value for each row of three given attributes.

Thus median(a, b, c) := 

median(a, interval([[min_elem]](b, c), [[max_elem]](b, c))) :=<BR> 
a \< min_elem(b, c)<BR> 
&nbsp;&nbsp;&nbsp;   ? min_elem(b, c)<BR> 
&nbsp;&nbsp;&nbsp;   : max_elem(b, c) \< a <BR>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;      ? max_elem(b, c) <BR>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;      : a<BR>

This can be used to bind a to a given range of allowed values.

## syntax

- median(*a*, *b*, *c*)

## conditions

1. the [[domain|domain unit]] arguments *a*, *b*, and *c* must match or be void.
2. the [[values unit]] of arguments *a*, *b*, and *c* must match.
3. *b* is known to be not larger than *c*.

## since version

since 2008

## example
<pre>
attribute&lt;uint32&gt; medianABC (ADomain) := <B>median(</B>A, B, C<B>)</B>;
</pre>

|A(int32)|B(int32)|C(int32)|medianABC |
|-------:|-------:|-------:|---------:|
|0       |1       |2       |**1**     |
|1       |-1      |4       |**1**     |
|-2      |2       |2       |**2**     |
|4       |0       |7       |**4**     |
|999     |111     |-5      |**111**   |
|null    |0       |0       |**0**     |
|0       |null    |0       |**0**     |
|0       |0       |null    |**0**     |
|null    |0       |null    |**0**     |
|null    |null    |null    |**null**  |

*ADomain, nr of rows = 10*

*In earlier versions (before 7.202) a [[null]] value in one of the arguments could result in a null value of the resulting data item. This now only occurs if all arguments have null values.* *This is a results of the implementation of [[min_elem]] and [[max_elem]] as functions that since then return the minimum defined value of each row.*