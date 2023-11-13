*[[Configuration examples]] Point 2 Grid*

## since version 7.015

Since GeoDMS 7.015 a [[relation]] to a [[grid domain]] can be configured easily with the value function (example), based on a point [[attribute]] of a vector [[domain|domain_unit]].

The resulting attribute will contain the [[index numbers]] of the related grid domain for the vector domain.

### example
<pre>
<I>// coordinate system </I>
unit&lt;fpoint&gt; rdc_base : format = "EPSG:28992",
unit&lt;fpoint&gt; rdc      := range(rdc_base, point(300000f,0f), point(625000f,280000f));

<I>// configuration of grid domain, related to the rdc</I>
unit&lt;spoint&gt; griddomain := range(
      gridset(rdc, point(-100f, 100f, rdc), point(625000f, 10000f, rdc) ,'spoint')
     ,point(   0s,    0s)
     ,point(3250s, 2700s)
);

<I> // configuration of vector domain, with a point attribute in rdc coordinates</I>
unit&lt;uint32&gt; vectordomain: nrofrows = 4
{
   attribute&lt;rdc&gt; point: [{20427,69272},{17502,95885},{3188,80531},{12620,112190}];
}

<B>attribute&lt;griddomain&gt; grid_7015_and_later_rel (vectordomain) := vectordomain/point[griddomain];</B>
</pre>

A grid and vector domain_unit are configured, both with rdc as coordinate unit. In the bold line the actual relation is configured. The [[value]] functions works as the coordinates used are expressed in the same coordinates (in this case rdc).

The value function can also be applied to calculate coordinate transformations, if the format strings of the coordinate systems are known. These strings need to be configured in the format [[property]] of these base units for the [[coordinate systems|How to configure a coordinate system]].

## before version 7.015

Before GeoDMS version 7.015 such a relation needed to be configured with the [[GetProjectionFactor]] and [[GetProjectionOffset]] functions to relate a point attribute in world coordinates to a grid domain.

### example
<pre>
<I>// coordinate system, grid and vector domain are similar </I>
parameter&lt;rdc&gt; projOffset := GetProjectionOffset (griddomain); <I>// Result: [{625000, 10000)]</I>
parameter&lt;rdc&gt; projFactor := GetProjectionFactor (griddomain); <I>// Result: [(-100.0, 100.0)]</I>

<B>attribute&lt;griddomain&gt; grid_before_7015_rel (vectordomain) 
   := value((vectordomain/point - projOffset) / projFactor, griddomain);</B>
</pre>

In the bold line the actual relation is configured. The projOffset and projFactor are used to relate the points in the rdc coordinates to the griddomain.

## backward incompability

To make your configuration working both in versions before and after 7.015, you need to configure an expression in which the [[GeoDMSVersion]] function is used to determine the running GeoDMS version.

### example
<pre>
attribute&lt;griddomain&gt; grid_rel (vectordomain) := = GeoDMSVersion() > 7.015
   ? 'vectordomain/point[rdc]'
   : 'value((vectordomain/point - projOffset) / projFactor, griddomain")';
</pre>

The expression in the example with the double == is an [[indirect expression]]. Based on the GeoDMSVersion the grid_rel is calculated in a different way.