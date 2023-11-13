*[[Conversion functions]] uint16*

## concept

1. uint16 is a 16 bits (2 bytes) unsigned integer [[value type]].
2. uint16() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the uint16 value type.

This page describes the uint16() function.

## syntax

- uint16(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>w</B>

## definition

uint16(*a*) results in a **16 bits(2 bytes) unsigned integer** [[item|tree item]] converted from item *a*. The function results for:
- *integer* data items: the integer value is interpreted as uint16 value, if the value exceeds the allowed value range for the uint16 value type, the resulting value will be [[null]];
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the uint16 value type, the resulting value will be null;
- *boolean* data items : 1 for True values and 0 for False values;
- *string* data items, if the value starts with an allowed numeric value for the value type, this value is converted to a uint16 value type. Other characters after the  numeric values are ignored. If the string does not start with an allowed numeric value for the [[value type]], the resulting value will be null;
- *units*, a 16 bits(2 bytes) unsigned integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

- 5.15
- w suffix: since 7.105

## example

<pre>
1. parameter&lt;uint16&gt; uint16Numeric1 := <B>uint16(</B>1<B>)</B>;
2. parameter&lt;uint16&gt; uint16Numeric1 := 1<B>w</B>;

3. attribute&lt;uint16&gt; uint16A (ADomain) := <B>uint16(</B>A<B>)</B>;
4. attribute&lt;uint16&gt; uint16B (ADomain) := <B>uint16(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   |  **uint16A** | **uint16B** |
|-----------:|-------------|-------------:|------------:|
| 0          | 'Hello'     |     **0**    | **null**    |
| 1          | 'Test'      |     **1**    | **null**    |
| 1000000    | null        |     **null** | **null**    |
| -2.5       | 'Two words' |     **null** | **null**    |
| 99.9       | '88a'       |     **99**   | **88**      |

*ADomain, nr of rows = 5*