*[[Arithmetic functions]] absolute (||)*

## syntax

-   abs(*a*)

## definition

abs(*a*) results in the [**absolute values**](https://en.wikipedia.org/wiki/Absolute_value) (|*a*|) of [[data item]] *a*, meaning *a* if *a* \>= 0 and
-*a* if *a* \< 0.

## applies to

Data item with int8, int16, int32, float32 or float64 [[value type]]

## example

<pre>
attribute&lt;float32&gt; absA (ADomain) := <B>abs(</B>A<B>)</B>;
</pre>

| A   |**absA** |
|----:|--------:|
| 0   | **0**   |
| 1   | **1**   |
| -2  | **2**   |
| 3.6 | **3.6** |
| 999 | **999** |

*ADomain, nr of rows = 5*