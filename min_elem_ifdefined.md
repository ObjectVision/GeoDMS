*[[Ordering functions]] minimum element ifdefined*

## syntax

- min_elem_ifdefined(*a*, *b*, .. , *n*)

## definition

min_elem_ifdefined(*a*, *b*, .. , *n*) is a variant of the [[min_elem]] function resulting in defined values if **any value** of the [[arguments|argument]] for an entry in the [[domain unit]] is defined. If all arguments contains [[null]] values, the resulting value for that entry will be become null.

## since version
8.6.5

## example

<pre>
attribute&lt;uint32&gt; min_elem_ifdefinedABC (MDomain) := <B>min_elem_ifdefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32)|B(int32)|C(int32)|min_elem_ifdefinedABC|
|-------:|-------:|-------:|---------------------:|
|0       |1       |2       |**0**                 |
|1       |-1      |4       |**-1**                |
|-2      |2       |2       |**-2**                |
|4       |0       |7       |**0**                 |
|999     |111     |-5      |**-5**                |
|2       |null    |1       |**1**                 |
|0       |1       |null    |**0**                 |
|null    |1       |2       |**1**                 |
|null    |null    |null    |**null**              |

*MDomain, nr of rows = 9*

## see also

- [[min_elem_alldefined]]
- [[min_elem]]
- [[argmin]]
