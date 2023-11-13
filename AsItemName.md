*[[String functions]] AsItemName*

## syntax

- *AsItemName*(*string_dataitem*)

## definition

AsItemName(*string_dataitem*) results in the conversion from string [[argument]] *string_dataitem* to a valid [[tree item name]].

Invalid characters (including spaces) are replaced by underscores. Multiple underscores behind each other are replaced by one underscore.

If the string starts with a numeric character, an underscore is used as first character.

## applies to

[[data items|data item]] with string [[value type]]

## conditions

[[argument]] *string_dataitem* must be [utf8 encoded](https://en.wikipedia.org/wiki/UTF-8) (default).

## since version

7.412

## example

<pre>
parameter&lt;string&gt; ValidItemName := <B>AsItemName(</B>a<B>)</B>;
</pre>

| a              | **ValidItemName**     |
|----------------|-----------------------|
| '12&e €b naam' | **'_12_e_€b_naam**'   |