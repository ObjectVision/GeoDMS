*[[Sequence functions]] int16Seq*

## syntax

- int16Seq(*a*)

## definition

int16Seq(*a*) results in a **sequence of 16 bits signed integers** derived from string [[data item]] *a*.

The syntax for string [[argument]] *a* need to be: {10: 41 9999 42 10 600 1 7 116 0 110}.

In this string:
- The curly brackets {..} indicate the start and end of the sequence.
- The first number (10) indicates the number of elements of the sequence followed by a colon. The elements of the sequence follow this colon, separated by spaces.

The [[composition type|composition]] need to be configured to poly. The [[Sequence2Points]] function can be used to make a pointset domain.

## applies to

- [[data item]] *a* with a string [[value type]]

## since version

7.130

## example
<pre>
parameter&lt;string&gt; param              := '{10: 41 9999 42 10 600 1 7 116 0 110}';
parameter&lt;int16&gt;  param_int16 (poly) := <B>int16Seq(</B>source/param<B>)</B>;
</pre>

| param_int16                            |
|----------------------------------------|
| **{10: 41 9999 42 10 600 7 116 0 110}**|
