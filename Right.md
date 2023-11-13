*[[String functions]] right*

## syntax

- right(*string_dataitem*, *length*)

## definition

right(*string_dataitem*, *length*) results in a **substring** of *string_dataitem* with the number of characters of the *length* [[argument]], starting from the **right** of the *string_dataitem*.

## description

right(A, 3) is synonym for [[substr]](A, [[strlen(|strlen]]A) - 3, 3).

## applies to

- [[data item]] *string_dataitem* with string [[value type]]
- [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameter]] *length* with uint32 value type

## since version

7.155

## example

<pre>
attribute&lt;string&gt; rightA (ADomain) := <B>right(</B>A, 3<B>)</B>;
</pre>

| A                  |**rightA**       |
|--------------------|-----------------|
| 'Test'             | **'est**'       |
| '88hallo99'        | **'099**'       |
| '+)'               | ''''''**'+)''** |
| 'twee woorden'     | **'den**'       |
| ' test met spatie' | **'tie**'       |

*ADomain, nr of rows = 5*

## see also
- [[left]]
- [[substr]]