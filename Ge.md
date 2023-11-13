*[[Ordering functions]] greater than or equals to (>=)*

## syntax

1. ge(*a*, *b*)
2. *a* >= *b*

## definition

ge(*a*, *b*) or *a* \> *b* results in a boolean [[data item]] indicating if the values of data item *a* are **greater than or equal to** the corresponding values of data item *b*.

## description

Each comparison with missing values results in the value false.

## applies to

Data items with Numeric, string or bool [[value type]]

## conditions

1. [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain).
2. [[Arguments|argument]] must have matching:
    - [[value type]]
    - [[metric]]

## example

<pre>
1. attribute&lt;bool&gt; AgeB (CDomain) := <B>ge(</B>A, B<B>)</B>;
2. attribute&lt;bool&gt; AgeB (CDomain) := A <B>&gt;=</B> B;
</pre>

| A    | B    | **AgeB**  |
|-----:|-----:|-----------|
| 0    | 0    | **True**  |
| 1    | 2    | **False** |
| 2.5  | 2.5  | **True**  |
| -100 | 100  | **False** |
| 999  | -999 | **True**  |
| null | 0    | **False** |
| null | null | **False** |
| 0    | null | **False** |
| null | 100  | **False** |
| 100  | null | **False** |

*CDomain, nr of rows = 10*

## see also

- [[greater than or equals to or right side has null values|ge_or_rhs_null]]