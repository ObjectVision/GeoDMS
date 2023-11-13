*[[String functions]] quote*

## syntax

- quote(*string_dataitem*)

## definition

quote(*string_dataitem*) **single quotes** the values of [[data item]] *string_dataitem*.

## applies to

data item *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; quoteA (ADomain) := <B>quote(</B>A<B>)</B>;
</pre>

| A                  | **quoteA**                  |
|--------------------|-----------------------------|
| 'Test'             | ''****Test****''            |
| '88hallo99'        | ''****88hallo99****''       |
| '+)'               | ''****+)****''              |
| 'twee woorden'     | ''****twee woorden****''    |
| ' test met spatie' | ''****test met spatie****'' |

*ADomain nr of rows = 5*

## see also
- [[unquote]]
- [[dquote]]