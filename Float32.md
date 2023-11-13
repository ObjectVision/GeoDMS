*[[Conversion functions]] float32*

## concept

1.  float32 a 32 bits (4 bytes) floating point [[value type]].
2.  float32() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the float32 value type.

This page describes the float32() function.

## syntax

- float32(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>f</B>

## definition

float32(*a*) results in a **32 bits(4 bytes) floating point** [[item|tree item]] converted from item *a*. The function results for:

- integer data items, the integer value is interpreted as floating point value (e.g. an integer value 1 is interpreted as 1.0), if the value exceeds the maximum value for the float32 value type, the result is [[null]];
- float64 data items, we refer to documentation on calculation with floating point in your processor for more exact information on how values will be rounded off;
- boolean data items, 1.0 for True values and 0.0 for False values;
- string data items if the value starts with an allowed numeric value for the value type, this value is converted to a float32 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be null;
- units, a 32 bits(4 bytes) floating point unit of the converted unit.

## applies to

data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

- 5.15
- f suffix since 7.105

## example
<pre>
1. parameter&lt;float32&gt; float32Numeric1 := <B>float32</B>(1<B>)</B>;
2. parameter&lt;float32&gt; float32Numeric1 := 1<B>f</B>;

3. attribute&lt;float32&gt; float32A (ADomain) := <B>float32(</B>A<B>)</B>;    `
4. attribute&lt;float32&gt; float32B (ADomain) := <B>float32(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **float32A** | **float32B** |
|-----------:|-------------|-------------:|-------------:|
| 0          | 'Hello'     | **0**        | **null**     |
| 1          | 'Test'      | **1**        | **null**     |
| 1000000    | null        | **1000000**  | **null**     |
| -2.5       | 'Two words' | **-2**       | **null**     |
| 99.9       | '88a'       | **99**       | **88**       |

*ADomain, nr of rows = 5*