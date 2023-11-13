*[[Trigonometric functions]] tan(gent)*

## syntax

- tan(*angle*)

## definition

tan(*angle*) results in the [**tangent**](https://en.wikipedia.org/wiki/Trigonometric_functions) of [[data item]] *angle*.

## description

[[data item]] *angle* need to be expressed in the [[unit]] radian (radiaal), the [SI unit](https://en.wikipedia.org/wiki/International_System_of_Units) for angles.

## applies to

data item *angle* with float32 or float64 [[value type]]

## since version

5.18

## example

<pre>
attribute&lt;float64&gt; Angle_rad    (ADomain) := Angle * pi() / 180.0;
attribute&lt;float64&gt; tanAngle_rad (ADomain) := <B>tan(</B>Angle_rad<B>);</B>
</pre>

| Angle(Â) | Angle_rad(rad) | **tanAngle_radA** |
|---------:|---------------:|------------------:|
| 0        | 0              | **1**             |
| 30       | 0.52           | **0.58**          |
| 45       | 0.79           | **1**             |
| 420      | 7.33           | **1.73**          |
| null     | null           | **null**          |

*ADomain, nr of rows = 5*

## see also

- [[sin]]
- [[cos]]
- [[atan]]