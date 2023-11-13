*[[Logical functions]] iif ( ? : )*

## syntax

- iif(*condition*, *then*, *else*)
- *condition* ? *then* : *else*

## definition

iif(*condition*, *then*, *else*) or *condition* ? *then* : *else* results in a [[data item]] with the result of the [[subexpression]] **then if the condition is true and of else if the condition is false**.

## description

The iif function results in a data item with the same [[domain unit]] and [[values unit]] as the subexpressions *then* and *else*.

iif statements can be nested, but it is advised to use the [[switchcase]] function for nested statements.

In the evaluation of an iif function, both *then* and *else* subexpressions are calculated. Use an [[indirect expression]] if the calculation of one of both subexpressions is time consuming or illogical.

## applies to

- *condition* data item with bool [[value type]]
- *subexpressions* *then* and *else*

## conditions

1. The domain unit of *condition*, subexpressions *then* and *else* and the resulting data item must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be combined with data items of any domain).
2. The values unit of subexpressions *then* and *else* and the resulting data item must match.

## example

<pre>
1. attribute&lt;float32&gt; iifAgteB (ADomain) := <B>iif(</B>A &gt;= B, 1f, 0f<B>)</B>;
2. attribute&lt;float32&gt; iifAgteB (ADomain) := A &gt;= B <B>?</B> 1f <B>:</B> 0f;
</pre>

| A(float32) | B(float32) | iifAgteB |
|-----------:|-----------:|---------:|
| 0          | 2          | **0**    |
| 1          | 2          | **0**    |
| 2          | 2          | **1**    |
| 3          | 2          | **1**    |
| null       | 2          | **0**    |

*ADomain, nr of rows = 5*

## see also

- [[switchcase]]