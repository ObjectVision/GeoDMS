*[[String functions]] replace*

## syntax

- replace(*source_string_dataitem*, *old_substring*, *new_substring*)

## definition

replace(*source_string_dataitem*, *old_substring*, *new_substring*) **replaces *old_substring*** in the *source_string_dataitem* **with *new_substring***.

## description

The replace function replaces full and substrings in *source_string_dataitem*. The [[replace_value]] replaces only string values that fully matches.

The replace function is [case sensitive](https://en.wikipedia.org/wiki/Case_sensitivity).

Multiple replacements can be combined in the replace function with the symtax: replace(*source_string_dataitem*, *old_substring1*, *new_substring1*, *old_substring2*, *new_substring2*).

Replacements are executed according to their configuration sequence, so in the example first *old_substring1* by *new_substring1*, then *old_substring2* by *new_substring2*.

## applies to

- [[data items|data item]] *source_string_dataitem*, *old_substring*, *new_substring* with string [[value types|value type]]

## conditions

The [[domain unit]] of all [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain unit).

## example

<pre>
attribute&lt;string&gt; replaceA (ADomain) := <B>replace(</B>A, 'Tes', 'Taar'<B>)</B>;
</pre>

| A               |  **replaceA**        |
|-----------------|----------------------|
| 'Test'          | **'Taart**'          |
| 'test'          | **'test**'           |
| '88hallo99'     | **'88hallo99**'      |
| 'Test met Text' | **'Taart met Text**' |
| 'Text met Test' | **'Text met Taart**' |

*ADomain, nr of rows = 5*

## see also

- [[replace_value]]
- [[regex_replace]]