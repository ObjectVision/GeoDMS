*[[Ordering functions]] not equals or one side has null values*

## syntax

- ne_or_one_null(*a*, *b*)

## definition

ne_or_one_null(*a*, *b*) results in a boolean [[data item]] indicating if the values of data item *a* are **not equal to** the corresponding values of data item *b* or if the corresponding **values of data items *a* or *b* are [[null]]**.

## description

The comparison between two missing values (null = null) results in the value False.

## applies to

Data items with Numeric, Point, or string [[value type]]

## conditions

1. [[Domain|domain unit]] of the [[arguments|argument]] must match or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be compared to data items of any domain).
2.  [[Arguments|argument]] must have matching:
    -   [[value type]]
    -   [[metric]]

## example

<pre>
attribute&lt;bool&gt; neAB (CDomain) := <B>ne_or_one_null(</B>A, B<B>)</B>;
</pre>

## see also

-   [[not equals|ne]]