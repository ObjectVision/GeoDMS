*[[Aggregation functions]] all*

## syntax

- all(*a*)
- all(*a*, *relation*)

## definition

- all(*a*) results in a boolean [[parameter]], with the value True if **all** values of boolean [[data item]] *a* are True and the value False in all other cases.
- all(*a*, *relation*) results in a boolean [[attribute]], with the values True if **all** values of boolean data item *a* are True and the values False in all other cases, grouped by *[[relation]]*. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## applies to

- attribute *a* with bool [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

The domain unit of [[arguments|argument]] *a* and *relation* must match.

## example

<pre>
parameter&lt;bool&gt; allA := <B>all(</B>A<B>)</B>; result = True
parameter&lt;bool&gt; allB := <B>all(</B>B<B>)</B>; result = False
parameter&lt;bool&gt; allC := <B>all(</B>C<B>)</B>; result = False
</pre>

| **A**    | **B**     | **C**     |
|----------|-----------|-----------|
| **True** | **True**  | **False** |
| **True** | **False** | **False** |
| **True** | **True**  | **False** |
| **True** | **False** | **False** |
| **True** | **True**  | **False** |

*ADomain, nr of rows = 5*

## see also

- [[any]]