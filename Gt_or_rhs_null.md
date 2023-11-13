*[[Ordering functions]] greater than or right side has null values*

## syntax

- gt_or_rhs_null(*a*,*b*)

## definition

gt_or_rhs_null(*a*, *b*) results in a boolean [[data item]] indicating if the values of data item *a* are **greater than** the corresponding values of data item *b* or if the **values of data item *a* are [[null]]**.

## description

The comparison with missing values in data item a results in the value True (except for null values in data item b).

## applies to

Data items with Numeric, string or bool [[value type]]

## conditions

1. [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming))
or [[parameters|parameter]] can be compared to data items of any domain).
2. [[Arguments|argument]] must have matching:
    - [[value type]]
    - [[metric]]

## example

<pre>
attribute&lt;bool&gt; AgtB(CDomain) := <B>gt_or_rhs_null(</B>A, B<B>)</B>;
</pre>

| A    | B    | **AgtB**  |
|-----:|-----:|-----------|
| 0    | 0    | **False** |
| 1    | 2    | **False** |
| 2.5  | 2.5  | **False** |
| -100 | 100  | **False** |
| 999  | -999 | **True**  |
| null | 0    | **False** |
| null | null | **False** |
| 0    | null | **True**  |
| null | 100  | **False** |
| 100  | null | **True**  |

<em>CDomain, nr of rows = 10</em>

## see also

 - [[greater than|gt]]