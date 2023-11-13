*[[Conversion functions]] Uint2*

## concept

1.  uint2 is a 2 bits unsigned integer [[Sub Byte Element]].
2.  uint2() is a function converting [[data items|data item]] or [[units|unit]] of other [[value types|value type]] to the uint2 value type.

This page describes the uint2() function.

## syntax

- uint2(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>u2</B>

## definition

uint2(*a*) results in a **2 bits unsigned integer** [[item|tree item]] converted from  item *a*. The function results for:

- *integer* data items: the integer value is interpreted as uint2 value, if the value exceeds the allowed value range for the uint2 value type, the resulting value will be 0;
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the uint2 value type, the resulting value will be 0; *boolean* data items: 1 for True values and 0 for False values;
- *string* [[data items|data item]], if the value starts with an allowed numeric value for the [[value type]], this value is converted to a uint2 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be 0; 
- *units*, a 2 bits unsigned integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint4, bool or string value type

## since version

- 5.15
- u2 suffix: since 7.105

## example

<pre>
1. parameter&lt;uint2&gt; uint2Numeric1 := <B>uint2(</B>1<B>)</B>;
2. parameter&lt;uint2&gt; uint2Numeric1 := 1<B>u2</B>;

3. attribute&lt;uint2&gt; uint2A (ADomain) := <B>uint2(</B>A<B>)</B>;
4. attribute&lt;uint2&gt; uint2B (ADomain) := <B>uint2(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **uint2A** | **uint2B** |
|-----------:|-------------|-----------:|-----------:|
| 0          | 'Hello'     | **0**      | **0**      |
| 1          | 'Test'      | **1**      | **0**      |
| 1000000    | null        | **0**      | **0**      |
| -2.5       | 'Two words' | **0**      | **0**      |
| 99.9       | '88a'       | **0**      | **0**      |

*ADomain, nr of rows = 5*