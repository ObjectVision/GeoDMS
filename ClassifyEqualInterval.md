*[[Classify functions]] ClassifyEqualInterval*

## syntax

- ClassifyEqualInterval(*a*, *[[domain unit]]*)

## definition

ClassifyEqualInterval(*a*, *domain unit*) results in a [[data item]] with **class breaks, based on a equal interval distribution of classes**. The resulting [[values unit]] is the values unit of data item *a*, the resulting domain unit is the*domain unit* [[argument]].

- *a*: numeric data item to be classified
- *domain unit*: determining the number of class breaks.

## description

An equal distribution of classes means the data item to be classified is split up in classes with, as far as possible, an equal interval size of each class.

The same function can also be applied from the [[GUI|GeoDMS GUI]], by requesting the Palette Editor of a map layer and activate the Classify > Equal Interval classification.

The ClassifyEqualInterval results in a set of ClassBreaks that can be used in the [[classify]] function to classify a data item.

## applies to

- data item *a* with Numeric value type
- *domain unit* with [[value type]] from group CanBeDomainUnit

## since version

7.019

## example

<pre>
attribute&lt;nrPersons&gt; classifyEiNrInh (inh_4K) := <B>ClassifyEqualInterval(</B>NrInh, inh_4K<B>)</B>;
</pre>

| **classifyEiNrInh** |
|--------------------:|
| **0**               |
| **341**             |
| **683**             |
| **1025**            |

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