*[[Allocation functions]] full script example*

The discrete [allocation](https://github.com/ObjectVision/LandUseModelling/wiki/Allocation) is used to allocate [land use types](https://github.com/ObjectVision/LandUseModelling/wiki/Land-Use-Types) to [[grid]] cells with the [[allocation functions]].

The example presents how to configure all [[arguments|argument]] and the function itself.

## script
<pre>
container allocation
{
   unit&lt;fpoint&gt; TestCoords: Range = "[{300000, 0}, {625000, 280000})";
 
   unit&lt;spoint&gt; DomainGrid := 
      range(
         gridset(
             TestCoords
            ,point(  -500f, 500f,   TestCoords)
            ,point(625000f, 10000f, TestCoords)
           , sPoint
        ), point(0s, 0s), point(6s, 6s)
     )
     ,   DialogType = "Map";
   
   container source
   {
      unit&lt;int32&gt; EurM2: Range = "[0, 20)";

      unit &lt;uint8&gt; lu_type: nrofrows = 3
      {
         attribute&lt;string&gt;  Name:           ['Living', 'Working', 'Nature'];
         attribute&lt;string&gt;  PartioningName: ['Living', 'Working', 'Nature'];
         attribute&lt;lu_type&gt; partioning    := id(lu_type);
       }
       attribute&lt;lu_type&gt; landuse (DomainGrid):
       [
        2, 2, 2, 1, null, null,
        2, 2, 0, 0, null, 1,
        2, 0, 0, 1,    1, 1,
        2, 0, 0, 0,    1, 1,
        2, 0, 0, 1, null, 1,
        2, 2, 2, 2,    2, 2
       ];
       container Suitability
       {
          attribute&lt;EurM2&gt; Living (DomainGrid): 
          [
           1,  2,  5, 4,  3, -1,
           2,  5,  8, 9,  7, 3,
           4, 10, 12,13, 12, 6,
           5, 11, 13,14, 12, 6,
           4,  9,  9, 5,  3, 2,
           2,  2,  4, 3,  1, 1
          ];
          attribute&lt;EurM2&gt; Working (DomainGrid):
          [
           1, 1, 2, 3,  4, -6,
           2, 3, 4, 6,  8,  9,
           2, 4, 9,11, 12, 10,
           1, 3, 5, 9, 10,  6,
           2, 4, 5, 5,  3,  2,
           1, 1, 2, 1,  1,  1
          ];
          attribute&lt;EurM2&gt; Nature (DomainGrid):
          [
           3, 3, 3, 2, 2, -2,
           3, 3, 2, 2, 2, 2,
           3, 2, 1, 1, 1, 1,
           3, 2, 1, 1 ,1, 2,
           3, 3, 2, 1, 2, 2,
           3, 3, 3, 3, 3, 3
          ];
       }
       container regMaps
       {
          unit&lt;uint8&gt; p1: nrofrows = 1;
          unit&lt;uint8&gt; p2: nrofrows = 2;

          attribute&lt;p1&gt; p1Map (DomainGrid) := const(0, DomainGrid, p1);
          attribute&lt;p2&gt; p2Map (DomainGrid) := pointRow(id(DomainGrid)) &lt; 4s 
             ? 0[p2] 
             : 1[p2];
       }
       container claim_sources
       {
          unit&lt;float32&gt; Meter := BaseUnit('m', float32);
          unit&lt;float32&gt; Ha    := 10000.0 * Meter * Meter; 

          container p1
          {
             attribute&lt;Ha&gt; Nature_min (regMaps/p1): [12];
             attribute&lt;Ha&gt; Nature_max (regMaps/p1): [20];
             attribute&lt;Ha&gt; Living_min (regMaps/p1): [5];
             attribute&lt;Ha&gt; Living_max (regMaps/p1): [9];
          }
          container p2
          {
             attribute&lt;Ha&gt; Working_min (regMaps/p2): [6,2];
             attribute&lt;Ha&gt; Working_max (regMaps/p2): [10,4];
          }
       }
       parameter&lt;float32&gt; nrHaPerCel := 1[claim_sources/Ha];
       container claims_min
       {
         attribute&lt;uint32&gt; Living  (regMaps/p1)  := uint32(claim_sources/p1/Living_min  / nrHaPerCel);
         attribute&lt;uint32&gt; Working (regMaps/p2)  := uint32(claim_sources/p2/Working_min / nrHaPerCel);
         attribute&lt;uint32&gt; Nature  (regMaps/p1)  := uint32(claim_sources/p1/Nature_min  / nrHaPerCel);
      }
      container claims_max
      {
         attribute&lt;uint32&gt; Living  (regMaps/p1)  := uint32(claim_sources/p1/Living_max  / nrHaPerCel);
         attribute&lt;uint32&gt; Working (regMaps/p2)  := uint32(claim_sources/p2/Working_max / nrHaPerCel);
         attribute&lt;uint32&gt; Nature  (regMaps/p1)  := uint32(claim_sources/p1/Nature_max  / nrHaPerCel);
      }
      container regionSets
      {
         attribute&lt;regMaps/p1&gt; Nature  (DomainGrid) := regMaps/p1Map;
         attribute&lt;regMaps/p1&gt; Living  (DomainGrid) := regMaps/p1Map;
         attribute&lt;regMaps/p2&gt; Working (DomainGrid) := regMaps/p2Map;
      }
      unit&lt;uint16&gt; AtomicRegions := overlay(lu_type/PartioningName, DomainGrid, regionSets);

      attribute&lt;bool&gt; InRegio (DomainGrid):
       [
        True, True, True, True, False, True,
        True, True, True, True, False, True,
        True, True, True, True, True,  True,
        True, True, True, True, True,  True,
        True, True, True, True, False, True,
        True, True, True, True, True,  True
       ];
       
       attribute &lt;bool&gt; FreeLand (DomainGrid) := InRegio;

       container Compacted
       {
          unit&lt;uint32&gt; ADomain := select_with_org_rel(FreeLand = True), label = "allocation domain";
 
         attribute&lt;ADomain&gt; BaseGrid (DomainGrid) := invert(ADomain/org_rel);
 
         container SuitabilityMaps
         {
            attribute&lt;EurM2&gt; Living  (ADomain) := source/Suitability/Living[ADomain/nr_orgEntity];
            attribute&lt;EurM2&gt; Working (ADomain) := source/Suitability/Working[ADomain/nr_orgEntity];
            attribute&lt;EurM2&gt; Nature  (ADomain) := source/Suitability/Nature[ADomain/nr_orgEntity];
         }
         attribute&lt;AtomicRegions&gt; AtomicRegionMap (ADomain) := AtomicRegions/UnionData[ADomain/nr_orgEntity];
      }
      parameter&lt;EurM2&gt; treshold := 0[EurM2];
      container FeasibleSolution;
  }
  container allocate_discrete := 
     <B>discrete_alloc(</B>
         source/lu_type/name
        ,source/Compacted/ADomain
        ,source/Compacted/SuitabilityMaps
        ,source/lu_type/partioning
        ,source/lu_type/PartioningName
        ,source/AtomicRegions
        ,source/Compacted/AtomicRegionMap
        ,source/claims_min
        ,source/claims_max
        ,source/treshold
        ,source/FeasibleSolution
        <B>)</B>
   {
     attribute&lt;Source/lu_type&gt; alloc (DomainGrid) := landuse[Source/Compacted/BaseGrid];
   }
}
</pre>