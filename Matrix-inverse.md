*[[Matrix functions]] matr(ix)_inv(erse)*

## syntax

- matr_inv(*point_att*)

## definition

The matr_inv(*point_att*) function calculates the **[invertible matrix](https://en.wikipedia.org/wiki/Invertible_matrix)** of the *point_att* [[argument]] with the same resulting [[domain unit]] as of the *point_att* argument.

The [[value type]] of the resulting [[attribute]] is the same as the value type of the *point_att* argument.

## applies to

- attribute *point_att* with float32 or float64 value type

## conditions

1. The value type of *point_att* and of the resulting attribute need to match.
2. The domain unit of *point_att* and the resulting attribute must match and be a Point value type of the group CanBeDomainUnit.

## since version

7.184

## example
<pre>
attribute&lt;float32&gt; invertible (GridDomain) := <B>matr_inv(</B>src<B>)</B>;
</pre>

src
|   |   |
|--:|--:|
| 1 | 2 |
| 3 | 4 |

*GridDomain, nr of rows = 2, nr of cols = 2*

**invertible**
|         |          |
|--------:|---------:|
| **-2**  | **1**    |           
| **1.5** | **-0.5** |

*GridDomain, nr of rows = 2, nr of cols = 2*