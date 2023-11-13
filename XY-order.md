## default

By default the orientation of point items in the GeoDMS [[Geometric functions]] is Y, X (column/row). This originates from the fact that the GeoDMS was developed to calculate mainly with [[grid data]], in which the orientation is usually colum / row.

## setting

Later on, when the GeoDMS was used more and more for projects using also (and mainly) [[vector data]], a setting was introduced in the [[config.ini]] to set the orientation (Y,X) or (X,Y).

**YX** : For projects with mainly [[grid data]] we advice to keep the default orientation Y, X. No settings has to be added to the config.ini or the following setting can be added:

<pre>
ConfigPointColRow=0
</pre>

**XY** : For projects with mainly [[vector data]] we advice to overrule the order to X, Y by the following setting in the config.ini file:

<pre>
ConfigPointColRow=1
</pre>

## examples

The following two examples show how to configure a geographic [[Grid Domain]], based on the setting for the X,Y order:

**I** (default order of **Y, X**):

<pre>
unit&lt;fpoint&gt; rdc_meter: Range = "[{300000, 0}, {625000, 280000})";

parameter&lt;rdc_meter&gt; TopLeftCoord := point(float32(625000), float32(10000), rdc_meter);

parameter&lt;int16&gt; nrofrows := int16(3250);
parameter&lt;int16&gt; nrofcols := int16(2700);

unit&lt;spoint&gt; rdc_100m
:=  range(
      gridset(
         rdc_meter
        ,point(float32(-100), float32(100), rdc_meter)
        ,TopLeftCoord
        ,spoint
      )
      ,point(int16(0), int16(0))
      ,point(nrofrows, nrofcols)
   )
,  Descr = "rdCoords/100m van NW naar SE (3250 rows, 2700 cols)";
</pre>

**II** (order overruled to **X, Y**):

<pre>
unit&lt;fpoint&gt; rdc_meter: Range = "[{0, 300000}, {280000, 625000})";

parameter&lt;rdc_meter&gt; TopLeftCoord := point(float32(10000), float32(625000), rdc_meter);

parameter&lt;int16&gt; nrofrows := int16(3250);
parameter&lt;int16&gt; nrofcols := int16(2700);

unit&lt;spoint&gt; rdc_100m
 :=  range(
       gridset(
          rdc_meter
         ,point(float32(100), float32(-100), rdc_meter)
         ,TopLeftCoord
         ,spoint
       )
       ,point(int16(0), int16(0))
       ,point(nrofcols, nrofrows)
   )
,  Descr = "rdCoords/100m van NW naar SE (3250 rows, 2700 cols)";
</pre>