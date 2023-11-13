*[[String functions]] replace_value*

## syntax

- replace_value(*source_string_dataitem*, *old_string*, *new_string*)

## definition

replace_value(*source_string_dataitem*, *old_string*, *new_string*) **replace_values *old_string*** in the *source_string_dataitem* **with *new_string***.

## description

The replace_value function replaces only string values that fully matches. The [[replace]] function replaces full and substrings in *source_string_dataitem*.

The replace_value function is [case insensitive](https://en.wikipedia.org/wiki/Case_sensitivity)

## applies to

- [[data items|data item]] *source_string_dataitem*, *old_string*, *new_string* with string [[value type]]

## conditions

The [[domain unit]] of all [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain unit).

## example

<pre>
attribute&lt;string&gt; replace_valueA (ADomain) := <B>replace_value(</B>A, 'Tes', 'Taart'<B>)</B>;
</pre>

| A               | **replace_valueA**  |
|-----------------|---------------------|
| 'Tes'           | **'Taart**'         |
| 'tes'           | **'Taart**'         |
| '88hallo99'     | **'88hallo99**'     |
| 'Test met Text' | **'Test met Text**' |
| 'Tes met Test'  | **'Tes met Test**'  |

*ADomain, nr of rows = 5*

## see also

- [[replace]]
- [[regex_replace]]