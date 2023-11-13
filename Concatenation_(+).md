*[[String functions]] concatenation (+)*

## syntax

- add(*string_dataitem_a*,*string_dataitem_b*)
- *string_dataitem_a* + *string_dataitem_b*

## definition

add(*string_dataitem_a*,*string_dataitem_b*) or *string_dataitem_a* + *string_dataitem_b* + ... results in the element-by-element [**concatenation**](https://en.wikipedia.org/wiki/Concatenation) of corresponding string values of [[arguments|argument]]: *string_dataitem_a* and *string_dataitem_b*.

## description

The [[+ operator|add]] is also in use for numeric additions. String concatenations differ from normal additions, they are not commutative (a + b is not equal to b + a).

## applies to

- [[data items|data item]] with string [[value type]]

## conditions

The [[domain units|domain unit]] of all arguments must match or be [[void]] (string [literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) and [[parameters|parameter]] can be concatenated with any string data item.

## example
<pre>
attribute&lt;string&gt; AandB (ADomain) := A <B>+</B> B;
</pre>

| A                  | B                   |**AandB**                            |
|--------------------|---------------------|-------------------------------------|
| 'Test'             | 'Test2'             | **'TestTest2**'                     |
| '88hallo99'        | null                | **null**                            |
| '+)'               | '-'                 | **'+)-**'                           |
| 'twee woorden'     | 'drie woorden test' | **'twee woordendrie woorden test**' |
| ' test met spatie' | ' _woord'           | **' test met spatie _woord'**       |

*ADomain, nr of rows = 5*

## see also

- [[add]]