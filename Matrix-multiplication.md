*[[Matrix functions]] matr(ix)_mul(tiplication)*

## syntax

-   matr_mul(*left_att*, *right_att*, *result_domain*)

## definition

The matr_mul(*left_att*, *right_att*, *result_domain*) function calculates the **[matrix product](https://en.wikipedia.org/wiki/Matrix_multiplication)** of the *left_att* and *right_att* [[attributes|attribute]] with as resulting [[domain unit]]: *result_domain*.

The [[value type]] of the resulting attribute is the the value type of *left_att* and *right_att* [[arguments|argument]].

## applies to

- attributes *left_att* and *right_att* with float32 or float64 value type

## conditions

1. The value type of *left_att*, *right_att* and resulting attribute need to match.
2. The domain unit of the *left_att*, *right_att* attribute must be a Point value type of the group CanBeDomainUnit.
3. The *result_domain* argument must be a [[unit]] of with a Point value type of the group CanBeDomainUnit and match with the result of the matrix product calculation in terms of number or rows/columns.

## since version

7.184

## example
<pre>
attribute&lt;float32&gt; product (GridDomain) := <B>matr_mul(</B>left, right, GridDomain<B>)</B>;
</pre>

left
|   |   |
|--:|--:|
| 1 | 2 | 
| 3 | 4 |

*GridDomain, nr of rows = 2, nr of cols = 2*

right
|     |     |
|----:|----:|
| 100 | 200 |
| 300 | 400 |

*GridDomain, nr of rows = 2, nr of cols = 2*

 **product**                 
|                                  |                                  |
|---------------------------------:|---------------------------------:|
| <B>  700 (1 * 100 + 2 * 300)</B> | <B>1.000 (1 * 200 + 2 * 400)</B> |
| <B>1.500 (3 * 100 + 4 * 300)</B> | <B>2.200 (3 * 200 + 4 * 400)</B> |

*GridDomain, nr of rows = 2, nr of cols = 2*