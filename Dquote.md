*[[String functions]] d(ouble)quote*

## syntax

- dquote(*string_dataitem*)

## definition

dquote(*string_dataitem*) **double quotes** the values of [[data item]] *string_dataitem*.

## applies to

data item *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; dquoteA (ADomain) := <B>dquote(A)</B>;
</pre>

| A                  |  **dquoteA**             |
|--------------------|--------------------------|
| 'Test'             | **'"Test"**'             |
| '88hallo99'        | **'"88hallo99"**'        |
| '+)'               | **'"+)"**'               |
| 'twee woorden'     | **'"twee woorden"**'     |
| ' test met spatie' | **'" test met spatie"**' |

*ADomain, nr of rows = 5*

## see also

- [[quote]]
- [[undquote]]