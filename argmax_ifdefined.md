*[[Ordering functions]] argmax ifdefined*

## syntax

- argmax_ifdefined(*a*, *b*, .. , *n*)

## definition

argmax_ifdefined(*a*, *b*, .. , *n*) is a variant of the [[argmax]] function resulting in defined values if **any value** of the [[arguments|argument]] 
 for an entry in the [[domain unit]] is defined. If all arguments contain [[null]] values, the resulting value for that entry will be null. If all arguments contain the same value, e.g. A=0, B=0, C=0, then the result will be the first argument.

## since version
8.6.5

## example
<pre>
attribute&lt;uint32&gt; argmax_ifdefinedABC (DomDomain) := <B>argmax_ifdefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argmax_ifdefinedABC|
|-------------------------:|-------------------------:|-------------------------:|------------------:|
|0                         |1                         |2                         |**2**              |
|1                         |-1                        |4                         |**2**              |
|-2                        |2                         |2                         |**1**              |
|4                         |0                         |7                         |**2**              |
|999                       |111                       |-5                        |**0**              |
|2                         |null                      |1                         |**0**              |
|0                         |1                         |null                      |**1**              |
|null                      |1                         |2                         |**2**              |
|null                      |null                      |null                      |**null**           |
|1                         |1                         |1                         |**0**              |

*DomDomain, nr of rows = 10*

## see also

- argmax_ifdefined_uint16, a version of the argmax_ifdefined function resulting in a uint16 data item
- argmax_ifdefined_uint8, a version of the argmax_ifdefined function resulting in a uint8 data item
- [[argmax_alldefined]]
- [[argmax]]
