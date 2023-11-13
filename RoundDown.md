*[[Conversion functions]] roundDown*

## syntax

roundDown(*a*)

## definition

roundDown(*a*) results in a **integer data item rounded off downwards** from [[data item]] *a*. Float32/64 data items are rouded off to the int32 [[value type]], f/dpoint data items to the ipoint value type.

## applies to

- data item with float32, float64, fpoint or dpoint value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundDownA (ADomain) := <B>roundDown(</B>A<B>)</B>;
</pre>

| A     |**roundDownA**|
|------:|-------------:|
| 1.49  | **1**        |
| 1.5   | **1**        |
| -1.49 | **-2**       |
| -1.5  | **-2**       |
| -1.51 | **-2**       |

*ADomain, nr of rows = 5*

## see also

- [[roundDown_64]]
- [[round]]
- [[roundUp]]
- [[roundToZero]]