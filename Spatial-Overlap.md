*[[Configuration examples]] Spatial Overlap*

In polygon layers, polygons might overlap. This can cause issues for instance with the [[point_in_polygon]] function in the GeoDMS (the sequence of polygons then defines the result, which is often undesired).

It can be useful, before working with a polygon layer, to first find out if in the layer polygons do overlap. For that purpose the following script shows two options.

1. grid approach. This gives a fast indication for substantial overlap. By reducing the grid size, the approach becomes more precise. The grid approach uses the [[poly2grid]] function for all polygon geometries in the order of the src file and the reverse order. If these approaches result in different outcomes, there must be overlap. The resulting boolean parameter: anyOverlap indicates if overlap does occur.
2. vector approach. This apporach is more time consuming, but give an exact area of overlap for the overlapping polygons. The [[polygon_connectivity]] function is used to find out which polygons are connected. For this set the multiplication function result in the overlap between these polygons. With the [[area]] function, the surface of the overlap is calculated.

## script
<pre>
container SpatialOverlap
{
   unit&lt;fpoint&gt;  point_rd_wms : format = "EPSG:28992";
   unit&lt;fpoint&gt;  point_rd     := range(point_rd_wms, point(300000f,0f), point(625000f,280000f)); 

<I> // the polygon set that might contain overlap is configured here &lt;</I>;
   unit&lt;uint32&gt; src_poly:
      StorageName     = "%SourceDataDir%/CBS/2019/cbs_gem_2019_si.shp"
   ,  StorageType     = "gdal.vect"
   ,  StorageReadOnly = "True"
   {
      attribute&lt;point_rd&gt; geometry (poly);
   }

   container grid
   {
      unit&lt;spoint&gt; domain :=  range(
         gridset(
             point_rd
            ,point(-100f, 100f, point_rd)
            ,point(625000f, 10000f, point_rd)
            ,spoint
         )
            ,point(0s, 0s)
            ,point(3250s, 2700s)
         )
      ,   DialogType = "point_rd";

      <I> // a reverse domain is configured with the same number of rows as the source polygon domain, 
             only with the geometries in reverse order </I>
       unit&lt;uint32&gt; reverse_poly := range(uint32, 0, #src_poly)
      {
         attribute&lt;point_rd&gt; geometry   (poly)  := src_poly/geometry[#reverse_poly - (id(reverse_poly) + 1)];
         attribute&lt;src_poly&gt; src_poly_rel       := #. - (id(.) + 1);
      }

      <I>// for both the source polygon domain and the reverse domain the poly2grid function is applied
         , the results are made comparable</I>
      attribute&lt;src_poly&gt;     src_poly_rel               (domain) := poly2grid(src_poly/geometry    , domain);
      attribute&lt;reverse_poly&gt; reverse_poly_rel           (domain) := poly2grid(reverse_poly/geometry, domain);
      attribute&lt;src_poly&gt;     reverse_poly_src_poly_rel  (domain) := reverse_poly/src_poly_rel[reverse_poly_rel];
      attribute&lt;bool&gt;         hasOverlap                 (domain) := src_poly_rel &lt;&gt; reverse_poly_src_poly_rel;

      parameter&lt;bool&gt; anyOverlapa := any(hasOverlap);
   }

   container vector
   {
      unit&lt;float32&gt; m  := baseunit('m', float32);
      unit&lt;float32&gt; m2 := m * m;
      unit&lt;ipoint&gt;  point_rd_mm :=
         gridset(
             point_rd
            ,point(0.001f, 0.001f, point_rd)
            ,point(0f    , 0f    , point_rd)
            ,ipoint
         );

      <I> // the  polygon_connectivity results in the set of connected/overlapping polygons </I>
      unit&lt;uint32&gt; overlap_vector := polygon_connectivity(ipolygon(src_poly/geometry[point_rd_mm]))
      {
         attribute&lt;point_rd&gt; geometry_F1 (poly) := src_poly/geometry[F1];
         attribute&lt;point_rd&gt; geometry_F2 (poly) := src_poly/geometry[F2];
   
         attribute &lt;ipoint&gt;  intersect   (poly) := 
            ipolygon(geometry_F1[point_rd_mm]) * ipolygon(geometry_F2[point_rd_mm]);
         attribute&lt;m2&gt;       area               := area(intersect[point_rd], m2);
      }

      unit&lt;uint32&gt; met_overlap_vector := select_with_org_rel(overlap_vector/area &gt; 0[m2])
      {
         attribute&lt;point_rd&gt; geometry (poly)  := value(overlap_vector/intersect[org_rel], point_rd);
         attribute&lt;m2&gt;       area             := overlap_vector/area[org_rel];
      }
   }
}