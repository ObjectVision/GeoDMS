*[[Conversion functions]] roundUp*

## syntax

roundUp(*a*)

## definition

roundUp(*a*) results in a **integer data item rounded off upwards** from [[data item]] *a*. Float32/64 data items are rounded off to the int32 [[value type]], f/dpoint data items to the ipoint value type.

## applies to

- [[data item]] with float32, float64, fpoint or dpoint [[value type]]

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundUpA (ADomain) := <B>roundUp(</B>A<B>)</B>;
</pre>

| A     |**roundUpA**|
|------:|-----------:|
| 1.49  | **2**      |
| 1.5   | **2**      |
| -1.49 | **-1**     |
| -1.5  | **-1**     |
| -1.51 | **-1**     |

*ADomain, nr of rows = 5*

## see also

- [[roundUp_64]]
- [[round]]
- [[roundDown]]
- [[roundToZero]]