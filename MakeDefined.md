*[[Predicates functions]] MakeDefined*

## syntax

- MakeDefined(*a*, *value*)

## definition

MakeDefined(*a*, *value*) defines the [[argument]] ***value* (often zero) for the [[null]]** values in [[data item]] *a*.

## applies to

- data items with Numeric, Point or string [[value type]]

## conditions

1. The [[values unit]] of the arguments *a* and *value* must match.
2. The [[domain unit]] of the arguments must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be calculated with data items of any domain).

## example

<pre>
attribute&lt;float32&gt; MakeDefinedA (ADomain) := <B>MakeDefined(</B>A, 0f<B>)</B>;
</pre>

| A(float32) |**MakeDefinedA** |
|-----------:|----------------:|
| 0          | **0**           |
| null       | **0**           |
| 1000000    | **1000000**     |
| -2.5       | **-2.5**        |
| 99.9       | **99.9**        |

## see also

- [[IsDefined]]