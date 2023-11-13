A Value Type indicates the kind of values that can be stored in parameters and attributes.

- A [[unit]] determines a set of comparable values (= domain in the terminology of C.J. Date) and must be related to a specific Value Type.
- A [[parameter]] stores one element value, for which the value type is determined by the [[values unit]] of the parameter.
- An [[attribute]] is implemented as an array of elements of a specific value type. The value type of these elements is determined by the values unit of the attribute. The location of these elements is implicitly determined by their position in the array. The set of valid locations and the type of these location elements is determined by it's [[domain unit]].

## examples of the usage of the value type float32
<pre>
1. unit&lt;float32&gt;      meter             := baseunit('m', &lt;float32&gt;);
2. parameter&lt;meter&gt;   RealWagonLength   := 22[meter];
3. parameter&lt;float32&gt; MarklinHO         := float32(1.0 / 87.0);
4. parameter&lt;meter&gt;   ScaledWagonLength := MarklinHO * RealWagonLength;
</pre>

### Available value types
<table>
<thead>
<tr class="header">
<th><sup>Name</sup></th>
<th><sup>Description</sup></th>
<th><sup>Size</sup></th>
<th><sup>MinValue</sup></th>
<th><sup>MaxValue</sup></th>
<th><sup>Missing<br>Value</sup></th>
<th><sup>CanBe<br>DomainUnit</sup></th>
<th><sup>Numeric<br>Group</sup></th>
<th><sup>Signed<br>Group</sup></th>
<th><sup>Point<br>Group</sup></th>
<th><sup>Suffix</sup></th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><sup>[[bool]]</sup></td>
<td><sup>boolean</sup></td>
<td><sup>1 bit</sup></td>
<td><sup>false</sup></td>
<td><sup>true</sup></td>
<td><sup>n/a</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
</tr>
<tr class="even">
<td><sup>[[uint2]]</sup></td>
<td><sup>unsigned integer</sup></td>
<td><sup>2 bits</sup></td>
<td><sup>0</sup></td>
<td><sup>3</sup></td>
<td><sup>n/a</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>u2</sup></td>
</tr>
<tr class="odd">
<td><sup>[[uint4]]</sup></td>
<td><sup>unsigned integer</sup></td>
<td><sup>4 bits</sup></td>
<td><sup>0</sup></td>
<td><sup>15</sup></td>
<td><sup>n/a</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>u4</sup></td>
</tr>
<tr class="even">
<td><sup>[[uint8]]</sup></td>
<td><sup>unsigned integer</sup></td>
<td><sup>1 byte</sup></td>
<td><sup>0</sup></td>
<td><sup>0xFE (254)</sup></td>
<td><sup>0xFF (255)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>b</sup></td>
</tr>
<tr class="odd">
<td><sup>[[uint16]]</sup></td>
<td><sup>unsigned integer</sup></td>
<td><sup>2 bytes</sup></td>
<td><sup>0</sup></td>
<td><sup>0xFFFE (65534)</sup></td>
<td><sup>0xFFFF (65535)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>w</sup></td>
</tr>
<tr class="even">
<td><sup>[[uint32]]</sup></td>
<td><sup>unsigned integer</sup></td>
<td><sup>4 bytes</sup></td>
<td><sup>0</sup></td>
<td><sup>0xFFFFFFFE (4294967294)</sup></td>
<td><sup>0xFFFFFFFF (4294967295)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>u</sup></td>
</tr>
<tr class="odd">
<td><sup>[[uint64]]</sup></td>
<td><sup>unsigned long integer</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>0x0000000<BR>000000000</sup></td>
<td><sup>0xFFFFFFF<BR>FFFFFFFFE</sup></td>
<td><sup>0xFFFFFFF<BR>FFFFFFFFF</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>u64</sup></td>
</tr>
<tr class="even">
<td><sup>[[int8]]</sup></td>
<td><sup>signed integer</sup></td>
<td><sup>1 byte</sup></td>
<td><sup>0x81 (-127)</sup></td>
<td><sup>0x7F (127)</sup></td>
<td><sup>0x80 (-128)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>c</sup></td>
</tr>
<tr class="odd">
<td><sup>[[int16]]</sup></td>
<td><sup>signed integer</sup></td>
<td><sup>2 bytes</sup></td>
<td><sup>0x8001 (-32767)</sup></td>
<td><sup>0x7FFF (32767)</sup></td>
<td><sup>0x8000 (-32768)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>s</sup></td>
</tr>
<tr class="even">
<td><sup>[[int32]]</sup></td>
<td><sup>signed integer</sup></td>
<td><sup>4 bytes</sup></td>
<td><sup>-2147483647 (0x80000001)</sup></td>
<td><sup>2147483647 (0x7FFFFFFF)</sup></td>
<td><sup>-2147483648 (0x80000000)<br />
(-9999 before version 7.126)</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>i</sup></td>
</tr>
<tr class="odd">
<td><sup>[[int64]]</sup></td>
<td><sup>signed long integer</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>0x8000000<BR>000000001</sup></td>
<td><sup>0x7FFFFFF<BR>FFFFFFFFF</sup></td>
<td><sup>0x8000000<BR>000000000</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>i64</sup></td>
</tr>
<tr class="even">
<td><sup>[[float32]]</sup></td>
<td><sup>floating point</sup></td>
<td><sup>4 bytes</sup></td>
<td><sup>-3.4E+38 (7 digits)</sup></td>
<td><sup>3.4E+38 (7 digits)</sup></td>
<td><sup><a href="https://en.wikipedia.org/wiki/NaN#Quiet_NaN">QNaN</a> (-9999.0f before version 7.126)</sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>f</sup></td>
</tr>
<tr class="odd">
<td><sup>[[float64]]</sup></td>
<td><sup>floating point</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>-1.7E+308 (15 digits)</sup></td>
<td><sup>1.7E+308 (15 digits)</sup></td>
<td><sup><a href="https://en.wikipedia.org/wiki/NaN#Quiet_NaN">QNaN</a> (-9999.0 before version 7.126)</sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup>d</sup></td>
</tr>
<tr class="even">
<td><sup>[[spoint]]</sup></td>
<td><sup>two int16 coördinates (row, col)</sup></td>
<td><sup>4 bytes</sup></td>
<td><sup>(MinValue of int16)^2</sup></td>
<td><sup>(MaxValue of int16)^2</sup></td>
<td><sup>(MissingValue of int16)^2</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="odd">
<td><sup>[[wpoint]]</sup></td>
<td><sup>two uint16 coördinates (row, col)</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>(MinValue of uint16)^2</sup></td>
<td><sup>(MaxValue of uint16)^2</sup></td>
<td><sup>(MissingValue of uint16)^2</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="even">
<td><sup>[[ipoint]]</sup></td>
<td><sup>two int32 coördinates (row, col)</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>(MinValue of int32)^2</sup></td>
<td><sup>(MaxValue of int32)^2</sup></td>
<td><sup>(MissingValue of int32)^2</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="odd">
<td><sup>[[upoint]]</sup></td>
<td><sup>two uint32 coördinates (row, col)</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>(MinValue of uint32)^2</sup></td>
<td><sup>(MaxValue of uint32)^2</sup></td>
<td><sup>(MissingValue of uint32)^2</sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="even">
<td><sup>[[fpoint]]</sup></td>
<td><sup>two float32 coördinates (x, y)</sup></td>
<td><sup>8 bytes</sup></td>
<td><sup>(MinValue of Float32)^2</sup></td>
<td><sup>(MaxValue of Float32)^2</sup></td>
<td><sup>(MissingValue of Float32)^2</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="odd">
<td><sup>[[dpoint]]</sup></td>
<td><sup>two float64 coördinates (x, y)</sup></td>
<td><sup>16 bytes</sup></td>
<td><sup>(MinValue of Float64)^2</sup></td>
<td><sup>(MaxValue of Float64)^2</sup></td>
<td><sup>(MissingValue of Float64)^2</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup>X</sup></td>
<td><sup></sup></td>
</tr>
<tr class="even">
<td><sup>[[string]]</sup></td>
<td><sup>sequence of chars</sup></td>
<td><sup>2*sizeof<br>(UInt32)<br>+n*sizeof<br>(uint8)</sup></td>
<td><sup></sup></td>
<td><sup>0x255 0x255 0x255 0x255</sup></td>
<td><sup>'null'</sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
<td><sup></sup></td>
</tr>
</tbody>
</table>
</span>