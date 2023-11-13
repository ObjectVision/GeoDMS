*[[Classify functions]] ClassifyNonzeroJenksFisher*

## syntax

- ClassifyNonzeroJenksFisher(*a*, *[[domain unit]]*)

## definition

ClassifyNonzeroJenksFisher(*a*, *domain unit*) results in a [[data item]] with class breaks, based on the method described in [[Fisher's Natural Breaks Classification complexity proof]].
The resulting [[values unit]] is the values unit of data item *a*, the resulting [[domain unit]] is the *domain unit* [[argument]].

- *a*: numeric data item to be classified
- *domain unit*: determining the number of class breaks.

This Nonzero version of this function classifies the positive and negative values separately. The value 0 is a treated as a special value, it is a compulsory class break. The number of classes for the positive and the negative values, within the total number of classes, are chosen in a manner that minimalizes the sum of the square deviations from the class means.

## description

The Jenks Fisher classification method, is a fast algorithm that results in breaks that minimize the sum of the square deviations from the class means, also known as natural breaks. The self contained code with an example usage is:
[[CalcNaturalBreaksCode]]

The same function can also be applied from the [[GUI|GeoDMS GUI]] by requesting the Palette Editor of a map layer and activate the Classify > JenksFisher classification.

The ClassifyNonzeroJenksFisher results in a set of ClassBreaks that can be used in the [[classify]] function to classify a data item.

## applies to

- data item a with Numeric [[value type]]
- domain unit with value type from group CanBeDomainUnit

## since version

7.019

## example

<pre>
attribute&lt;nrPersons&gt; classifyNZJfNrInh  (inh_4K) := <B>ClassifyNonzeroJenksFisher(</B>NrInh, inh_4K<B>)</B>;
</pre>

| **classifyJfNrInh** |
|--------------------:|
| **0**               |
| **2**               |
| **300**             |
| **750**             |

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

see also
- [[ClassifyJenksFisher]]