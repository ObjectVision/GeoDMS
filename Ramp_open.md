*[[Rescale functions]] ramp_open*

## syntax

- ramp_open(*startvalue*, *uptovalue*, *domainunit*)

## definition

ramp_open(*startvalue*, *uptovalue*, *domainunit*) **ramp values**, starting with the *startvalue* [[argument]] up to the *uptovalue* argument.

The *uptovalue* argument itself is not part of the resulting [[attribute]].

The number of values is defined by the [[cardinality]] of the *[[domain unit]]* argument.

## applies to

unit *domain unit* with [[value type]] from group CanBeDomainUnit

## since version

7.031

## example

<pre>
attribute&lt;uint32&gt; ramped (City) := <B>ramp_open(</B>0, 70, City<B>)</B>;
</pre>

| **ramped** |
|-----------:|
| **0**      |
| **10**     |
| **20**     |
| **30**     |
| **40**     |
| **50**     |
| **60**     |

*domain City, nr of rows = 7*

## see also

- [[ramp]]