*[[Logical functions]] bitor*

## syntax

- bitor(*a*,*b*)

## definition

bitor(*a*,*b*) results in the **logical or** of two boolean or integer [[data items|data item]] *a* and *b*. In this bitwise comparison, a true or 1 value is returned if any [[argument]] has a true or 1 value at the bit position compared and a false or 0 value if not.

The [[values unit]] of the resulting data item is the values unit of data item *a*.

## description

bitor is a bitwise operation.

## applies to

- data items with bool, (u)int8, (u)int16, (u)int32 or (u)int64 value type

## conditions

The [[domain unit]] and values unit of arguments *a* and *b* must match.

## example

<pre>
attribute&lt;uint8&gt; bitorAB (ADomain) := <B>bitor(</B>A, B<B>)</B>;
</pre>

| A    | B   | **bitorAB** |
|-----:|----:|------------:|
| 0    | 2   | **2**       |
| 1    | 2   | **3**       |
| 2    | 2   | **2**       |
| 3    | 2   | **3**       |
| null | 2   | **null**    |

*ADomain, nr of rows = 5*

## see also

- [[bitand]]
- [[complement]]
- [[or]] (||)