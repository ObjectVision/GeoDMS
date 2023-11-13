*[[String functions]] repeat*

## syntax

- repeat(*string_dataitem*, *number*)

## definition

repeat(*string_dataitem*, *number*) **repeats** *number* (of) times the values of *string_dataitem*.

## applies to

- [[data item]] *string_dataitem* with string [[value type]]
- data item *number* with uint32 value type

## conditions

The [[domain unit]] of data items *string_dataitem* and *number* must match ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain unit).

## since version

5.35

## example

<pre>
attribute&lt;string&gt; repeatA (ADomain) := repeat<B>(</B>A, nr<B>)</B>;
</pre>

| A   | nr  |**repeatA** |
|-----|----:|------------|
| '0' | 0   |            |
| '1' | 1   | **'1**'    |
| '2' | 2   | **'22**'   |
| '3' | 3   | **'333**'  |
| '4' | 4   | **'4444**' |

*ADomain, nr of rows = 5*