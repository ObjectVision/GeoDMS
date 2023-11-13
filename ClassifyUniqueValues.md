*[[Classify functions]] ClassifyUniqueValues*

## syntax

- ClassifyUniqueValues(*a*, *[[domain unit]]*)

## definition

ClassifyUniqueValues(*a*, *domain unit*) results in a [[data item]] with class breaks, based on the **unique values** occurring in the data item *a*. The resulting [[values unit]] is the values unit of data item *a*, the resulting domain unit is the domain unit [[argument]].

- *a*: numeric data item to be classified
- *domain unit*: determining the number of class breaks.

## description

The ClassifyUniqueValues function results in a number of class breaks, based on the number of rows of the *domain unit* argument. If the data item to be classified contains more unique values than this number, the resulting set of cut off by this number. If the data item to be classified contains less unique values, the resulting [[data item]] is completed up with repeating the highest unique value.

To create a new domain unit with all unique values occurring in the original data set once, use the [[unique]] function.

This unique function works similar to the unique values function in the [[GUI|GeoDMS GUI]] which can be applied by requesting the Palette Editor of a map layer and activate the Classify > Unique values classification.

## applies to

- data item *a* with Numeric [[value type]]
- *domain unit* with value type from group CanBeDomainUnit

## since version

7.019

## example

<pre>
attribute&lt;nrPersons&gt; classifyUvNrInh (inh_4K) := <B>ClassifyUniqueValues(</B>NrInh, inh_4K<B>)</B>;
</pre>

| **classifyUvNrInh** |
|--------------------:|
| **0**               |
| **2**               |
| **20**              |
| **55**              |

*Table inh_4K, nr of rows = 4*


| NrInh |
|------:|
| 550   |
| 1025  |
| 300   |
| 200   |
| 0     |
| null  |
| 300   |
| 2     |
| 20    |
| 55    |
| 860   |
| 1025  |
| 1025  |
| 100   |
| 750   |

*Table District, nr of rows = 15*