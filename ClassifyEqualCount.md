*[[Classify functions]] ClassifyEqualCount*

## syntax

- ClassifyEqualCount(*a*, *[[domain unit]]*)

## definition

ClassifyEqualCount(*a*, *domain unit*) results in a [[data item]] with **class breaks, based on an equal count distribution of classes**. The resulting [[values unit]] is the values unit of data item *a*, the resulting domain unit is the *domain unit* [[argument]].
- *a*: numeric data item to be classified
- *domain unit*: determining the number of class breaks.

## description

An equal distribution of classes means the data item to be classified is split up in classes with, as far as possible, an equal count of occurrences in each class.

The same function can also be applied from the [[GUI|GeoDMS GUI]], by requesting the Palette Editor of a map layer and activate the Classify > Equal Count classification.

The ClassifyEqualCount results in a set of ClassBreaks that can be used in the [[classify]] function to classify a data item.

## applies to

- data item *a* with Numeric value type
- *domain unit* with [[value type]] from group CanBeDomainUnit

## since version

7.019

## example

<pre>
attribute&lt;nrPersons&gt; classifyEcNrInh (inh_4K) := <B>ClassifyEqualCount(</B>NrInh, inh_4K<B>)</B>;
</pre>

| **classifyEcNrInh** |
|--------------------:|
| **0**               |
| **55**              |
| **300**             |
| **860**             |

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