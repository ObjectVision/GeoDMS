*[[Conversion functions]] roundToZero*

## syntax

roundToZero(*a*)

## definition

roundToZero(*a*) results in a **integer data item rounded off towards zero** from [[data item]] *a*. Float32/64 data items are rounded off to the int32 [[value type]], f/dpoint data items to the ipoint value type.

## applies to

- data item with float32, float64, fpoint or dpoint value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundToZeroA (ADomain) := <B>roundToZero(</B>A<B>)</B>;
</pre>

| A     |**roundToZeroA**|
|------:|---------------:|
| 1.49  | **1**          |
| 1.5   | **1**          |
| -1.49 | **-1**         |
| -1.5  | **-1**         |
| -1.51 | **-1**         |

*ADomain, nr of rows = 5*

## see also

- [[roundToZero_64]]
- [[round]]
- [[roundUp]]
- [[roundDown]]