*[[Conversion functions]] round*

## syntax

round(*a*)

## definition

round(*a*) results in a **integer data item rounded off** from [[data item]] *a*. Float32/64 data items are rounded off to the int32 [[value type]], f/dpoint data items to the ipoint value type.

## applies to

- data item with float32, float64, fpoint or dpoint value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundA (ADomain) := <B>round(</B>A<B>)</B>;
</pre>

| A     |**roundA**|
|------:|---------:|
| 1.49  | **1**    |
| 1.5   | **2**    |
| -1.49 | **-1**   |
| -1.5  | **-1**   |
| -1.51 | **-2**   |

*ADomain, nr of rows = 5*

## see also

- [[round_64]]
- [[roundUp]]
- [[roundDown]]
- [[roundToZero]]