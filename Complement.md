*[[Logical functions]] complement*

## syntax

-   complement(*a*)

## definition

complement(*a*) results in the **logical negation** of the boolean or integer [[data item]] *a*. In this bitwise operation, a true or 1 value is returned if the original bit value was false or 0 and vice versa.

The [[values unit]] of the resulting data item is the values unit of data item *a*.

## description

complement is a bitwise operation.

## applies to

- data item with bool, (u)int8, (u)int16, (u)int32 or (u)int64 [[value type]]

## since version

7.023

## example

<pre>
attribute&lt;uint8&gt; complementA (ADomain) := <B>complement(</B>A<B>)</B>;
</pre>