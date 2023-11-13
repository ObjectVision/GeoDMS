*[[Predicates functions]] IsZero*

## syntax

- IsZero(*a*)

## definition

IsZero(*a*) results in a boolean [[data item]] with values **True for zero and False for all other values** of data item *a*.

## applies to

- data item with Numeric [[value type]]

## example

<pre>
attribute&lt;bool&gt; IsZero (ADomain) := <B>IsZero(</B>A<B>)</B>;
</pre>

| A(float32) | **IsZeroA** |
|-----------:|-------------|
| 0          | **True**    |
| null       | **False**   |
| 1000000    | **False**   |
| -2.5       | **False**   |
| 99.9       | **False**   |

*ADomain, nr of rows = 5*

## see also

- [[IsPositive]]
- [[IsNegative]]