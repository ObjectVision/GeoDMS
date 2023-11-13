*[[Ordering functions]] minimum element alldefined*

## syntax

- min_elem_alldefined(*a*, *b*, .. , *n*)

## definition

min_elem_alldefined(*a*, *b*, .. , *n*) is a variant of the [[min_elem]] function resulting in defined values if **any value** of the [[arguments|argument]] for an entry in the [[domain unit]] is defined. If all argument contains a [[null]] value, the resulting value for that entry will be become null.

## since version
8.6.5

## example

<pre>
attribute&lt;uint32&gt; min_elem_alldefinedABC (MDomain) := <B>min_elem_alldefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32)|B(int32)|C(int32)|min_elem_alldefinedABC|
|-------:|-------:|-------:|---------------------:|
|0       |1       |2       |**0**                 |
|1       |-1      |4       |**-1**                |
|-2      |2       |2       |**-2**                |
|4       |0       |7       |**0**                 |
|999     |111     |-5      |**-5**                |
|2       |null    |1       |**null**              |
|0       |1       |null    |**null**              |
|null    |1       |2       |**null**              |
|null    |null    |null    |**null**              |

*MDomain, nr of rows = 9*

## see also

- [[min_elem_ifdefined]]
- [[min_elem]]
- [[argmin]]
