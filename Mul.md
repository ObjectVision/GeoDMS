*[[Arithmetic functions]] multiplication (*)*

## syntax

-   mul(*a*, *b*, ...)
-   *a* \* *b* \* ...

## definition

mul(*a*,*b*, ...) or *a* \* *b* \* ... results in the element-by-element [**multiplication**](http://en.wikipedia.org/wiki/Multiplication) of corresponding values of the [[data items|data item]]: *a*, *b*, ...

## applies to

Data items with Numeric or Point [[Value Type]]

[[Units|unit]] with Numeric Value Type

## conditions

1.  [[Domain units|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](http://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be added to data items of any domain).
2.  Arguments must have matching:
    -   [[value type]]

## example

<pre>
1. attribute&lt;float32&gt; mulABC (ADomain) := <B>mul(</B>A, B, C<B>)</B>    
2. attribute&lt;float32&gt; mulABC (ADomain) := A <B>*</B> B <B>*</B> C;
</pre>

| A   | B    | C   | **mulABC**  | 
|----:|-----:|----:|------------:|
| 0   | 1    | 0   | **0**       |
| 1   | -1   | 1   | **-1**      |
| -2  | 2    | 4   | **-16**     |
| 3.6 | 1.44 | 7   | **36.29**   |
| 999 | 111  | -5  | **554,445** |

*ADomain, nr of rows = 5*