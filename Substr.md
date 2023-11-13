*[[String functions]] substr(ing)*

## syntax

1. substr(*string_dataitem*, *startposition*, *length*)
2. substr(*string_dataitem*, *startposition*)

## definition

1. substr(*string_dataitem*, *startposition*, *length*) results in a **substring** of *string_dataitem* with the number of characters of the *length* [[argument]], starting from the *startposition* argument.
2. substr(*string_dataitem*, *startposition*) results in a **substring** of *string_dataitem* with all characters from the *startposition* argument.

## applies to

- [[data item]] *string_dataitem* with string [[value type]]
- [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameter]] *startposition* with uint32 value type
- literal or parameter *length* with uint32 value type

## since version

1. substr(*string_dataitem*, *startposition*, *length*) : 5.15
2. substr(*string_dataitem*, *startposition*) : 7.142

## example

<pre>
attribute&lt;string&gt; substrA  (ADomain) := <B>substr(</B>A, 1, 3<B>)</B>;
attribute&lt;string&gt; substr2A (ADomain) := <B>substr(</B>A, 1<B>)</B>;
</pre>

| A                  | **substrA** | **substr2A**          |
|--------------------|-------------|-----------------------|
| 'Test'             | **'est**'   | **'est**'             |
| '88hallo99'        | **'8ha**'   | **'8hallo99**'        |
| '+)'               | **')**'     | **')**'               |
| 'twee woorden'     | **'wee**'   | **'wee woorden**'     |
| ' test met spatie' | **'tes**'   | **'test met spatie**' |

*ADomain, nr of rows = 5*

## see also

- [[left]]
- [[right]]