*[[Conversion functions]] round_64*

## syntax

round_64(*a*)

## definition

round_64(*a*) results in a **integer data item rounded off** from [[data item]] *a*. Float32/64 data items are round_64ed off to the int64 [[value type]].

## applies to

- data item with float32 or float64 value type

## since version

5.45

## example

<pre>
attribute&lt;int32&gt; round_64A (ADomain) := <B>round_64(</B>A<B>)</B>;
</pre>

| A     |**round_64A**|
|------:|------------:|
| 1.49  | **1**       |
| 1.5   | **2**       |
| -1.49 | **-1**      |
| -1.5  | **-1**      |
| -1.51 | **-2**      |

*ADomain, nr of rows = 5*

## see also

- [[round]]