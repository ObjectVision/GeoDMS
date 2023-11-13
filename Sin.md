*[[Trigonometric functions]] sin(e)*

## syntax

- sin(*angle*)

## definition

sin(*angle*) results in the [**sine**](https://en.wikipedia.org/wiki/Sine) of [[data item]] *angle*.

## description

data item *angle* need to be expressed in the [[unit]] radian (radiaal), the [SI unit](https://en.wikipedia.org/wiki/International_System_of_Units) for angles.

## applies to

data item *angle* with float32 or float64 [[value type]]

## since version

5.18

## example

<pre>
attribute&lt;float64&gt; Angle_rad    (ADomain) := Angle * pi() / 180.0;
attribute&lt;float64&gt; sinAngle_rad (ADomain) := <B>sin(</B>Angle_rad<B>)</B>;
</pre>

| Angle(°)  | Angle_rad(rad) |**sinAngle_radA**|
|----------:|---------------:|----------------:|
| 0         | 0              | **0**           |
| 30        | 0.52           | **0.5**         |
| 45        | 0.79           | **0.71**        |
| 420       | 7.33           | **0.87**        |
| null      | null           | **null**        |

*ADomain, nr of rows = 5*

## see also

- [[cos]]
- [[tan]]