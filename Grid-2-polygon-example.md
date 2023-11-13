*[[Configuration examples]] Grid 2 polygon*

Calculating with [[grid data]] is usually much faster as with [[vector data]].

If it is still useful to transform grid data into [[polygons|polygon]], see the following example:

<pre>
parameter&lt;meter&gt; gridsize := 100[meter];

unit&lt;spoint&gt; griddomain
: StorageName = "%SourceDataDir%/src_grid.tif"
, StorageType = "gdal.grid"
, DialogData  = "Geografie/rdc"
{
  attribute&lt;float32&gt; GridData;
}

unit&lt;uint32&gt; polydomain := select_with_org_rel(IsDefined(griddomain/GridData))
{
   attribute&lt;rdc&gt; point_rdc       := org_rel[rdc];
   attribute&lt;rdc&gt; geometry (poly) := points2sequence(pointset/point, pointset/sequence, pointset/ordinal);

   unit&lt;uint32&gt; pointset := union_unit(.,.,.,.,.)
   {
       attribute&lt;rdc&gt;    point    := union_data(.
          , point_rdc                                         <I>// left top </I>
          , point_rdc + point(gridsize, 0[meter], rdc)        <I>// right top </I>
          , point_rdc + point(gridsize, -gridsize, rdc)       <I>// right bottom </I>
          , point_rdc + point(0[meter], -gridsize, rdc)       <I>// left bottom </I>
          , point_rdc                                         <I>// left top </I>
       );
       attribute&lt;..&gt;     sequence := union_data(., id(..), id(..), id(..), id(..), id(..))[uint32];
       attribute&lt;uint32&gt; ordinal  := 
          union_data(., const(0,..), const(1,..), const(2,..), const(3,..), const(4,..));
    }
}
</pre>

<pre>
With yx-coord the point-defintion is:

attribute&lt;rdc&gt;    point    := union_data(.
   , point_rdc
   , point_rdc + point(0[meter] , gridsize, rdc)
   , point_rdc + point(-gridsize, gridsize, rdc)
   , point_rdc + point(-gridsize, 0[meter], rdc)
   , point_rdc
);
</pre>
## explanation

This example results in a set of square polygons for each grid cell with non missing data in the GridData [[attribute]]. The selection with the isDefined
functions results in a uint32 [[domain unit]] with entries for each defined cell. In this subset also other criteria can be used to make selections of the relevant set of grid cells.

The configured pointset domain unit has 5 times the number of entries of the subset domain unit. This pointset domain is used to define for each polygon the topleft, bottomleft, bottomright, topright and again topleft coordinate. With the [[points2sequence]] function these points are converted to polygons.

The square polygons can be dissolved into large polygons (interior polygon segments are removed), by using the [[union_polygon|Union_polygon (dissolve)]] 
 or [[partitioned_union_polygon|Partitioned_union_polygon (dissolve by attribute)]] functions.




