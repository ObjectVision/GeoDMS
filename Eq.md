*[[Ordering functions]] equals (==)*

## syntax

-   eq(*a*, *b*)
-   *a* == *b*

## definition

eq(*a*, *b*) or *a* ==*b* results in a boolean [[data item]] indicating if the values of data item *a* are **equal to** the corresponding values of data item *b*.

## description

The comparison between two missing values ([[null]] == null) results in the value False.

## applies to

Data items with Numeric, Point, string or bool [[value type]]

## conditions

1. [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)%7Cliterals) or [[parameters|parameter]] can be compared to data items of any domain unit.
2. Arguments must have matching:
   - [[value type]]
   - [[metric]]

## example

<pre>
1. attribute&lt;bool&gt; AisB (CDomain) := <B>eq</B>(A, B);
2. attribute&lt;bool&gt; AisB (CDomain) := A <B>==</B> B;
</pre>

| A    | B    | **AisB**  |
|-----:|-----:|-----------|
| 0    | 0    | **True**  |
| 1    | 2    | **False** |
| 2.5  | 2.5  | **True**  |
| -100 | 100  | **False** |
| 999  | -999 | **False** |
| null | 0    | **False** |
| null | null | **False** |
| 0    | null | **False** |
| null | 100  | **False** |
| 100  | null | **False** |

*CDomain, nr of rows = 10*

## see also

- [[equals or both values null|eq_or_both_null]]
- [[float_isNearby]]