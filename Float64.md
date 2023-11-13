*[[Conversion functions]] float64*

## concept

1. float64 a 64 bits (8 bytes) floating point [[value type]].
2. float64() is a function converting [[data item|data item]] or [[units|unit]] of other value types to the float64 value type.

This page describes the float64() function.

## syntax

-   float64(*a*)
-   *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>d</B>

## definition

float64(*a*) results in a **64 bits(8 bytes) floating point** [[item|tree item]] converted from item *a*. The function results for:

- integer data items, the integer value is interpreted as floating point value (e.g. an integer value 1 is interpreted as 1.0), if the value exceeds the maximum value for the float64 value type, the result is [[null]];
- float32 data items, we refer to  documentation on calculation with floating point in your processor for more exact information on how values will be rounded off;
- boolean data items, 1.0 for True values and 0.0 for False values;
- string data items, if the value starts with an allowed numeric value for the value type, this value is converted to a float64 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be null;
- units, a 64 bits(8 bytes) floating point unit of the converted unit.

## description

The float64() function may be used to denote a [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) to a float64 value type but this is not necessary, as [literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)).0 (e.g. 1.0) are by default considered as float64.

## applies to

data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

-   5.15
-   d suffix since 7.105

## example
<pre>
1. parameter&lt;float64&gt; float64Numeric1 := <B>float64(</B>1<B>)</B>;
2. parameter&lt;float64&gt; float64Numeric1 := 1<B>d</B>;

3. attribute&lt;float64&gt; float64A (ADomain) := <B>float64(</B>A<B>)</B>;
4. attribute&lt;float64&gt; float64B (ADomain) := <B>float64(</B>B<B>)</B>;
</pre>
| A(float64) | B(string)   |**float64A** | **float64B** | 
|-----------:|-------------|------------:|-------------:|
| 0          | 'Hello'     | **0**       | **null**     |
| 1          | 'Test'      | **1**       | **null**     |
| 1000000    | null        | **1000000** | **null**     |
| -2.5       | 'Two words' | **-2**      | **null**     |
| 99.9       | '88a'       | **99**      | **88**       |

*ADomain, nr of rows = 5*