*[[Conversion functions]] rgb*

## syntax

-   rgb(*red*, *green*, *blue*)

## definition

rgb(*red*, *green*, *blue*) function results in a **uint32 rgb color value** with the [[data items|data item]]: *red*, *green* and *blue* representing the red, green and blue aspects of the rgb value.

## applies to

- data items with (u)int8, (u)int16 or (u)int32 [[value type]]

## conditions

1. data items *red*, *green* and *blue* need to match with regard to their [[domain unit]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be used with data items of any domain).

## example

<pre>
parameter&lt;uint8&gt;  red   := uint8(254);
parameter&lt;uint8&gt;  green := uint8(128);
parameter&lt;uint8&gt;  blue  := uint8(  0);
parameter&lt;uint32&gt; rgb   := <B>rgb(</B>red, green, blue<B>)</B>;
</pre>

*result = parameter rgb with rgb value: 254,128,0 (orange)*