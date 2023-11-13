*[[Conversion functions]] AsHex*

## syntax

- AsHex(*a*)

## definition

AsHex(*a*) results in a string [[data item]] with the **hexadecimal representation** of data item *a*.

## applies to

- since version 7.408: data item with uint4, uint8, uint16, uint32, uint64, or string [[value type]] 
   - for integers, the resulting strings always have a size based on the number of bits in the argument divided by 4 
   - for strings, the resulting string size is always the input size in bytes times 2.
- since version 5.40: data item with uint32 or string value type

## example

<pre>
attribute&lt;uint8&gt;  A8     (ADomain) : [ 0, 1, 11, 100, 255];
attribute&lt;uint32&gt; A32    (ADomain) := uint32(A8);

attribute&lt;string&gt; AsHexA (ADomain) := <B>asHex(</B>A32<B>)</B>;
attribute&lt;string&gt; AsHexB (ADomain) := <B>asHex(</B>A8<B>)</B>;
</pre>

| A8  | AsHexA (prior to 7.408) | AsHexA (since 7.408) | AsHexB (since 7.408) |
|----:|-------------------------|----------------------|----------------------|
| 0   | '**0**'                 | **'00000000**'       | **'00**'             |
| 1   | '**1**'                 | **'00000001**'       | **'01**'             |
| 11  | '**B**'                 | **'0000000B**'       | **'0B**'             |
| 100 | '**64**'                | **'00000064**'       | **'64**'             |
| 255 | **'FFFFFFFF**'          | **'FFFFFFFF**'       | **'FF**'             |

*ADomain, nr of rows = 5*