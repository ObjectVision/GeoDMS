Cleaning a [[polygon]] [[geometry]] has two purposes:

1) get rid of unexpected lines in the visualsiation of polygon items in map views, often due to [[coordinate conversions]].

2) remove double coordinates to finish simplifying your [[geometry]].

Cleaning a polygon geometry can be configured with the [[partitioned_union_polygon (dissolve by attribute)]] function, see example:

<pre>
attribute&lt;LatLong_mdegrees&gt; Geometry_mdegrees_clean (polygon, domain) := 
   partitioned_union_polygon(Geometry_mdegrees, id(domain));
</pre>