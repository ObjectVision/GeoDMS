*[[discrete_alloc function|Allocation functions]], argument 7: AtomicRegionMap*

## definition

*AtomicRegionMap* is the seventh [[argument]] of the discrete_alloc function.

*AtomicRegionMap* is an [[attribute]] that defines **to which atomic region each land unit** belongs.

The [[values unit]] of this attribute must be the AtomicRegions [[unit]], the [[domain unit]] the LandUnitDomain unit.

## description

If the [[overlay]] function is used to derive a AtomicRegions unit, the resulting UnionData [[subitem]] of this function can be used as a base for an AtomicRegionMap, see the example.

The [[lookup]] operator is needed to relate the UnionData subitem to the LandUnitDomain for which the actual allocation is performed.

## applies to

attribute AtomicRegionMap with [[value types|value type]]:

-   [[uint8]] or
-   [[uint16]]

## example
<pre>
unit&lt;uint8&gt; lu_type: nrofrows = 3
{ 
   attribute&lt;string> PartioningName: ['Living','Working','Nature'];
}
container regionSets
{
   attribute&lt;regMaps/p1&gt; Nature  (DomainGrid) := regMaps/p1Map;
   attribute&lt;regMaps/p1&gt; Living  (DomainGrid) := regMaps/p1Map;
   attribute&lt;regMaps/p2&gt; Working (DomainGrid) := regMaps/p2Map;
}

unit&lt;uint16&gt;             AtomicRegions             := overlay(lu_type/PartioningName, DomainGrid, regionSets);
attribute&lt;AtomicRegions&gt; AtomicRegionMap (ADomain) := AtomicRegions/UnionData[ADomain/nr_orgEntity];
</pre>