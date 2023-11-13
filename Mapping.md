*[[Relational functions]] mapping*

## syntax

- mapping(*SrcUnit*, *DstUnit*)

## definition

mapping(*SrcUnit*, *DstUnit*) results in a [[relation]] with the [[index numbers]] of the [[domain unit]] *DstUnit*. The resulting [[attribute]] has as [[values unit]] *DstUnit* and as domain unit *SrcUnit*.

## description

mapping(*SrcUnit*, *DstUnit*) is equivalent with [[convert]]([[id]] (*SrcUnit*), *DstUnit*), but requires less memory.

## applies to

- *SrcUnit*, *DstUnit* [[units|unit]] with [[value types|value type]] of the group CanBeDomainUnit

## since version

7.119

## example

<pre>
unit&lt;uint32&gt; RegionSrc: nrofrows = 5;
unit&lt;uint32&gt; RegionDst: nrofrows = 5;
attribute&lt;Region&gt; RegionDst_rel (RegionSrc) := <B>mapping(</B>RegionSrc, RegionDst<B>)</B>;
</pre>

| **RegionDst_rel** |
|------------------:|
| **0**             |
| **1**             |
| **2**             |
| **3**             |
| **4**             |

*domain RegionSrc, nr of rows = 5*

## see also

- [[mapping_count]]