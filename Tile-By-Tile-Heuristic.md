The Tile by Tile Heuristic is a heuristic for the [[Discrete Allocation]], implemented by the Function: discrete alloc for processing large allocation problems, larger than what can fit into the Virtual Address Space of a 32-bit process.

The discrete_alloc function supports the domain of the suitabilityMaps and atomicRegionMap to be tiled aka segmented by processing subsets of subsequent tiles as scaled versions of the total allocation where the allocations of the tiles before the current are frozen. So the processing of each tile only considers alternatives for UpdateSplitterDown within that tile. This starts with only the first tile aka tile 0, then the second tile aka tile 1 with claims scaled to the number of land units of both tiles minus what has already been allocated in the first tile, etc.

During the processing of a tile, only that segment of the used suitability maps, the used atomic region map and the resulting landuse is FileMapped into the Virtual Address Space of the GeoDms process. The heaps for each facet also only store alternatives within that tile since reallocations of landuse in earlier tiles cannot be performed anymore since those tiles of the resulting landuse are no longer directly accessible.

After processing a tile, the heaps for all facets are cleared to make space for allocation alternatives of the next tile.

The shadow_prices aka spitter that result from allocating the scaled claims to a tile are used as starting point for a next tile since the random perputation of the land units among tiles guarantee that scaled versions of the allocation result in reasonable (but not necessarily perfect) splitters.

With an unfortunate permutation of the domain, this can result in choices in the first tile(s) that are not optimal or even results in an
Infeasible Solution when taking the alternatives of later tiles into account.

# expected error

To quantify the possible deviation from a perfect (feasible + optimal) allocation, lets assume there are:
- N Land Units which is assumed to be a multiple of
- T Tiles
- 2 Land Use Types: urban and other
- 1 Claim Region
- 1 Strict Equality claim for urban of U land units (and therefore N-U for other).

The difference (S_io - S_iu) between suitability S_iu for urban and S_io for other land use is considered as the comparative advantage of other land use

Note that there is 1 Urban facet facing land units with a comparative advantage of other land use (in the generalized case of k Land Use Types there are (k-1) of such Urban facets and k*(k-1) facets in total, more if there are more than 1 claim
regions).

For a perfect allocation there is one split of the comparative advantages separating urban land units from other land units with price Lambda_u (Lambda_o is assumed to be zero, since only 1 claim needs to be binding) and Lambda_u is chosen as such that there are exactly U land units with S_iu+Lambda_u \> S_io.

Now let's consider the scaled allocation of the first tile with n := (N / T) Land Units and a scaled claim for urban Land Use of (U / T).

Take <ins>*X*</ins> as the stochastic number of land units in the first tile that should be allocated to Urban, which is equal to taking n times a drawing without replacement from a pot of U urban cells and (N-U) other cells, thus with an initial probability of (U/N) of drawing an Urban land unit. The expected value (the mean) of X then is n*(U/N) = U/T, since X has the [Hypergeometric distribution](http://en.wikipedia.org/wiki/Hypergeometric_distribution) and

<m>P(underline{X}=k) = {(matrix{2}{1}{U k}) (matrix{2}{1})} / (matrix{2}{1}{N n})</m>.

Thus the variance of <ins>*X*</ins> is <m>{u (1-u) n (N-n)}/{N-1}</m> with <m>u := U/N</m>

and the standard deviation for <ins>*X*</ins> is the square root of (n-1)/n times the variance, which is <m>sqrt{{(n-1) u (1-u) (N-n)}/{N-1}}</m>

which (with larger N) approximates <m>sqrt{(1-T^{-1}) u (1-u) N}</m>

See how that approximates the [Binomial distribution](http://en.wikipedia.org/wiki/Binomial_distribution) when T becomes large.

And the probability of <ins>*X*</ins> to be exactly the claimed (U/T) is only ...

As a numeric example, take N = 2000000, U = 200000 and T = 2, thus n= 1000000 and u = 10%. Then the expected (standard) deviation from the scaled claim U/T = 100000 units in the first tile to be Urban in the in a perfect allocation is about 212 units.

Note that this implies that an expected amount of 212 units will be mis-allocated in the first tile to meet the scaled claim of U/T units and that these mis-allocations usually represent a small total comparative disadvantage compared to the same amount of cells that will be reversely mis-allocated in the subsequent tiles. However, if suitability decreases steeply beyond the splitter as can be the case for non-urban land use if transition from urban to non-urban is prohibited, any amount of mis-allocated units in a first tile may result in much lower total suitability or even infeasibility if insufficient units remain available in the last tile to be non-urban.

# possible improvements

1. To avoid the mentioned issues, a 64 bit version of the GeoDmsRun.exe would be useful that would process an allocation without resorting to tiling.
2. Within the limitations of allocating memory in a 32 bit process, one could possibly keep the allocation results of one or several already     processed tiles into the virtual address space and also keep (the upper parts of) the facet-heaps that refer to reallocation options within those tiles. This would require:
    - a change to the interpretation of the indices that are stored in the facet-heaps, since now they are relative to the beginning of the current tile.
    - inclusion of comparative advantages with these indices for processed tiles to allow the processed suitability map tiles to be released (for the current tile, the priority heaps only contain indices which are prioritized by looking at the corresponding suitability maps from which the comparative         advantages are (re)calculated dyanmically     
     - the formulation and testing of a selection criterion for maintaining reallocation options into the heap (now only the absolute threshold works as a gatekeeper for allowing alternatives).
3.  An alternative improvement would be to first process all tiles separately (as is done now) and the combine the found tile-splitters. Note that the true splitter must be within the convex hull formed by the tile-splitters (TODO: prove this). Then rerun the allocation tile by tile by using the convex hull as a threshold: alternatives that are outside the hull are never useful for the to be found true splitter. This would require:
    - a fast way to determine if a point is in (or acceptably nearby) a convex hull of T (k-1)-points or its bounding box thereof.
    - some indication that this would succifiently filter the available alternatives to compensate the extra required memory for storing the selected reallocation alternatives including the comparative disadvantage for all tiles.
    - An adaptation of the current Solve to deal with this inter-tile-heaps.

See also:

- [A shortest augmenting path algorithm for dense and sparse linear assignment problems, R. Jonker and A. Volgenant, COMPUTING Volume 38, Number 4, 325-340](http://www.springerlink.com/content/7003m6n54004m004/).