*[[Predicates functions]] IsNegative*

## syntax

- IsNegative(*a*)

## definition

IsNegative(*a*) results in a boolean [[data item]] with values **True for negative and False for postive and zero values** of data item *a*.

## applies to

- data item with Numeric [[value type]]

## example

<pre>
attribute&lt;bool&gt; IsNegative (ADomain) := <B>IsNegative(</B>A<B>)</B>;
</pre>

| A(float32) |**IsNegativeA** |
|-----------:|----------------|
| 0          | **False**      |
| null       | **False**      |
| 1000000    | **False**      |
| -2.5       | **True**       |
| 99.9       | **False**      |

*ADomain, nr of rows = 5*

## see also
- [[IsPositive]]
- [[IsZero]]