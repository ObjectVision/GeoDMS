*[[String functions]] UpperCase*

## syntax

- UpperCase(*string_dataitem*)

## definition

UpperCase(*string_dataitem*) translates all lowercase characters of [[data item]] *string_dataitem* to **uppercases**.

## applies to

data item *string_dataitem* with string [[value type]]

## since version

5.90

## example

<pre>
attribute&lt;string&gt; UpperCaseA (ADomain) := <B>UpperCase(</B>A<B>)</B>;
</pre>

| A                  |  **UpperCaseA**        |
|--------------------|------------------------|
| 'Test'             | **'TEST**'             |
| '88hallo99'        | **'88HALLO99**'        |
| '+)'               | **'+)**'               |
| 'twee woorden'     | **'TWEE WOORDEN**'     |
| ' test met spatie' | **' TEST MET SPATIE**' |

*ADomain, nr of rows = 5*

## see also

- [[LowerCase]]