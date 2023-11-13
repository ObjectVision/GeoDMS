*[[Predicates functions]] IsPositive*

## syntax

- IsPositive(*a*)

## definition

IsPositive(*a*) results in a boolean [[data item]] with values **True for positive and False for negative and zero values** of data item *a*.

## applies to

- data item with Numeric [[value type]]

## example

<pre>
attribute&lt;bool&gt; IsPositive (ADomain) := <B>IsPositive(</B>A<B>)</B>;
</pre>

| A(float32) |**IsPositiveA** |
|-----------:|----------------|
| 0          | **False**      |
| null       | **False**      |
| 1000000    | **True**       |
| -2.5       | **False**      |
| 99.9       | **True**       |

*ADomain, nr of rows = 5*

## see also

- [[IsNegative]]
- [[IsZero]]