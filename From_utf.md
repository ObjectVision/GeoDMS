*[[String functions]] from_utf*

## syntax

- from_utf(*string_dataitem*)

## definition

from_utf(*string_dataitem*) results in the conversion from string [[argument]] *string_dataitem*, [utf8 encoded](https://en.wikipedia.org/wiki/UTF-8), to the [basic **ASCII** character set (first 128 codes)](https://en.wikipedia.org/wiki/ASCII).

The last column in the example presents the resulting character for each special character.

## description

The from_utf function can be used in string comparisons, for instance to compare addresses in which special characters can be ignored.

## applies to

[[data items|data item]] with string [[value type]]

## conditions

[[argument]] *string_dataitem* must be utf8 encoded (default).

## since version

7.130

## example

<pre>
attribute&lt;string&gt; from_utf (TextDomain) := <B>from_utf(</B>a<B>)</B>;
</pre>

|a|	b|	c|	d|	e|	f|	g| **from_utf**|
|-|------|-------|-------|-------|-------|-------|-------------|
|À|	Á|	Â|	Ä|	Ã|	Å|	Æ|        **A**|
|à|	á|	â|	ä|	ã|	å|	æ|	  **a**|
|Ç|	 |	 |	 |	 |	 |	 |        **C**|
|ç|	 |	 |	 |	 |	 |	 |	  **c**|
|È|	É|	Ê|	Ë|	 |	 |	 |	  **E**|
|è|	é|	ê|	ë|	 |	 |	 |	  **e**|
|Ì|	Í|	Î|	Ï|	 |	 |	 |	  **I**|
|ì|	í|	î|	ï|	ï|	 |	 |	  **i**|
|Ñ|      |	 |	 |	 |	 |	 |        **N**|
|ñ|	 |	 |	 |	 |	 |	 |	  **n**|
|Ò|	Ó|	Ô|	Ö|	Õ|	Ø|	Œ|	  **O**|
|ò|	ó|	ô|	ö|	õ|	ø|	œ|	  **o**|
|Ù|	Ú|	Û|	Ü|	 |	 |	 |	  **U**|
|ù|	ú|	û|	 |	 |	 |	 |     	  **u**|
|Ý|	 |	 |	 |	 |	 |	 |        **Y**|
|ý|	ÿ|	 |	 |	 |	 |	 |	  **y**|

*TextDomain, nr of rows = 16*