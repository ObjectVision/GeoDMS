*[[Conversion functions]] roundDown_64*

## syntax

roundDown_64(*a*)

## definition

roundDown_64(*a*) results in a **integer data item rounded off downwards** from [[data item]] *a*. Float32/64 data items are rounded off to the int64 [[value type]].

## applies to

- [[data item]] with float32 or float64 value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundDown_64A (ADomain) := <B>roundDown_64(</B>A<B>)</B>;
</pre>

| A     |**roundDown_64A**|
|------:|----------------:|
| 1.49  | **1**           |
| 1.5   | **1**           |
| -1.49 | **-2**          |
| -1.5  | **-2**          |
| -1.51 | **-2**          |

*ADomain, nr of rows = 5*

## see also

- [[roundDown]]