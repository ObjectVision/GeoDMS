*[[String functions]] sort_str(ing)*

## syntax

- sort_str(*string_dataitem*)

## definition

sort_str(*string_dataitem*) **sorts** [[data item]] *string_dataitem* in **ascending order** (ASCII sort sequence).

## description

The sort_str function is [case sensitive](https://en.wikipedia.org/wiki/Case_sensitivity).

Use the [[reverse]] function on a sorted string to sort in descending order (see example).

## applies to

- data item *string_dataitem* with string [[value type]]

## example

<pre>
1. attribute&lt;string&gt; sortA_asc  (ADomain) := <B>sort_str(</B>A<B>)</B>;
2. attribute&lt;string&gt; sortA_desc (ADomain) := <B>reverse(</B>sortA_asc<B>)</B>;
</pre>

| A                  | **sortA_asc**          | **srtA_desc**          |
|--------------------|------------------------|------------------------|
| 'Test'             | **' test met spatie**' | **'twee woorden'**     |
| '88hallo99'        | **'+)**'               | **'Test'**             |
| '+)'               | **'88hallo99**'        | **'88hallo99'**        |
| 'twee woorden'     | **'Test**'             | **'+)'**               |
| ' test met spatie' | **'twee woorden**'     | **' test met spatie'** |

*ADomain, nr of rows = 5*

## see also
- [[sort]]
- [[reverse]]