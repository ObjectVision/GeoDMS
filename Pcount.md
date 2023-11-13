*[[Aggregation functions]] pcount*

## syntax

- pcount(*a*)
- pcount_uint(8|16|32)(a)

## definition

- pcount(a) is defined as the uint32 count of the **number of entries** of a [[relation]] in the related [[domain unit]], so pcount(a) is a synonym for [[count]](a, a).
- pcount_uint(8|16|32)(a) is the pcount variant resulting in uint8, uint16 and uint32 [[data items|data item]]  

## applies to

- [[attribute]] *a* with uint2, uint4, uint8, uint16, uint32, spoint, ipoint or bool [[value type]]

## example

<pre>
attribute&lt;uint32&gt; pcountReg (Region) := <B>pcount(</B>City/Region_rel<B>)</B>;
</pre>

| City/Region_rel |
|----------------:|
| 0               |
| 1               |
| 2               |
| 1               |
| 3               |
| null            |
| 3               |

*domain City, nr of rows = 7*


| **pcountReg** |
|--------------:|
| **1**         |
| **2**         |
| **1**         |
| **2**         |
| **0**         |

*domain Region, nr of rows = 5*

## see also

- [[count]]