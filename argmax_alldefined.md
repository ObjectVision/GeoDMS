*[[Ordering functions]] argmax alldefined*

## syntax

- argmax_alldefined(*a*, *b*, .. , *n*)

## definition

argmax_alldefined(*a*, *b*, .. , *n*) is a variant of the [[argmax]] function resulting in defined values if **all values** of the [[arguments|argument]] 
 for an entry in the [[domain unit]] are defined. If any argument contains [[null]] values, the resulting value for that entry will be null.

## since version
8.6.5

## example
<pre>
attribute&lt;uint32&gt; argmax_alldefinedABC (DomDomain) := <B>argmax_alldefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argmax_alldefinedABC|
|-------------------------:|-------------------------:|-------------------------:|-------------------:|
|0                         |1                         |2                         |**2**               |
|1                         |-1                        |4                         |**2**               |
|-2                        |2                         |2                         |**1**               |
|4                         |0                         |7                         |**2**               |
|999                       |111                       |-5                        |**0**               |
|2                         |null                      |1                         |**null**            |
|0                         |1                         |null                      |**null**            |
|null                      |1                         |2                         |**null**            |
|null                      |null                      |null                      |**null**            |
|1                         |1                         |1                         |**0**               |

*DomDomain, nr of rows = 10*

## see also

- argmax_alldefined_uint16, a version of the argmax_alldefined function resulting in a uint16 data item
- argmax_alldefined_uint8, a version of the argmax_alldefined function resulting in a uint8 data item
- [[argmax_ifdefined]]
- [[argmax]] 
