*[[Rescale functions]] scalesum*

## syntax

- scalesum(*proxy*, *a*)
- scalesum(*proxy*, *relation*, *a*)

## definition

- scalesum(*proxy*, *a*) **disaggregates** the values of [[parameter]] *a*, according to the distribution of the *proxy* [[attribute]]. The function results in a new attribute with the [[values unit]] of [[argument]] *a* and the [[domain unit]] of argument *proxy*.
- scalesum(*proxy*, *relation*, *a*) **disaggregates** the values of attribute *a*, according to the distribution of the *proxy* attribute, grouped by [[relation]]. The function results in a new attribute with the values unit of argument *a* and the domain unit of arguments *proxy* and *relation*.

## description

To avoid rounding off errors within the scalesum calculation, use a float32 of float64 [[value type]] for arguments *proxy* and *a*.

The sum of the result will equal the given quantity *a*, provided that the quantity can be related to any element of the disaggregated domain with a positive proxy value.

To avoid loosing parts of the quantity, check that for each partition the total proxy value is positive (unless the quantity is zero).

## conditions

1. The domain units of arguments *proxy* and *relation* must match.
2. The value type of arguments *proxy* and *a* must match.

## example

<pre>
1. attribute &lt;float32&gt; ssum_NrInh (CityDomain) := 
   <B>scalesum(</B>
      City/Area  <I>// proxy variable for distribution</I> 
     ,550f       <I>// amount to be distributed</I> 
   <B>)</B>;

2. attribute&lt;uint32&gt;  ssum_NrInh_per_region (CityDomain) := 
   uint32(
      <B>scalesum(</B>
          City/Area
         ,City/Region_rel  <I>// relation from city to region</I> 
         ,float32(Region/NrInhabitants)
      <B>)</B>
   );
</pre>

| City/Area | City/NrInhabitants | City/Region_rel |**ssum_NrInh** | **ssum_NrInh_per_region** |
|----------:|-------------------:|----------------:|--------------:|--------------------------:|
| 2         | 550                | 0               | **120.55**    | **550**                   |
| 0         | 525                | 1               | **100.69**    | **531**                   |
| 1         | 300                | 2               | **49.31**     | **300**                   |
| 0         | 500                | 1               | **89.04**     | **493**                   |
| 1         | 200                | 3               | **27.40**     | **111**                   |
| 1         | 175                | null            | **null**      | **null**                  |
| null      | null               | 3               | **null**      | **88**                    |

*domain City, nr of rows = 7*

| Region/NrInhabitants |
|---------------------:|
| 550                  |
| 1025                 |
| 300                  |
| 200                  |
| 0                    |

*domain Region, nr of rows = 5*