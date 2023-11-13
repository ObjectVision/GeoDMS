*[[Ordering functions]] reverse*

## syntax

reverse(*a*)

## definition

reverse(a) reverses the sequence of the values of [[attribute]] *a*. The function results in a new attribute with the same [[values unit]] and [[domain unit]] as attribute *a* and with the values of attribut *a* in reversed order.

## applies to

attributes with Numeric, Point or bool [[value type]]

## example

<pre>
attribute&lt;float32&gt; reverseA (ADomain) := reverse(A);
</pre>

| A    |**reverseA**|
|-----:|-----------:|
| 0    | **100**    |
| 1    | **null**   |
| 2.5  | **0**      |
| -100 | **null**   |
| 999  | **null**   |
| null | **999**    |
| null | **-100**   |
| 0    | **2.5**    |
| null | **1**      |
| 100  | **0**      |

*ADomain, nr of rows = 10*

## see also

- [[sort]]
- [[sort str]]