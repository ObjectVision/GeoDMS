*[[Ordering functions]] argmin ifdefined*

## syntax

- argmin_ifdefined(*a*, *b*, .. , *n*)

## definition

argmin_ifdefined(*a*, *b*, .. , *n*) is a variant of the [[argmin]] function resulting in defined values if **any value** of the [[arguments|argument]] 
 for an entry in the [[domain unit]] is defined. If all arguments contain [[null]] values, the resulting value for that entry will be null.

## since version
8.6.5

## example
<pre>
attribute&lt;uint32&gt; argmin_ifdefinedABC (DomDomain) := <B>argmin_ifdefined(</B>A, B, C<B>)</B>;
</pre>

|A(int32),<BR>sequencenr: 0|B(int32),<BR>sequencenr: 1|C(int32),<BR>sequencenr: 2|argmin_ifdefinedABC|
|-------------------------:|-------------------------:|-------------------------:|-------------------:|
|0                         |1                         |2                         |**0**               |
|1                         |-1                        |4                         |**1**               |
|-2                        |2                         |2                         |**0**               |
|4                         |0                         |7                         |**1**               |
|999                       |111                       |-5                        |**2**               |
|2                         |null                      |1                         |**2**               |
|0                         |1                         |null                      |**0**               |
|null                      |1                         |2                         |**1**               |
|null                      |null                      |null                      |**null**            |
|1                         |1                         |1                         |**0**               |

*DomDomain, nr of rows = 10*

## see also

- argmin_ifdefined_uint16, a version of the argmin_ifdefined function resulting in a uint16 data item
- argmin_ifdefined_uint8, a version of the argmin_ifdefined function resulting in a uint8 data item
- [[argmin_alldefined]]
- [[argmin]]
