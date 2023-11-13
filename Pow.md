*[[Arithmetic functions]] power (^)*

## syntax

-   pow(*base*, *exp*)
-   *base*^*exp*

## definition

pow(*base*, *exp*) or *base*^*exp* results in the element-by-element [**exponentiation**](https://en.wikipedia.org/wiki/Exponentiation) of the base values of [[data item]] *base* to the corresponding exponent values of data item *exp*.

## description

The power operator can not (yet) be used associative, [[expressions|expression]] as: a^b^c are not allowed, use (a^b)^c instead.

## applies to

Data items with float32 or float 64 [[value type]]

## conditions

1.  [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(mathematical_logic)) or [[parameters|parameter]] can be calculated with data items of any domain).
2.  [[Arguments|argument]] must have matching:
    -   [[value type]]

## example

<pre>
1. attribute&lt;float32&gt; powAB (ADomain) := <B>pow(</B>A, B<B>)</B>;
2. attribute&lt;float32&gt; powAB (ADomain) := A<B>^</B>B;
</pre>

| A   | B   |**powAB** |
|----:|----:|---------:|
| 0   | 1   | **0**    |
| 1   | 1   | **1**    |
| 4   | 2   | **16**   |
| 7   | 3   | **343**  |
| -5  | -2  | **0.04** |

*ADomain, nr of rows = 5*