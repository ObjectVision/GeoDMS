*[[Arithmetic functions]] divide (/)*

## syntax

-   div(*a*, *b*)
-   *a* / *b*

## definition

div(*a*, *b*) or a / b results in the element-by-element [**division**](http://en.wikipedia.org/wiki/Division_(mathematics)) of the values of [[data item]] *a* by the corresponding values of data item *b*. The resulting [[metric]] of the [[values unit]] is the [quotient](http://en.wikipedia.org/wiki/Quotient) of the [[metric]] of the [[arguments|argument]].

## description

A division by zero results in an undefined ([[null]]) value.

## applies to

Data items with Numeric or Point [[value type]]

[[Units|unit]] with Numeric value type

## conditions

1.  [[Domain units|domain unit]] of the arguments|argument must match or be [[void]] ([literals](http://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be divided by data items of any domain).
2.  Arguments must have matching:
    -   [[value type]]

## example

<pre>
1. attribute&lt;float32&gt; divAB (ADomain) := <B>div(</B>A, B<B>)</B>;    
2. attribute&lt;float32&gt; divAB (ADomain) := A <B>/</B> B;
</pre>

| A   | B    |**divAB**|
|----:|-----:|--------:|
| 0   | 1    | **0**   |
| 1   | -1   | **-1**  |
| -2  | 2    | **-1**  |
| 3.6 | 1.44 | **2.5** |
| 999 | 111  | **9**   |

*ADomain, nr of rows = 5*