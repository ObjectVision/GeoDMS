*[[Conversion functions]] int64*

## concept

1. int64 is a 64 bits (8 bytes) signed integer [[value type]].
2. int64() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the int64 value type.

This page describes the int64() function.

## syntax

- int64(*a*)
- *[literal](https://en.wikipedia.org/wiki/Literal_(computer_programming))*<B>i64</B>

## definition

int64(*a*) results in a **64 bits(8 bytes) signed integer** [[item|tree item]] converted from item *a*. The function results for:
- *integer* data items: the integer value is interpreted as int64 value, if the value exceeds the allowed value range for the int64 value type, the resulting value will be [[null]];
- *float32/64* data items: the value before the decimal point (so 1.9 will be rounded off to 1 and 2.0 to 2). If the value exceeds the allowed value range for the int64 value type, the resulting value will be null;
- *boolean* data items: 1 for True values and 0 for False values;
- *string* data items, if the value starts with an allowed numeric value for the value type, this value is converted to a int64 value type. Other characters after the numeric values are ignored. If the string does not start with an allowed numeric value for the value type, the resulting value will be null;
- *units*, a 64 bits(8 bytes) signed integer unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, uint4, bool or string value type

## since version

- 5.15
- i64 suffix: since 7.105

## example

<pre>
1. parameter&lt;int64&gt; int64Numeric1 := <B>int64(</B>1<B>)</B>;
2. parameter&lt;int64&gt; int64Numeric1 := 1<B>i64</B>;

3. attribute&lt;int64&gt; int64A (ADomain) := <B>int64(</B>A<B>)</B>;
4. attribute&lt;int64&gt; int64B (ADomain) := <B>int64(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **int64A**  | **int64B** |
|------------|-------------|-------------|------------|
| 0          | 'Hello'     | **0**       | **null**   |
| 1          | 'Test'      | **1**       | **null**   |
| 1000000    | null        | **1000000** | **null**   |
| -2.5       | 'Two words' | **-2**      | **null**   |
| 99.9       | '88a'       | **99**      | **88**     |

*ADomain, nr of rows = 5*