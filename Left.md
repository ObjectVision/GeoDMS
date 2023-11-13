*[[String functions]] left*

## syntax

- left(*string_dataitem*, *length*)

## definition

left(*string_dataitem*, *length*) results in a **substring** of *string_dataitem* with the number of characters of the *length* [[argument]], starting from the **left** of the *string_dataitem*.

## description

left(A, 3) is synonym for [[substr]](A, 0, 3).

## applies to

- [[data item]] *string_dataitem* with string [[value type]]
- [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameter]] *length* with uint32 value type

## since version

7.155

## example

<pre>
attribute&lt;string&gt; leftA (ADomain) := <B>left(</B>A, 3<B>)</B>;
</pre>

| A                  |    **leftA**    |
|--------------------|-----------------|
| 'Test'             | **'Tes**'       |
| '88hallo99'        | **'88h**'       |
| '+)'               | ''''''**'+)''** |
| 'twee woorden'     | **'twe**'       |
| ' test met spatie' | ' **te**'       |

*ADomain, nr of rows = 5*

## see also

- [[right]]
- [[substr]]