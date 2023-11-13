

## syntax

- discrete_alloc(*arguments*)

## definition

The discrete_alloc [function](Operators-and-functions) is used to let the GeoDms to [discrete allocation](https://github.com/ObjectVision/LandUseModelling/wiki/Discrete-Allocation).

| #  | argument          | description                                                                                    | unit type | value type   | 
|----|-------------------|------------------------------------------------------------------------------------------------|-----------|--------------|
| 1  | TypeNames         | This attribute contains the land-use type names.                                               | attribute | string       |
| 2  | LandUnitDomain    | This domain unit defines the set of land units. It can be of any value type but usually defines a grid domain. | unit      | uint32       |
| 3  | SuitabilityMaps   | A container that contains an attribute for each land-use type that provides suitability for that land-use type for each land unit. | container | int32        |
| 4  | Partitionings     | maps each land-use type to a partitioning id                                                   | attribute | uint8        |
| 5  | PartitioningNames | maps each partitioning to a partitioning name, such as "Province" or "Municipality".           | attribute | string       |
| 6  | AtomicRegions     | Unit that defined the set of atomic regions. which are the smallest divisible regions of overlapping claim regions                                    | unit      | uint8/uint16 |
| 7  | AtomicRegionMap   | attribute that defines to which atomic region each land unit belongs.                          | attribute | uint16       |
| 8  | MinClaims         | minimum number of land units that should be allocated within that region to that land-use type | container | uint32       |
| 9  | MaxClaims         | maximum number of land units that should be allocated within that region to that land-use type | container | uint32       |
| 10 | Threshold         | minimum suitability value that a land use type should have to get allocated. A higher threshold allows fewer land-use types to get allocated which can result in no feasible solution remaining even when the [[FeasibilityTest]] succeeded. A value of <null> allows any suitability.                    | parameter | int32        |
| 11 | FeasibleSolution  | arbitrary container for possible future use                                                    | container | (empty)      |

## Result

The result of the discrete_alloc function is a container with:

-   attribute<LandUseType> landuse(domain) an attribute that gives per land unit the resulting [land use type](Land_Use_Type "wikilink")
-   parameter<Sring> status: a text that
-   parameter<bool> statusFlag.
-   container shadow_prices: a container with for each LandUseType an attribute<Price> %Lut.Name%(%Lut.ClaimRegions%).
-   total_allocated: a container with for each LandUseType an attribute<UInt32> %Lut.Name%(%Lut.ClaimRegions%).
-   bid_price

### status

Examples of possible resulting status texts are:

-   DiscreAlloc completed with a total magnified suitability of 48317397677 over 715179 land units = 67559.866379 per land unit, which is optimal.

This text means that an optimal allocation has been generated with an average magnified suitability of 67559.866379. Assuming a magnification factor of 100000, this represents an average suitability (aka transition potential) of 0.67559866379

-   ClaimRange(type 0 (Urban), Nuts2 3 (ITC3: Liguria)) = \[min 25883, max 26057\]; 26062 allocated for price {0, 0};

This indicates that 5 more land units were allocated than the max claim because no alternatives could be found for allocating urban land use without violating other restrictions.

### statusFlag

The statusFlag indicates whether a feasible allocation has been generated (without violating any claim or threshold).

### total_allocated

The total_allocated container presents a counting of the allocation results per [Land Use Type](Land_Use_Type "wikilink") per corresponding claim region.

This is similar to what can be produced with [reg_count] (Landuse, typeNames, RegionContainer, RegionRefs).

where

RegionContainer is a container with names partitionNames and values equal to the corresponding AtomicRegions/AR\[atomicRegionMap\] and

RegionRefs is defined as partitionNames\[partitioning\].

In future versions of the GeoDMS, the total_allocated sub-container could be removed or made optional, thus the usage of this part of the results is depreciated since they can easily be obtained by using reg_count explicitly.

## applies to

## since version

## example

```
container Disc_alloc := 
	discrete_alloc(
		  LandUsePreparation/CBSKlasse/name			// 1 string 	attribute
		, Input/Compacted/ADomain				// 2 uint32	unit
		, Input/Compacted/Suitability				// 3 int32	container
		, const(0[Partitioning], LandUsePreparation/CBSKlasse)	// 4 uint8	attribute
		, Partitioning/name					// 5 string	attribute
		, Input/AggRegio					// 6 uint16	unit
		, Input/Compacted/AtomicRegionMap			// 7 uint16	attribute
		, Input/Claims/minclaims				// 8 uint32	container
		, Input/Claims/maxclaims				// 9 uint32	container
		, threshold						// 10 int32	parameter
		, FeasibleSolution					// 11 (empty)	container
	);
```

## frequent error messages

- value 255 out of range of valid Atomic Regions
     - solution: check for undefined values in the AtomicRegionMap. There might be gaps between regions, which will become undefined with poly2grid.




## discussion

### shadow prices as a splitter

Assuming that there is one solution to the optimization problem, knowing the shadow prices that result from the allocation means that the allocation can be regenerated by simply taking the land use type j with the highest augmented suitability for each land unit i, thus j: S_ij + lambda_j \>= S_ik+lambda_k for all i,k.

The S_ij can be regarded as the coordinates of N points in a K-dimensional vector space. If we see this space from the perspective of the point given by the coordinates of -lambda_j, we see that the resulting allocation of land unit i is equal to the direction j that the point Si has the highest value above -lambda. The hyperplane given by j+lambda_j == k+lambda_k differentiates between allocation towards j respectively k and therefore the shadow prices can be regarded as a splitter in the K dimensional vector space of land unit suitabilities.

Therefore, within the context of the discrete_alloc function, the shadow prices are aka splitter.

For an elaborate description of the splitter perspective, the sampling and scaling, and proof of the complexity bound of O(N\*K\*log(K)) for the discrete_alloc function, see [Tokuyama, T. and Nakano, J. (1995) Efficient algorithms for the Hitchcock transportation problem. SIAM Journal on Computing 24(3): 563-578](http://dl.acm.org/citation.cfm?id=139440).

### Degeneration and SoS

Degenerate cases are cases where different allocations have the same total suitability, which means that there is a continuum of solutions to the optimization problem when X_ij are allowed to have any real value between 0 and 1.

The discrete_alloc uses [virtual perturbation](virtual_perturbation "wikilink"), also known as [(Simulation of simplicity: a technique to cope with degenerate cases in geometric algorithms, Edelsburnner et al.)](http://arxiv.org/PS_cache/math/pdf/9410/9410209v1.pdf) to decide on degenerate cases. Within the discrete_alloc function, each suitability S'_ij is not taken as S_ij but as S_ij + epsilon \* i \* j where epsilon is infinitely smaller than the smallest unit of S_ij. This implies that S_ij \< S_ik is equivalent to S'_ij \< S'_ik since by definition epsilon \* i \* (j-k) \< (S'_ik - S'_ij). Only when S_ij = S_ik while i\<\>k, the simulated perturbation makes a difference of epsilon \* i \* (k-j).

Also, the shadow prices internally have an explicit epsilon component which can make the difference between two otherwise equal land units.

### scaling

The discrete_alloc function uses a scaling method to find estimates for the shadow_prices before all data is processed. Instead of a random sample, the first n land units are taken from the total of N land units and it is up to the caller to provide the land units in such an order that the first n units are a representative sample of the total set. This can be done by reordering all suitabilities and claim region maps randomly.

The shadow prices are initially set to 0. The scaling starts by dividing N, the total number of land units to be allocated by the stepFactor 4 until n, the number of sample units is less or equal to 1000. Based on the first sample all claims are scaled by a factor (n / N) and all units are allocated to the highest augmented suitability, which initially equals the original unaugmented suitability.

Once an allocation of land use type j to land unit i results in an excess of the maximum claim for j, a procedure is started to maintain the feasibility of the maximum claims, called **UpdateSplitterDown** that tries to move the splitter given by the coordinates of the negative shadow price -lambda in such a way that one land unit less is allocated to j without violating other maximum claims. To achieve this, a heap is maintained for each facet(j,k) between the set of suitability vectors allocatable to j and the set of suitability vectors allocatable to k that contains all land units i that have been allocated to j but could be allocated to k. This heap is ordered on (S'_ij - S'_ik) where the ordering takes the virtual epsilon component into account. After each allocation of land unit i to type j, i is pushed into the heaps of all facets of j to other allocation alternatives k so that these alternatives are recorded in an accessible way for future reallocations. UpdateSplitterDown then searches the shortest path through the network of facets (as links with cost (S'ij + Lambda_j) - (S'ij -Lambda_k) ) and allocation alternatives k (as nodes) until an alternative without a bound claim is reached. Note that the ordered heaps give constant time complexity access to the cheapest reallocation alternative from j to k, that the link cost thereof is always positive and therefore [dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm) can be and is used.

After processing each of the land units of a (scaled version of an) allocation while maintaining the feasibility of the maximum claim restrictions, a reversed procedure called **UpdateSplitterDown** is called repeatedly to get sufficient land allocated to meet the minimum claims.

### Memory usage

#### overview

The usage of RAM memory or Virtual Address Space by the discrete_alloc function for allocating N land units (with a non-tiled domain) to K land use types is O(K\*N), to be more precise, it is as follows:

-   administration of land use types and claim region names: insignificant
-   administration of claims and their shadow prices and facets: PM.
-   suitabilityMaps: K page-files of N\*sizeof(Int32) are mapped into memory.
-   atomicRegionMap: 1 page-file of N\*sizeof(UInt16) is mapped into memory.
-   resulting landuse: 1 page-file of N\*sizeof(UInt8) is mapped into memory.
-   ordered heaps with reallocation options: used part bounded by (K-1)\*N\*sizeof(UInt32), but up to a double amount can be taken from the dynamic memory pool.

This totals to (4K+3)\*N bytes distributed over K+2 memory mapped page-files and up to 2 times (4K-4)\*N bytes for the ordered heaps. Calculating and storing the bid_prices per land unit (and the shadow_prices and total_allocated counts) requires an additional page file of N\*sizeof(Int32) bytes to be mapped into the Virtual Address Space, but this is done after the allocation completes and the ordered heaps have been cleared.

Given that in a Win32 process, approximately 1.5 GiB is available as heap addresses, the discrete_allocation without selective threshold is safe as long as (4K+3)\*N + 2\*(4K-4)\*N = (12K-5)\*N \< 1.5 GiB. With K=13 as in the [EuClueScanner](http://www.objectvision.nl/products/eucluescanner) this simplifies to 151N \< 1.5 GiB or roughly N \< 10 million allocatable land units.

#### memory usage of the ordered heaps

For each claim j that has an overlapping region with claim k, there is a facet with an ordered heap that registers reallocation options from j to k. The number of facets is bounded by K and the number of atomic regions to ...

The number of reallocation alternatives that are stored in these heaps is bounded by (K-1)\*N except that for performance reasons, the execution of a reallocation from j to k does not directly remove the other alternatives that became obsolete from the other facets of j.

The growth strategy of the ordered heaps is that when the required size overflows the reserved memory, a memory reallocation with double of this size is requested from the dynamic memory pool. This can result in an amount of memory reserved for the heaps that are double the amount that has been in use during any step of the discrete_alloc algorithm. Furthermore, the dynamic aspect of moving land unit indices from one facet to the other due to reallocations has not been thoroughly analyzed.

A strict threshold will reduce the amount of (re)allocation options and therefore the sizes of these ordered heaps.

#### Possible Improvement of Memory Usage

The heap growing strategy could be reduced to only allocating a fraction r, which reduces the bound of heap memory usage to (1+r) times the used parts but increases the maximum average number of copying reallocation options from 1 to approximately 1/r and it is likely that losses due to more fragmentation cancel the advantage of less memory reservation.

### Support for tiled data

The discrete_alloc function supports the domain of the suitability maps and atomicRegionMap to be tiled (also known as segmented) by processing subsets of subsequent tiles as scaled versions of the total allocation where the allocations of the tiles before the current are frozen. So the processing of each tile only considers alternatives for UpdateSplitterDown within that tile. This starts with only the first tile (aka tile 0), then the second tile (aka tile 1) with claims scaled to the number of land units of both tiles minus what has already been allocated in the first tile, etc. This we call the [[Tile By Tile Heuristic]].

With an unfortunate permutation of the domain, this can result in choices in the first tile(s) that are not optimal or even result in an [[Infeasible Solution]] when taking the alternatives of later tiles into account.

## see also