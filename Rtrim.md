*[[String functions]] rtrim*

## syntax

- rtrim(*string_dataitem*)

## definition

rtrim(*string_dataitem*) **removes space characters** after the last non space character in *string_dataitem*.

## applies to

[[data item]] *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; rtrimA (ADomain) := <B>rtrim(</B>A<B>)</B>;
</pre>

| A                  | **rtrimA**             |
|--------------------|------------------------|
| 'Test '            | **'Test**'             |
| '88hallo99'        | **'88hallo99**'        |
| '+)'               | **'+)**'               |
| 'twee woorden'     | **'twee woorden**'     |
| ' test met spatie' | ' **test met spatie**' |

*ADomain, nr of rows = 5*

## see also

- [[trim]]
- [[ltrim]]