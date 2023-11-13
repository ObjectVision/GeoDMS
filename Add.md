*[[Arithmetic functions]] addition (+)*

## syntax

-   add(*a, b, ...*) or add_list(*a, b, ...*)
-   *a* + *b* + ...

## definition

add(*a, b, ...*) or *a* + *b* + ... results in the element-by-element [**addition**](http://en.wikipedia.org/wiki/Addition) of corresponding values of the [[data items|data item]]: *a*, *b*, ... .. 

add_list is a synonym for add

## applies to

Data items with Numeric, Point, or String [[value type]].

## conditions

1.  [[Domain units|domain unit]] of the [[arguments|argument]] must match or be [[void]], ([literals](http://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be added to data items of any domain).
2.  Arguments must have matching:
    -   value type
    -   metric

## example

<pre>
1. attribute&lt;float32&gt; addABC (ADomain) := <B>add(</B>A, B, C<B>)</B>;
2. attribute&lt;float32&gt; addABC (ADomain) := A <B>+</B> B <B>+</B> C;
</pre>

| A   | B    | C   | **addABC**|
|----:|-----:|----:|----------:|
| 0   | 1    | 0   | **1**     |
| 1   | -1   | 1   | **1**     |
| -2  | 2    | 4   | **4**     |
| 3.6 | 1.44 | 7   | **12.04** |
| 999 | 111  | -5  | **1105**  |

*ADomain, nr of rows = 5*

## see also

-   [[string concatenation|concatenation_(+)]]
-   [[add polygon|Add (union)]]