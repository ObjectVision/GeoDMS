*[[String functions]] unquote*

## syntax

- unquote(*string_dataitem*)

## definition

unquote(*string_dataitem*) **removes single quotes** from the values of [[data item]] *string_dataitem*.

## applies to

data item *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; unquoteA (ADomain) := <B>unquote(</B>A<B>)</B>;
</pre>

| A                        | **unquoteA**           |
|--------------------------|------------------------|
| ***'Test***'             | **'Test**'             |
| ***'88hallo99***'        | **'88hallo99**'        |
| ***'+)***'               | **'+)**'               |
| ***'twee woorden***'     | **'twee woorden**'     |
| ***' test met spatie***' | **' test met spatie**' |

*ADomain nr of rows = 5*

## see also
- [[quote]]
- [[undquote]]