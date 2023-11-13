*[[Arithmetic functions]] square number (a<sup>2</sup>)*

## syntax

-   sqr(*a*)

## definition

sqr(*a*) results in the [**square number**](https://en.wikipedia.org/wiki/Square_number) of the values of [[data item]] *a*, synonym for *a*<sup>2</sup>.

## applies to

Data item or [[unit]] with Numeric [[value type]]

## example

<pre>
attribute&lt;float32&gt; sqrA (ADomain) := <B>sqr(</B>A<B>)</B>;
</pre>

| A   | **sqrA**   | 
|----:|-----------:|
| 0   | **0**      |
| 1   | **1**      |
| -2  | **4**      |
| 3.6 | **12.96**  |
| 999 | **998001** |

*ADomain, nr of rows = 5*