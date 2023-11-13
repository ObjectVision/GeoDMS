*[[Ordering functions]] less than or equals to or left side has null values*

## syntax

- le_or_lhs_null(*a*, *b*)

## definition

le_or_lhs_null(*a*, *b*) results in a boolean [[data item]] indicating if the values of data item *a* are **less than** or equals the corresponding values of data item *b* or if the **values of data item a are [[null]]**.

## description

The comparison with missing values in data item *a* results in the value True (except for null values in data item *b*).

## applies to

Data items with Numeric, string or bool [[value type]]

## conditions

1.  [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain).
2.  [[Arguments|argument]] must have matching:
    -   [[value type]]
    -   [[metric]]

## example

<pre>
attribute&lt;bool&gt; AltB (CDomain) := <B>le_or_lhs_null(</B>A, B<B>)</B>;
</pre>

| A    | B    | **AltB**  |
|-----:|-----:|-----------|
| 0    | 0    | **True**  |
| 1    | 2    | **True**  |
| 2.5  | 2.5  | **True**  |
| -100 | 100  | **True**  |
| 999  | -999 | **False** |
| null | 0    | **True**  |
| null | null | **False** |
| 0    | null | **False** |
| null | 100  | **True**  |
| 100  | null | **False** |

*CDomain, nr of rows = 10*

## see also

- [[less than or equals to|le]] (\<=)