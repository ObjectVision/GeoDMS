*[[Trigonometric functions]] a(rc)tan(gent)*

## syntax

- atan(*a*)

## definition

atan(*a*) results in the [**arc tangent**](https://en.wikipedia.org/wiki/Inverse_trigonometric_functions) of [[data item]] *a*.

## applies to

data item *a* with float32 or float64 [[value type]]

## since version

5.18

## example

<pre>
attribute&lt;float64&gt; atanAngle_rad (ADomain) := <B>atan(</B>tanAngle_rad<B>)</B>;
</pre>

| Angle(°) | Angle_rad(rad) |tanAngle_radA | **atanAngle_rad** |
|---------:|---------------:|-------------:|------------------:|
| 0        | 0              | 1            | **0**             |
| 30       | 0.52           | 0.58         | **0.52**          |
| 45       | 0.79           | 1            | **0.79**          |
| 420      | 7.33           | 1.73         | **1.05**          |
| null     | null           | null         | **null**          |

*ADomain, nr of rows = 5*

## see also

- [[tan]]