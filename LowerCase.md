*[[String functions]] LowerCase*

## syntax

- LowerCase(*string_dataitem*)

## definition

LowerCase(*string_dataitem*) translates all uppercase characters of [[data item]] *string_dataitem* to **lowercases**.

## applies to

data item *string_dataitem* with string [[value type]]

## since version

5.90

## example

<pre>
attribute&lt;string&gt; LowerCaseA (ADomain) := <B>LowerCase(</B>A<B>)</B>;
</pre>

| A                  | **LowerCaseA**         |
|--------------------|------------------------|
| 'Test'             | **'test**'             |
| '88hallo99'        | **'88hallo99**'        |
| '+)'               | **'+)**'               |
| 'twee woorden'     | **'twee woorden**'     |
| ' test met spatie' | **' test met spatie**' |

*ADomain, nr of rows = 5*

## see also

- [[UpperCase]]