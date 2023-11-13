Kernels are used to define the size and weight of the neighborhood used in a [[Neighbourhood Potential]] calculation.

A kernel in the GeoDMS consists of a non geographic [[grid domain]] with a weight [[attribute]] for this domain. This weight
attribute is used as second [[argument]] in the [[potential]] function.

Kernels are usually configured in the GeoDMS configurations.

## basic

The following kernel is also used in the [[Potential with kernel]] example.

<pre>
container kernel
{
   unit&lt;uint32&gt; Dist2Range;
   unit&lt;spoint&gt; pot50km := range(spoint, point(-50s, -50s), point(51s, 51s))
   {
      attribute&lt;Dist2Range&gt; distMatr  := dist2(point(0s, 0s, .), Dist2Range);
      attribute&lt;float32&gt;    AbsWeight := distMatr <= 2500 ? 1f : 0f;
   }
}
</pre>

This basic kernel defines a grid with 101 * 101 = 10.201 cells.

The distrMatr attribute calculates the square distances from each cell towards the center cell of the grid. The AbsWeight attribute gets the value one for each cell at a distance of less than or equal to 50 cells and a value of zero for cells at a distance of more than 50 cells from the center. This
AbsWeight attribute is shown in the next figure:

[[images/Kernel_w200.png]]

As in this AbsWeight attribute only the values 0 and 1 occur, potentials functions with such an attribute have no distance decay weight. Each cell in the kernel has the same weight.

## distance decay

The next example shows the configuration of an weight attribute with a [distance decay function](https://en.wikipedia.org/wiki/Distance_decay), in which the weights diminish as a function of the distance:

<pre>
attribute&lt;float32&gt; AbsWeightDistDecay := distMatr <= 2500 ? 1f / float32(distMatr) : 0f;
</pre>

As the distMatr [[attribute]] calculates square distances, this function models a [distance decay function](https://en.wikipedia.org/wiki/Distance_decay) with an exponent of 2. Like the basic kernel, all cells with a distance of more than 50 cells towards the center cell will get a zero value.

This AbsWeightDistDecay attribute is shown in the next figure:

[[images/Kernel_dd_w200.png]]

## relative weights

If a [[potential]] function is used to spread a certain amount over a region, it is often desirable to keep the sum of the spreaded out amount the same as the sum of the original amount.

To achieve this, the sum of the weight values need to sum up to the value 1. This can be achieved with a relative weight attribute as configured in the next example.

<pre>
attribute&lt;float32&gt; RelWeight := scalesum(AbsWeightDistDecay, 1f);
</pre>