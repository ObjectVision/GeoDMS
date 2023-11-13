*[[Logical functions]] switchcase*

## syntax

- switch(*case1*(*condition*, *value*),..,*casen*(*condition*, *value*), *othervalue*)

## definition

Switch case results in **different values depending on conditions**, specified as different cases. Each case is specified with the syntax: case(*condition*, *value*). Cases are separated by commas.

The last specified value (without condition) must be configured for the values that do not meet any condition (case other).

## description

The switch case function is comparable to the select case function in Visual Basic.

Use the [[iif]] function if only one case (and the case other) is needed.

## applies to

- *condition* [[data item]] with bool [[value type]]
- *value* [[subexpressions|subexpression]]

## conditions

1.  The [[domain unit]] of the *conditions*, *values* and the resulting data item must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be combined with data items of any domain).
2.  The [[values unit]] of the *values* and the resulting data item must match.

## example

<pre>
attribute&lt;string&gt; switchA (ADomain) :=
   <B>switch(
        case(</B>Source/floatA <  Source/floatB, 'A < B'<B>)
      , case(</B>Source/floatA >  Source/floatB, 'A > B'<B>)
      , case(</B>Source/floatA == Source/floatB, 'A = B'<B>)
      ,</B> 'Missing'
   <B>)</B>;
</pre>

| A(float32) | B(float32) | **switchA**   |
|-----------:|-----------:|---------------|
| 0          | 2          | **'A \< B**'  |
| 1          | 2          | **'A \< B**'  |
| 2          | 2          | **'A = B**'   |
| 3          | 2          | **'A \> B**'  |
| null       | 2          | **'Missing**' |

*ADomain, nr of rows = 5*

## see also

- [[iif]] ( ? : )