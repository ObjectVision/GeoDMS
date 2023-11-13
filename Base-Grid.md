A [[grid]] defines a two-dimensional structure in which each cell has a value. But for some [[operators and functions]] many cells might be irrelevant. Think for example of a [[allocation]] function that does not allocate sea cells.

If the number of irrelevant cells is substantial, it is more efficient to apply operators and functions on a [[subset]] of the grid cells. This
subset only contains the cells that are relevant. A Base Grid attribute is configured as the [[relation]] for the grid domain unit towards the
one-dimensional subset domain (usually a uint32 [[domain unit]]). This basegrid is used to convert the results at the subset domain back to the original grid domain. This is necessary for the visualisation in a Map View.

## *example*
<pre>
attribute&lt;bool&gt; IsAllocatable (GridDomain) := LandUseType == luEndogenous

unit&lt;uint32&gt; FreeLandCells := select_with_org_rel(IsAllocatable)
{
   attribute&lt;FreeLandCells&gt; permutation                  := rnd_permutation(0, rnd_FreeLandCells);
   attribute&lt;GridDomain&gt;    org_rel2     (FreeLandCells) := org_rel[permutation];
   attribute&lt;FreeLandCells&gt; BaseGrid     (GridDomain)    := invert(org_rel2);
}

attribute&lt;dollar_ha>                     Residential  (FreeLandCells) 
   := SuitabilityMap/Residential[FreeLandCells/nr_OrgEntity2];
attribute&lt;Classifications/LU/Endogenous> landuse_grid (GridDomain)    
   := landuse[Compact/FreeLandCells/BaseGrid];
</pre>

-   The **IsAllocatable** attribute is a boolean attribute defining the condition for the selection of allocatable cells.
-   The **FreeLandCells** [[unit]] is the domain unit of the allocatable cells. A random permutation is used in this domain to process all grid cells in a random order. This is usefull if the suitability maps are not distinctive enough, resulting in multiple cells with the same suitability. The random order prevents in this situation an allocation mainly derived from the grid cell coordinate.
-   The **BaseGrid** attribute defines the relation from the GridDomain domain unit towards the FreeLandCells domain.
-   The **Residential** attribute presents an example of how to relate an attribute (in this case the suitability map for Residential) from the GridDomain towards the FreeLandCells domain.
-   The **landuse_grid** attribute presents an example of how to to relate the results of the allocation at the FreeLandCells domain backwards to the GridDomain.