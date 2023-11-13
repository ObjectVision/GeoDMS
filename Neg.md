*[[Arithmetic functions]] negative (-)*

## syntax

-   neg(*a*)
-   \- *a*

## definition

neg(*a*) or - *a* results in the **negative** values of [[data item]] *a*.

## applies to

Data item or [[unit]] with Numeric [[value type]]

## example

<pre>
1. attribute&lt;float32&gt; negA (ADomain) := <B>neg(</B>A<B>)</B>;
2. attribute&lt;float32&gt; negA (ADomain) := <B>-</B> A;
</pre>

| A   | **negA** |
|----:|---------:|
| 0   | **0**    |
| 1   | **-1**   |
| -2  | **2**    |
| 3.6 | **-3.6** |
| 999 | **-999** |

*ADomain, nr of rows = 5*

## see also

- [[sub]]