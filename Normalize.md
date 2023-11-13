*[[Rescale functions]] normalize*

## syntax

- normalize(*a*, *mean*, *sd*)

## definition

normalize(*a*, *mean*, *sd*) scales the [[attribute]] *a* to a [normal distribution](https://en.wikipedia.org/wiki/Normal_distribution) with as mean value the [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameter]] *mean* and as standard deviation the literal or parameter *sd*.

The resulting attribute has a new [[values unit]] and the same [[domain unit]] as attribute *a*.

## description

The literals or parameters *mean* and *sd* have default values of zero and one.

To avoid rounding off errors in the normalize calculations, use a float32 of float64 [[value type]] for the *a*, *mean* and *sd* [[arguments|argument]].

## conditions

The value type of attribute *a* and literals or parameters *mean* and *sd* must match.

## example

<pre>
attribute&lt;float32&gt; normalize_NrInh (City) := <B>normalize(</B>City/NrInhabitants, 0f, 1f<B>)</B>;
</pre>

| City/NrInhabitants | **normalize_nrInh** |
|-------------------:|--------------------:|
| 550                |           **1.13**  |
| 525                |           **0.96**  |
| 300                |           **-0.48** |
| 500                |           **0.80**  |
| 200                |           **-1.13** |
| 175                |           **-1.29** |
| null               |           **null**  |

*domain City, nr of rows = 7*