*[[Arithmetic functions]] subtract(-)*

## syntax

-   sub(*a, b*)
-   *a* - *b*

## definition

sub(*a*, *b*) or a - b results in the element-by-element [**subtraction**](http://en.wikipedia.org/wiki/Subtraction) of the values of [[data item]] *b* from the corresponding values of data item *a*.

## applies to

Data items with Numeric or Point [[Value Type]]

## conditions

1.  [[Domain units|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](http://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be subtracted from data items of any domain).
2.  [[Arguments|argument]] must have matching:
    -   [[value type]]
    -   [[metric]]

## example

<pre>
1. attribute&lt;float32&gt; AminB (ADomain) := <B>sub</B>(A, B);
2. attribute&lt;float32&gt; AminB (ADomain) := A <B>-</B> B;
</pre>

| A   | B    |**AminB** |
|----:|-----:|---------:|
| 0   | 1    | **-1**   |
| 1   | -1   | **2**    |
| -2  | 2    | **-4**   |
| 3.6 | 1.44 | **2.16** |
| 999 | 111  | **888**  |

*ADomain, nr of rows = 5*

## See also

-   [[Sub (polygon difference)|Sub_(difference)]]