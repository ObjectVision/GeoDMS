*[[Ordering functions]] minimum element*

## syntax

- min_elem(*a*, *b*, .. , *n*)

## definition

min_elem(*a*, *b*, .. , *n*) results in a [[data item]] with the **lowest value** of the [[arguments|argument]] in the element-by-element comparison.

The [[domain unit]] of the resulting item is the same as the domain units of all arguments of the function.

The [[values unit]] of the resulting item is the values unit of the of all arguments of the function.

## applies to

Data items with Numeric or string value type

## conditions

1. Domain of the arguments must match or be void.
2. Arguments must have matching:
    - [[value type]]
    - [[metric]]

## example

<pre>
attribute&lt;uint32&gt; min_elemABC (MDomain) := <B>min_elem(</B>A, B, C<B>)</B>;
</pre>

|A(int32)|B(int32)|C(int32)|min_elemABC|
|-------:|-------:|-------:|----------:|
|0       |1       |2       |**0**      |
|1       |-1      |4       |**-1**     |
|-2      |2       |2       |**-2**     |
|4       |0       |7       |**0**      |
|999     |111     |-5      |**-5**     |
|2       |null    |1       |**0**      |
|0       |1       |null    |**0**      |
|null    |1       |2       |**0**      |
|null    |null    |null    |**null**   |

*MDomain, nr of rows = 9*

*In earlier versions (before 7.202) a null value in one of the arguments could result in a null value of the resulting data item. This now only occurs if all arguments have null values.*

## see also

- [[min_elem_alldefined]]
- [[min_elem_ifdefined]]
- [[argmin]]
- [[max_elem]]