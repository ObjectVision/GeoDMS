*[[Matrix functions]] matr(ix)_(co)var(iance)*

## syntax

- matr_var(*point_att*, *result_domain*)

## definition

The matr_var(*point_att*, *result_domain*) function calculates the **[covariance matrix](https://en.wikipedia.org/wiki/Covariance_matrix)** of the *point_att*, with as resulting domain: *result_domain*.

The [[value type]] of the resulting [[attribute]] is the same as the value type of the *point_att*.

## applies to

- attribute *point_att* with float32 or float64 value type

## conditions

1. The value type of *point_att* and of the result attribute need to match.
2. The [[domain unit]] of the *point_att* attribute must be a Point value type of the group CanBeDomainUnit.
3. The *result_domain* [[argument]] must be a [[unit]] of with a Point value type of the group CanBeDomainUnit and match with the result of the matrix covariance calculation in terms of number or rows/columns.

## since version

7.184

## example

<pre>
attribute&lt;float32&gt; covariance (GridDomain) := <B>matr_var(</B>src, GridDomain<B>)</B>;
</pre>

src
|   |   |
|--:|--:|
| 1 | 2 |
| 3 | 4 |

*GridDomain, nr of rows = 2, nr of cols = 2*

 **covariance**
|        |         |
|-------:|--------:|
| **10** | **14**  |
| **14** | **200** |

*GridDomain, nr of rows = 2, nr of cols = 2*