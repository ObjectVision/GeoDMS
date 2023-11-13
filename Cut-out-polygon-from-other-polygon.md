*[[Configuration examples]] Cut out polygon from other polygon*

The script presents an example to cut out a [[polygon]] from another polygon.

## example

<pre>
unit&lt;uint32&gt; Square : NrofRows = 1
{
   <I>// create a square </I>
   attribute&lt;meter&gt; left   := const(      0,.,meter);
   attribute&lt;meter&gt; right  := const( 280000,.,meter);
   attribute&lt;meter&gt; top    := const( 625000,.,meter);
   attribute&lt;meter&gt; bottom := const( 300000,.,meter);

   attribute&lt;rdc_meter&gt; left_top     := point(   top, left,  rdc_meter);
   attribute&lt;rdc_meter&gt; right_top    := point(   top, right, rdc_meter);
   attribute&lt;rdc_meter&gt; right_bottom := point(bottom, right, rdc_meter);
   attribute&lt;rdc_meter&gt; left_bottom  := point(bottom, left,  rdc_meter);

   unit&lt;uint32&gt; pointset : nrofrows = 5
   {
      attribute&lt;rdc_meter&gt; coordr := union_data(., left_top, right_top, right_bottom, left_bottom, left_top);
   }

   attribute&lt;rdc_meter&gt; geometry   (polygon) := 
      points2sequence(pointset/coord, const(0, pointset, square), id(pointset));
   attribute&lt;ipoint&gt;    geometry_i (polygon) := ipolygon(geometry);

   <I> // cut out polygon is for instance the boundary of the first municipality 
      (index numbers, here referred to as cut_out_polygon.</I> 
  <B>attribute&lt;rdc_meter&gt; square_with_polygon_cut_out (poly) := 
     (geometry_i - cut_out_polygon/geometry_i[0])[rdc_meter];</B>
}
</pre>
## explanation

The example first configures a square [[polygon]] for the whole of the Netherlands.

The actual cutting out is done in the bold line. The [[-|Sub (difference)]] function is used to cut out the first geometry in the cut_out_polygon domain for instance the first municipality in the Netherlands, from the square.

The [[ipolygon]] conversion function is used as these vector functions only work on integer coordinates. The casting back to *[rdc_meter]* at the end of the [[expression]] is used to convert the integer 
coordinates back to the rdc_meter [[values unit]].