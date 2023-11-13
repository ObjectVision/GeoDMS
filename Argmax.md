*[[Ordering functions]] argmax*

## syntax

- argmax(*a*, *b*, .. , *n*)

## definition

argmax(*a*, *b*, .. , *n*) results in a uint32 [[data item]] with the order number of the [[argument]] with the **highest value** for the element-by-element comparison.

The [[domain unit]] of the resulting item is the same as the domain units of all arguments of the function.

The [[values unit]] of the resulting item is [[value type]] of the domain unit of all arguments of the function. But is by default a uint32. For other value types use for example *argmax_uint8(a, b, .. , n).*

If the highest value occurs more than once, the function results the first order number.

[[null]] is considered lower than any other value.

## applies to

Data items with Numeric or string value type

## conditions

1.  Domain of the arguments must match or be void.
2.  Arguments must have matching:
    -   [[value type]]
    -   [[metric]]

## example
<pre>
attribute&lt;uint32&gt; argmaxABC (DomDomain) := <B>argmax(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argmaxABC|
|-------------------------:|-------------------------:|-------------------------:|--------:|
|0                         |1                         |2                         |**2**    |
|1                         |-1                        |4                         |**2**    |
|-2                        |2                         |2                         |**1**    |
|4                         |0                         |7                         |**2**    |
|999                       |111                       |-5                        |**0**    |
|2                         |null                      |1                         |**0**    |
|0                         |1                         |null                      |**1**    |
|null                      |1                         |2                         |**2**    |
|null                      |null                      |null                      |**0**    |
|1                         |1                         |1                         |**0**    |

*DomDomain, nr of rows = 10*

*In earlier versions (before 7.202) a null value in one of the arguments could result in a null value of the resulting data item.*

## see also

- argmax_uint16, a version of the argmax function resulting in a uint16 data item
- argmax_uint8, a version of the argmax function resulting in a uint8 data item
- [[argmax_alldefined]]
- [[argmax_ifdefined]]
- [[argmin|argmin]] 
- [[maximum element|max_elem]]