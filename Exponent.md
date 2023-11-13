*[[Transcendental functions]] exp(onent)*

## syntax

- exp(*exponent*)

## definition

exp(*exponent*) is defined as [**e**](https://en.wikipedia.org/wiki/E_(mathematical_constant))<sup>**exponent**</sup>

exp(*exponent*) results in [[null]] values for the values of [[data item]] *exponent* for which the exp(onent) results are outside the scope of the [[value type]] (e.g. values above 88 for items of value type float32).

## applies to

- data item with float32 or float64 value type

## example

<pre>
attribute&lt;float32&gt; expA (ADomain) := <B>exp(</B>A<B>)</B>;
</pre>

| A   | **expA**     |
|----:|-------------:|
| 1   | **2.71**     |
| 2   | **7.39**     |
| 3.5 | **33.12**    |
| 5   | **148.41**   |
| 10  | **22026.46** |

*ADomain, nr of rows = 5*