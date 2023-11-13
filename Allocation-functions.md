*Allocation [[functions|Operators and functions]] discrete_alloc*

[Land Use Allocation](https://github.com/ObjectVision/LandUseModelling/wiki/Allocation) is the process of the assignment of land resources to various usage purposes. It is used to find the allocation of  land use to land units that maximizes total suitability, given a set of claims, when endogenous interactions are disregarded. The following two allocation processes are dinstinguished:

1.  *continuous*: each unit of land is allocated to a percentage, indicating the expected value of each land use type for this land unit. The continuous allocation (aka probabilistic allocation) can be configured with the [[loop]] operator, see for an example the Land Use Scanner Student Edition configuration.
2.  *discrete*: each unit of land is allocated to one out of a limited set of land use types. Use the discrete_alloc function, described in this page, for this allocation process.

## syntax

-   discrete_alloc(*TypeNames*, *LandUnitDomain*, *SuitabilityMaps*, *PartioningAttribute*, *PartioningName*, *AtomicRegions*, *AtomicRegionMap*, *MinClaims*, *MaxClaims*, *Threshold*, *FeasibleSolution*)

## definition

The [[discrete_alloc|Function discrete alloc]] function is used to find the discrete allocation of land use to land units that maximizes total suitability under the constrains of minimum and maximum claims.

## arguments

The discrete_alloc function has 11 [[arguments|argument]]:

-   *[[TypeNames]]*: an [[attribute]] with the name of each land use type, [[value type]]: string;
-   *[[LandUnitDomain]]*: the [[domain unit]] for which the allocation is calculated, value type: uint32;
-   *[[SuitabilityMaps]]*: a [[container]] with a suitability map for each land use type, value type: int32;
-   *[[PartioningAttribute]]*: an attribute that maps each land use type to a [[partitioning|relation]] id (a set of attributes with uint8 value type);
-   *[[PartioningName]]*: an attribute that maps each partitioning to a partitioning name such as "province" or "municipality", value type: string;
-   *[[AtomicRegions]]*: the domain unit defining the set of atomic regions, value type: uint16;
-   *[[AtomicRegionMap]]*: an attribute defining to which atomic region each land unit belongs, value type: uint16;
-   *[[MinClaims]]*: a container with for each land use type an attribute that defines for each region of the partitioning that belongs to that land    use type, the minimum number of land units that should be allocated to that land use type within the region, value type: uint32;
-   *[[MaxClaims]]*: a container with for each land use type an attribute that defines for each region of the partitioning that belongs to that land     use type, the maximum number of land units that should be allocated to that land use type within the region, value type: uint32. Make sure that the total
 claims are larger or equal to the number of cells in the analysis;
-   *[[Threshold]]*: a [[parameter]] defining the minimum suitability value that a land use type should have to get allocated, value type: int32;
-   *[[FeasibleSolution]]*: arbitrary container for possible future use (empty container).

## results

The function results in the following set of subitems:

-   **landuse**: the attribute containing the allocated land use types for the land units. The values unit of this attribute is set of land use types,     the domain unit is the LandUnitDomain. If the LandUnitDomain is configured as subset of an original land use [[grid]] domain, a [[lookup]] function can be used to relate the allocated results to the original land use grid (see the example).
-   **status**: a string parameter describing if the function resulted in an optimal solution or in not. In the latter case the reason why an optimal solution could not be reached is indicated.
-   **statusflag**: a boolean parameter with the value True if the function resulted in an optimal solution and False in all other cases.
-   **shadow_prices**: a container with [[subitems|subitem]] for each land use type. These subitems are attributes containing the shadow_prices for     each land use type. The value type of these prices is int32. The domain units are the domain units of the claims for these land use types.
-   **total_allocated**: a container with subitems for each land use type. These subitems are attributes containing the number of allocated land units for each land use type. The value type of these numbers is uint32. The domain units are the domain units of the claims for these land use types.
-   **bid_price**: an attribute containing the resulting bid prices of the allocation process. The values unit of this attribute is the same as the  values unit of the suitability maps, the domain unit is the LandUnitDomain.

## description

When applied iteratively and by incorporation of dynamic neighbourhood enrichment, one can simulate land use change caused by natural processes.

When applied iteratively with a feedback from future (neigbourhood dependend) yields on the current suitability, one can model a time consistent market equilibrium.

More information on the discrete allocation function can alse be found at our wiki.

## example
<pre>
container allocate_discrete := 
   discrete_alloc(
       lu_type/name
      ,Compacted/ADomain
      ,Compacted/SuitabilityMaps
      ,lu_type/partioning
      ,lu_type/PartioningName
      ,AtomicRegions
      ,Compacted/AtomicRegionsMap
      ,claims_min
      ,claims_max
      ,threshold
      ,FeasibleSolution
   )
 {
    attribute&lt;lu_type&gt; alloc (GridDomain) := <B>landuse</B>[Compacted/BaseGrid];
 }

</pre>
## full script

-   [[full discrete alloc script]]