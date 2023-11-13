*[[Unit functions]] gridset*

## syntax

- gridset(*coordinate_unit*, *size*, *offset*, *valuetype*)

## definition

gridset(*coordinate_unit*, *size*, *offset*, *valuetype*) configures a [[grid domain]] with a [[projection]] to a [[coordinate system| https://en.wikipedia.org/wiki/Geographic_coordinate_system]].

The gridset function has the following arguments:

- the *baseunit* in which the [[coordinates of the coordinatesystem|how to configure a coordinate system]] are expressed;
- the *size* of the grid cells in both X and Y directions, expressed in the [[base unit]];
- the *offset*, the coordinates of the top left coordinate, expressed in the base unit;
- the *[[value type]]* of the resulting [[domain unit]] (in earlier versions the value type was configured as string, this is not supported anymore).

## applies to

- [[unit]] *baseunit* with a Point value type
- [[parameter]] or literal *size* is with a Point value type
- parameter or literal *offset* is with a Point value type
- *valuetype* with a Point value type of the group CanBeDomainUnit

## conditions

The [[values unit]] of the *baseunit*, *size* and *offset* [[arguments|argument]] must match.

## be aware

The evaluation of a gridset, like the [[BaseUnit]] is executed when the meta/scheme information is generated in the [[GeoDMS GUI]]. If for this evaluation (large) primary files are read, this becomes times consuming. Expanding tree items in the treeview becomes slow. Therefor we advice to use the contents of large primary data file (or complex calculations) as little as possible in configuring gridsets.

If multiple [[geographic grid domains|geographic grid domain]] are needed, use the [[range]] function with arguments that might result from primary data and a gridset with arguments not dependent on primary data (see example 2). The variable arguments of the range are not part of the meta/scheme information and do not influence the speed in expanding tree items in the GUI. 

## example
<pre>
unit&lt;fpoint&gt; rdc_m := range(fpoint, point(300000f,0f), point(625000f,280000f))
,  label = "rijksdriehoekmeting in meters";

1. unit&lt;spoint&gt; rdc_100 := range(
      <B>gridset(</B>
          rdc_m
         ,point(  -100f,   100f, rdc_m)
         ,point(625000f, 10000f, rdc_m)
         ,spoint
      <B>)</B>
      ,point(   0s,    0s)
      ,point(3250s, 2700s)
   );

2. unit&lt;ipoint&gt; ahn_tiles_gridset :=
      <B>gridset(</B>
               point_rd
              ,point(-0.5[m], 0.5[m], point_rd)
              ,point(0f, 0f, point_rd)
              ,ipoint
      <B>)</B>;
   unit&lt;ipoint&gt; ahn_tile_I:=
      range(
            ahn_tiles_gridset
    	   ,point(bbox/y_max, bbox/x_min, point_rd)[ahn_tiles_gridset]
	   ,point(bbox/y_max, bbox/x_min, point_rd)[ahn_tiles_gridset] 
            + point(nr_rows_int32, nr_cols_int32, ahn_tiles_gridset)
      );
</pre>