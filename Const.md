*[[Constant functions]] const*

## syntax

- const(*value*, *domain unit*)
- const(*value*, *domain unit*, *values unit*)

## definition

const(*value*, *domain unit*) or const(*value*, *domain unit*, *values unit*) results in an [[attribute]] with the first [[argument]] ***value* as constant value** for each entry of the *[[domain unit]]* second argument.

The *values unit* can be configured with the *value* argument or explicitly as third argument).

## description

- const(*0* , *domain*) is synonym for const(*0*,*domain*,*unit32*).
- const(*0f* , *domain*) or const(*float32(0)*,*domain*) is synonym for const(*0*,*domain*,*float32*).
- const(*3[m]*, *domain*) is synonym for const (*3*, *domain*, m).

## applies to

- [literal](https://en.wikipedia.org/wiki/Literal) *value* with Numeric, Point, uint2, uint4, bool or string [[value type]]
- [[unit]] domain unit with value type of group CanBedomain unit
- unit [[values unit]] with Numeric, Point, uint2, uint4, bool or string value type

## conditions

The value type of argument *value* needs to match with the value type of the *values unit* argument.

## example

<pre>
attribute&lt;uint8&gt;  Uint8Att  (ADomain) := <B>const(</B>1b, ADomain<B>)</B>;
attribute&lt;string&gt; StringAtt (ADomain) := <B>const(</B>'const', ADomain, string<B>)</B>;
</pre>

| **Uint8Att** | **StringAtt** |
|-------------:|---------------|
| **1**        | **'const**'   |
| **1**        | **'const**'   |
| **1**        | **'const**'   |
| **1**        | **'const**'   |
| **1**        | **'const**'   |

*ADomain, nr of rows = 5*