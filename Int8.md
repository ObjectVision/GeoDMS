*[[Conversion functions]] int8*

## concept

1. int8 is a 8 bits (1 byte) signed integer [[value type]].
2. int8() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the int8 value type.

This page describes the int8() function.

## syntax

- int8(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>c</B>

## definition

int8(*a*) results in a **8 bits(1 byte) signed integer** [[item|tree item]] converted from item *a*. The function results for:
- *integer* data items: the integer value is interpreted as int8 value, if the value exceeds the allowed value range for the int8 value type, the resulting value will be [[null]];
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the int8 value type, the resulting value will be null;
- *boolean* data items: 1 for True values and 0 for False values;
- *string* data items, if the value starts with an allowed numeric value for the value type, this value is converted to a int8 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be null;
- *units*, a 8 bits(1 byte) signed integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

- 5.15
- c suffix: since 7.105

## example

<pre>
1. parameter&lt;int8&gt; int8Numeric1 := <B>int8(</B>1<B>)</B>;
2. parameter&lt;int8&gt; int8Numeric1 := 1<B>c</B>;

3. attribute&lt;int8&gt; int8A (ADomain) := <B>int8(</B>A<B>)</B>;
4. attribute&lt;int8&gt; int8B (ADomain) := <B>int8(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **int8A**| **int8B** |
|-----------:|-------------|---------:|----------:|
| 0          | 'Hello'     | **0**    | **null**  |
| 1          | 'Test'      | **1**    | **null**  |
| 1000000    | null        | **null** | **null**  |
| -2.5       | 'Two words' | **-2**   | **null**  |
| 99.9       | '88a'       | **99**   | **88**    |

*ADomain, nr of rows = 5*