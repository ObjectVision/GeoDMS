*[[Rescale functions]] ramp_rgb*

## syntax

- ramp_rgb(*startvalue*, *endvalue*, *domainunit*)

## definition

ramp_rgb(*startvalue*, *endvalue*, *domainunit*) **ramps rgb values**, starting with the *startvalue* [[argument]] and ending with the *endvalue* argument.

The number of values is defined by the [[cardinality]] of the [[domain unit]].

## applies to

- [[parameters|parameter]] *startvalue* and *endvalue* with [[value type]] uint32 (often configured with the [[rgb]] function)
- unit *domainunit* with value type from group CanBeDomainUnit

## since version

7.031

## example

<pre>
attribute&lt;uint32&gt; BrushColor (City) := <B>ramp_rgb(</B>rgb(0,255,0), rgb(0,0,255), City<B>)</B>;
</pre>

| **BrushColor**        |
|-----------------------|
| **rgb( 0, 255, 0)**   |
| **rgb( 0, 212, 42)**  |
| **rgb( 0, 170, 85)**  |
| **rgb( 0, 127, 127)** |
| **rgb( 0, 85, 170)**  |
| **rgb( 0, 42, 212)**  |
| **rgb( 0, 0, 255)**   |

*domain City, nr of rows = 7*

## see also

- [[ramp]]