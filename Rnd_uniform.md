*[[Miscellaneous functions]] rnd_uniform*

## syntax

- rnd_uniform(*seed*, *domainunit*, *valuesrange*)

## definition

rnd_uniform(*seed*, *domainunit*, *valuesrange*) results in a [[data item]] with **pseudo random values** and with the following [[arguments|argument]]:

- a random *seed* with a unique number; using the same random seed results in the same random values;
- the *domainunit* of the resulting data item;
- the *valuesrange* determines the range of possible values. Use the [[range]] function to configure this range.

## applies to

- data item *seed* with uint32 [[value type]] 
- [[unit]] *domainunit* with value type from the group CanBeDomainUnit

## conditions

The [[values unit]] of the resulting data item and of the *valuesrange* argument must match.

## example

<pre>
attribute &lt;float32&gt; rand (ADomain) := <B>rnd_uniform(</B>0, ADomain, range(float32, 0f, 1f)<B>)</B>;
</pre>

| **rand**     |
|-------------:|
| **0.719643** |
| **0.781171** |
| **0.974884** |
| **0.446728** |
| **0.087888** |

*ADomain, nr of rows = 5*

## see also

- [[rnd_permutation]]