*[[discrete_alloc function|Allocation functions]], argument 2: LandUnitDomain*

## definition

*LandUnitDomain* is the second [[argument]] of the discrete_alloc function.

*LandUnitDomain* is the [[domain unit]] of the set of land units to be allocated, usually a [[grid]] domain.

## description

The LandUnitDomain is often configured as a subset of a land use map grid domain (see the example).

The selection criteria specifies if a land unit will be allocated (free land cells) or not as it is e.g. not located in the study area or exogeneous land use like water.

If the number of free land cells is substantial less than the number of cells of the original land use [[grid]] domain, it is advised to configure the LandUnitDomain as a subset. This increases the performance of the discrete_alloc function.

## applies to

-   [[unit]] LandUnitDomain with [[value type]] group: CanBeDomainUnit

## conditions

The LandUnitDomain needs to match with the resulting [[attribute]] of the discrete_alloc function called landuse.

All SuitabilityMaps need to have the LandUnitDomain as domain unit.

The AtomicRegionMap need to have the LandUnitDomain as domain unit.

## example
<pre>
attribute&lt;boolean&gt; FreeLand (GridDomain) := InRegio;
unit&lt;uint32&gt;       ADomain               := Subset(FreeLand), label = "allocation domain";
</pre>