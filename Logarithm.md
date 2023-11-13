*[[Transcendental functions]] logarithm*

## syntax

- log(*a*)

## definition

log(*a*) is defined as ln(a) or <sup>e</sup>log a ([**natural logarithm**](https://en.wikipedia.org/wiki/Natural_logarithm)).

log(*a*) results in [[null]] values for negative or zero values of [[data item]] *a*.

## applies to

- data item with float32 or float64 value type

## example
<pre>
attribute&lt;float32&gt; lnA (ADomain) := <B>log(</B>A<B>)</B>;
</pre>

| A   | **lnA**  |
|----:|---------:|
| 1   | **0**    |
| 2   | **0.69** |
| 3.5 | **1.25** |
| 5   | **1.61** |
| 10  | **2.30** |

*ADomain, nr of rows = 5*