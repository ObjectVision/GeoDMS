*[[Ordering functions]] Sort*

## syntax

- sort(*a*)

## definition

The sort(*a*) function sorts a numeric, point or boolean [[attribute]] in an numeric ascending order.

- [[null]] values become the first elements in the resulting order
- in boolean items False values are sorted before True values
- in [[two-dimensional|two-dimensional domain]] [[data items|data item]], the sort order is based on the first coordinate.

The [[sort_str]] function sort string attributes in alphabetic order.

## applies to

attribute with Numeric, Point or bool [[value type]]

## example

<pre>
attribute&lt;float32&gt; sortA (CDomain) := <B>sort(</B>A<B>)</B>;
</pre>

| A    | **sortA**|
|-----:|---------:|
| 0    | **null** |
| 1    | **null** |
| 2.5  | **null** |
| -100 | **-100** |
| 999  | **0**    |
| null | **0**    |
| null | **1**    |
| 0    | **2.5**  |
| null | **100**  |
| 100  | **999**  |

*CDomain, nr of rows = 10*

## see also

- [[sort_str]]
- [[reverse]]