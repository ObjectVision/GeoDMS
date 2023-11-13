*[[Ordering functions]] argmin*

## syntax

- argmin(*a*, *b*, .. , *n*)

## definition

argmin(*a*, *b*, .. , *n*) results in a uint32 [[data item]] with the order number of the [[argument]] with the **lowest value** for the element-by-element comparison.

The [[domain unit]] of the resulting item is the same as the domain units of all arguments of the function.

The [[values unit]] of the resulting item is [[value type]] of the domain unit of of all arguments of the function.

If the lowest value occurs more than once, the function results the first order number.

[[null]] is considered higher than any other value.

## applies to

Data items with Numeric or string value type

## conditions

1. Domain of the arguments must match or be void.
2. Arguments must have matching:
    - [[value type]]
    - [[metric]]

## example

<pre>
attribute&lt;uint32&gt; argminABC (DomDomain) := <B>argmin(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argminABC|
|-------------------------:|-------------------------:|-------------------------:|--------:|
|0                         |1                         |2                         |**0**    |
|1                         |-1                        |4                         |**1**    |
|-2                        |2                         |2                         |**0**    |
|4                         |0                         |7                         |**1**    |
|999                       |111                       |-5                        |**2**    |
|2                         |null                      |1                         |**2**    |
|0                         |1                         |null                      |**0**    |
|null                      |1                         |2                         |**1**    |
|null                      |null                      |null                      |**0**    |
|1                         |1                         1                          |**0**    |

*DomDomain, nr of rows = 10*

*In earlier versions (before 7.202) a null value in one of the arguments could result in a null value of the resulting data item*

## see also

- argmin_uint16, a version of the argmin function resulting in a uint16 data item
- argmin_uint8, a version of the argmin function resulting in a uint8 data item
- [[argmin_alldefined]]
- [[argmin_ifdefined]]
- [[argmax]]
- [[minimum element|min_elem]]