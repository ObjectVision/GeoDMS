*[[Relational functions]] join_equal_values*

## syntax

- join_equal_values(*first_rel*, *second_rel*)

## definition

join_equal_values(*first_rel*, *second_rel*) results in a new [[uint32]] [[domain unit]] with entries for each combinations of all equal values of first_rel and second_rel. The functions results in three subitems:

- *nr_1_re*l: the [[index numbers]] of the combination of equal values in the domain unit of the first [[argument]]
- *nr_2_rel*: the [[index numbers]] of the combination of equal values in the domain unit of the second argument
- *nr_X_rel*: the equal value in the combination

The explicit: *join_equal_values_uint64*, *join_equal_values_uint32*,*join_equal_values_uint16* and *join_equal_values_uint8* functions can be used in the same manner as the join_equal_values function, to create a new domain unit with an explicit [[value type]].

## applies to

- *first_rel*: a uint32 [[relation]]
- *second_rel*: a uint32 relation

## conditions

1. The [[values unit]] of the [[arguments|argument]]: *first_rel* and *second_rel* must match.
2. The domain unit of arguments: *first_rel* and *second_rel* must be zero based.

## see also

- https://en.wikipedia.org/wiki/Pullback_(category_theory)
- https://www.w3schools.com/sql/sql_join_inner.asp

## example

<pre>
unit&lt;uint32&gt; RegionCity := <B>join_equal_values(</B>region/country_rel, city/country_rel<B>)</B>;
</pre>

| **nr_1_rel**  | **nr_2_rel**  | **nr_X_rel**  |
|--------------:|--------------:|--------------:|
| **0**         | **1**         | **0**         |
| **1**         | **1**         | **0**         |
| **2**         | **0**         | **1**         |
| **3**         | **0**         | **1**         |
| **4**         | **2**         | **2**         |

*domain CityRegionCombinations , nr of rows = 5*

| region/country_rel |
|-------------------:|
| 0                  |
| 0                  |
| 1                  |
| 1                  |
| 2                  |

*domain region, nr of rows = 5*

| city/country_rel |
|-----------------:|
| 1                |
| 0                |
| 2                |

*domain city, nr of rows = 3*

## see also

* [[join_equal_values_uint8_16_32_64]]