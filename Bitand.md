*[[Logical functions]] bitand*

## syntax

- bitand(*a*,*b*)

## definition

bitand(*a*,*b*) results in the **logical and** of two boolean or integer [[data items|data item]] *a* and *b*. In this bitwise comparison, a true or 1 value is returned if both [[arguments|argument]] have a true or 1 value at the bit position compared and a false or 0 value if not.

The [[values unit]] of the resulting data item is the values  unit of data item *a*.

## description

bitand is a bitwise operation.

## applies to

- data items with bool, (u)int8, (u)int16, (u)int32 or (u)int64 [[value type]]

## conditions

The [[domain unit]] and values unit of arguments *a* and *b* must match.

## example

<pre>
attribute&lt;uint8&gt; bitandAB (ADomain) := <B>bitand(</B>A, B<B>)</B>;
</pre>

| A    | B   |**bitandAB**|
|-----:|----:|-----------:|
| 0    | 2   | **0**      |
| 1    | 2   | **0**      |
| 2    | 2   | **2**      |
| 3    | 2   | **2**      |
| null | 2   | **null**   |

*ADomain, nr of rows = 5*

## see also

- [[bitor]]
- [[complement]]
- [[and]] (&&)