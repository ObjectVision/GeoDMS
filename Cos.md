*[[Trigonometric functions]] cos(ine)*

## syntax

- cos(*angle*)

## definition

cos(*angle*) results in the [**cosine**](https://en.wikipedia.org/wiki/Cosine) of [[data item]] *angle*.

## description

data item *angle* need to be expressed in the [[unit]] radian (radiaal), the [SI unit](https://en.wikipedia.org/wiki/International_System_of_Units) for angles.

## applies to

data item *angle* with float32 or float64 [[value type]]

## since version

5.18

## example

<pre>
attribute&lt;float64&gt; Angle_rad    (ADomain) := Angle * pi() / 180.0;
attribute&lt;float64&gt; cosAngle_rad (ADomain) := <B>cos(</B>Angle_rad<B>)</B>;
</pre>

| Angle(°) | Angle_rad(rad) | **cosAngle_radA** |
|---------:|---------------:|------------------:|
| 0        | 0              | **1**             |
| 30       | 0.52           | **0.87**          |
| 45       | 0.79           | **0.71**          |
| 420      | 7.33           | **0.5**           |
| null     | null           | **null**          |

*ADomain, nr of rows = 5*

## see also

- [[sin]]
- [[tan]]