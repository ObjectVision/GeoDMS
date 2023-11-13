*[[Conversion functions]] bool*

## concept

1. bool is a 1 bits [[sub byte element]], with as possible values: True and False.
2. bool() is a function converting [[data items|data item]] or [[units|unit]] of other value types to the bool value type.

This page describes the bool() function.

## syntax

- bool(*a*)

## definition

bool(*a*) results in a **boolean (True or False)** [[item|tree item]] converted from item *a*. The function results for:

- numeric data items in True for non zero values and False for zero values of [[data item]] a;
- string data items in True for all values starting with the character "T" and False for other values of data item a.
- units, a boolean (1 bit) unit of the converted unit.

## applies to

- data item or unit with Numeric, uint2, uint4 or string value type

## example

<pre>
1. attribute&lt;bool&gt; boolA (ADomain) := <B>bool(</B>A<B>)</B>;    
2. attribute&lt;bool&gt; boolB (ADomain) := <B>bool(</B>B<B>)</B>;
</pre>

| A(float32) | B(string)   | **boolA** | **boolB** |
|-----------:|-------------|-----------|-----------|
| 0          | 'Hello'     | **False** | **False** |
| 1          | 'Test'      | **True**  | **True**  |
| 1000000    | null        | **True**  | **False** |
| -2.5       | 'Two words' | **True**  | **True**  |
| 99.9       | '88a'       | **True**  | **False** |

*ADomain, nr of rows = 5*