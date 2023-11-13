*[[String functions]] und(ouble)quote*

## syntax

- undquote(*string_dataitem*)

## definition

undquote(*string_dataitem*) **removes double quotes** from the values of [[data item]] *string_dataitem*.

## applies to

data item *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; undquoteA (ADomain) := <B>undquote(</B>A<B>)</B>;
</pre>

| A                    | **unquoteA**           |
|----------------------|------------------------|
| '"Test"'             | **'Test**'             |
| '"88hallo99"'        | **'88hallo99**'        |
| '"+)"'               | **'+)**'               |
| '"twee woorden"'     | **'twee woorden**'     |
| '" test met spatie"' | **' test met spatie**' |

*ADomain, nr of rows = 5*

## see also

- [[dquote]]
- [[unquote]]