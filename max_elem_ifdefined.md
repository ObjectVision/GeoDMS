*[[Ordering functions]] maximum element ifdefined*

## syntax

- max_elem_ifdefined(*a*, *b*, .. , *n*)

## definition

max_elem_ifdefined(*a*, *b*, .. , *n*) is a variant of the [[max_elem]] function resulting in defined values if **any value** of the [[arguments|argument]] for an entry in the [[domain unit]] is defined. If all arguments contains [[null]] values, the resulting value for that entry will be become null.

## since version
8.6.5

## example

<pre>
attribute&lt;uint32&gt; max_elem_ifdefinedABC (MDomain) := <B>max_elem_ifdefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32)|B(int32)|C(int32)|max_elem_ifdefinedABC|
|-------:|-------:|-------:|---------------------:|
|0       |1       |2       |**2**                 |
|1       |-1      |4       |**4**                 |
|-2      |2       |2       |**2**                 |
|4       |0       |7       |**7**                 |
|999     |111     |-5      |**999**               |
|2       |null    |1       |**2**                 |
|0       |1       |null    |**1**                 |
|null    |1       |2       |**2**                 |
|null    |null    |null    |**null**              |

*MDomain, nr of rows = 9*

## see also

- [[max_elem_alldefined]]
- [[max_elem]]
- [[argmax]]
