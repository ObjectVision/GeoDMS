*[[Configuration examples]] Dissolve*

Dissolving is the process of merging detailed [[polygon]] geometries in larger polygon geometries by removing interior segments.

In the GeoDMS this can also be done based on a [[relational attribute|relation]] to make polygons for each entry of the [[domain unit]] of this  relational attribute.

## example

<pre>
container dissolve
{
   unit&lt;float32> meter    := baseunit('meter', float32);
   unit&lt;fpoint&gt;  point_rd_base;
   unit&lt;fpoint&gt;  point_rd := 
      range(point_rd_base, point(300000[meter],0[meter]), point(625000[meter],280000[meter]));

   unit&lt;uint32&gt; municipality
   : StorageName      = "%SourceDataDir%/CBS/2017/gem_2017.shp"
   , StorageType      = "gdal.vect"
   , StorageReadOnly  = "True"
  {
      attribute&lt;point_rd&gt; geometry (polygon);
      attribute&lt;string&gt;   name;
      attribute&lt;string&gt;   regionname;
      attribute&lt;region&gt;   region_rel := rlookup(regionname, region /values);
   }
   unit&lt;uint32> region := unique(municipality/regionname)
   {
    <B>attribute&lt;point_rd&gt; geometry (polygon) := 
        partitioned_union_polygon(ipolygon(municipality/geometry), municipality/region_rel)[point_rd];</B>
   }
}
</pre>

## explanation

The example presents two domains: municipality and region. A [[relation]] is configured (region_rel) relating the municipality domain unit to the region  domain.

The actual dissolving is done in the bold line. The [[partitioned_union_polygon|Partitioned_union_polygon (dissolve by attribute)]]
is used to dissolve the geometries of a municipality for a region. The [[ipolygon]] conversion function is used as these vector functions only work on integer coordinates. The casting back to [point_rd] at the end of the [[expression]] is used to convert the integer coordinates back to the point_rd [[values unit]].