*[[Ordering functions]] argmin alldefined*

## syntax

- argmin_alldefined(*a*, *b*, .. , *n*)

## definition

argmin_alldefined(*a*, *b*, .. , *n*) is a variant of the [[argmin]] function resulting in defined values if **all values** of the [[arguments|argument]] 
 for an entry in the [[domain unit]] are defined. If any argument contains [[null]] values, the resulting value for that entry will be null.

## since version
8.6.5

## example
<pre>
attribute&lt;uint32&gt; argmin_alldefinedABC (DomDomain) := <B>argmin_alldefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argmin_alldefinedABC|
|-------------------------:|-------------------------:|-------------------------:|-------------------:|
|0                         |1                         |2                         |**0**               |
|1                         |-1                        |4                         |**1**               |
|-2                        |2                         |2                         |**0**               |
|4                         |0                         |7                         |**1**               |
|999                       |111                       |-5                        |**2**               |
|2                         |null                      |1                         |**null**            |
|0                         |1                         |null                      |**null**            |
|null                      |1                         |2                         |**null**            |
|null                      |null                      |null                      |**null**            |
|1                         |1                         |1                         |**0**               |

*DomDomain, nr of rows = 10*

## see also

- argmin_alldefined_uint16, a version of the argmin_alldefined function resulting in a uint16 data item
- argmin_alldefined_uint8, a version of the argmin_alldefined function resulting in a uint8 data item
- [[argmin_ifdefined]]
- [[argmin]] 
