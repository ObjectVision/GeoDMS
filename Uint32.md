*[[Conversion functions]] uint32*

## concept

1. uint32 is a 32 bits (4 bytes) unsigned integer [[value type]].
2. uint32() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the uint32 value type.

This page describes the uint32() function.

## syntax

- uint32(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>u</B>

## description

The uint32() function may be used to denote a [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) to a uint32 value type but this is not necessary, as [literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) are by default considered as uint32.

## definition

uint32(*a*) results in a **32 bits(4 bytes) unsigned integer** [[item|tree item]] converted from item *a*. The function results for:

- *integer* data items: the integer value is interpreted as uint32 value, if the value exceeds the allowed value range for the uint32 value type, the resulting value will be [[null]];
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the uint32 value type, the resulting value will be null;
- *boolean* data items: 1 for True values and 0 for False values;
- *string* data items, if the value starts with an allowed numeric value for the value type, this value is converted to a uint32 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be null;
- *units*, a 32 bits(4 bytes) unsigned integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

- 5.15
- u suffix: since 7.105

## example

<pre>
1. parameter&lt;uint32&gt; uint32Numeric1 := <B>uint32(</B>1<B>)</B>;
2. parameter&lt;uint32&gt; uint32Numeric1 := 1<B>u</B>;

3. attribute&lt;uint32&gt; uint32A (ADomain) := <B>uint32(</B>A<B>)</B>;
4. attribute&lt;uint32&gt; uint32B (ADomain) := <B>uint32(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **uint32A** | **uint32B** |
|-----------:|-------------|------------:|------------:|
| 0          | 'Hello'     | **0**       | **null**    |
| 1          | 'Test'      | **1**       | **null**    |
| 1000000    | null        | **1000000** | **null**    |
| -2.5       | 'Two words' | **null**    | **null**    |
| 99.9       | '88a'       | **99**      | **88**      |

*ADomain, nr of rows = 5*