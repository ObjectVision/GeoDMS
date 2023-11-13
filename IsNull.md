*[[Predicates functions]] IsNull*

## syntax

-   IsNull(*a*)

## definition

IsNull(*a*) results in a boolean [[data item]] with values **True for [[null]] and False for non null values** of data item *a*.

## applies to

- data item with Numeric, Point or string [[value type]]

## example
<pre>
attribute&lt;bool&gt; IsNullA (ADomain) := <B>IsNull(</B>A<B>)</B>;
</pre>

| A(float32) |**IsNullA** |
|-----------:|------------|
| 0          | **False**  |
| null       | **True**   |
| 1000000    | **False**  |
| -2.5       | **False**  |
| 99.9       | **False**  |

<em>ADomain, nr of rows = 5</em>

## see also

- [[IsDefined]]