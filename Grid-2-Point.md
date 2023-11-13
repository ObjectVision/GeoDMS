*[[Configuration examples]] Grid 2 Point*

## since version 7.015

Since GeoDMS 7.015 a value function (example) can be used to convert the [[index numbers]] of a [[grid domain]] to world coordinates.

## example
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

<B>attribute&lt;rdc&gt; point_7015_and_later_rel (griddomain) := id(griddomain)[rdc];</B>
</pre>

A grid domain is configured, with rdc as coordinate [[unit]]. In the bold line the actual conversion to world coordinates is configured.