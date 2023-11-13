*[[Conversion functions]] roundUp_64*

## syntax

roundUp_64(*a*)

## definition

roundUp_64(*a*) results in a **integer data item rounded off upwards** from [[data item]] *a*. Float32/64 data items are rounded off to the int64 [[value type]].

## applies to

- data item with float32 or float64 value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; roundUp_64A (ADomain) := <B>roundUp_64(</B>A<B>)</B>;
</pre>

| A     |**roundUp_64A**|
|------:|--------------:|
| 1.49  | **2**         |
| 1.5   | **2**         |
| -1.49 | **-1**        |
| -1.5  | **-1**        |
| -1.51 | **-1**        |

*ADomain, nr of rows = 5*

## see also

- [[roundUp]]