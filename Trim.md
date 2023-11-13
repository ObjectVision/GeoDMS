*[[String functions]] trim*

## syntax

- trim(*string_dataitem*)

## definition

trim(*string_dataitem*) **removes space characters** before the first and after the last non space character in *string_dataitem*.

## applies to

[[data item]] *string_dataitem* with string [[value type]]

## example

<pre>
attribute&lt;string&gt; trimA (ADomain) := <B>trim(</B>A<B>)</B>;
</pre>

| A                  |  **trimA**            |
|--------------------|-----------------------|
| 'Test'             | **'Test**'            |
| '88hallo99'        | **'88hallo99**'       |
| '+)'               | **'+)**'              |
| 'twee woorden'     | **'twee woorden**'    |
| ' test met spatie' | **'test met spatie**' |

*ADomain, nr of rows = 5*

## see also

- [[ltrim]]
- [[rtrim]]