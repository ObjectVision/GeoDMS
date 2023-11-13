*[[Conversion functions]] uint4*

## concept

1.  uint4 is a 4 bits unsigned integer [[Sub Byte Element]].
2.  uint4() is a function converting [[data items|data item]] or [[units|unit]] of other [[value types|value type]] to the uint4 value type.

This page describes the uint4() function.

## syntax

- uint4(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>u4</B>

## definition

uint4(*a*) results in a **4 bits unsigned integer** [[item|tree item]] converted from tree item *a*. The function results for:

- *integer* data items: the integer value is interpreted as uint4 value, if the value exceeds the allowed value range for the uint4 value type, the resulting value will be 0;
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the uint4 value type, the resulting value will be 0;
- *boolean* data items: 1 for True values and 0 for False values;
- *string* data items, if the value starts with an allowed numeric value for the value type, this value is converted to a uint4 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be 0;
- *units*, a 4 bits unsigned integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, bool or string value type

## since version

- 5.15
- u4 suffix: since 7.105

## example

<pre>
1. parameter&lt;uint4&gt; uint4Numeric1 := <B>uint4(</B>1<B>)</B>;
2. parameter&lt;uint4&gt; uint4Numeric1 := <B>1u4</B>;

3. attribute&lt;uint4&gt; uint4A (ADomain) := <B>uint4(</B>A<B>)</B>;
4. attribute&lt;uint4&gt; uint4B (ADomain) := <B>uint4(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **uint4A** | **uint4B** |
|-----------:|-------------|-----------:|-----------:|
| 0          | 'Hello'     |      **0** | **0**      |
| 1          | 'Test'      |      **1** | **0**      |
| 1000000    | null        |      **0** | **0**      |
| -2.5       | 'Two words' |      **0** | **0**      |
| 99.9       | '88a'       |      **0** | **0**      |

*ADomain, nr of rows = 5*