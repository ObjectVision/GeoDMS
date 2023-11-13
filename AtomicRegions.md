*[[discrete_alloc function|Allocation functions]], argument 6: AtomicRegions*

## definition

*AtomicRegions* is the sixth [[argument]] of the discrete_alloc function.

*AtomicRegions* is a [[domain unit]] defining the set of atomic regions.

It contains [[attributes|attribute]] for each *[[PartioningAttribute]]* that define to which region each atomic region belongs for that *PartioningAttribute*.

## description

The overlay function is used to derive a *AtomicRegions* [[unit]] with the set of relevant [[subitems|subitem]] for the discrete_alloc function, see the example.

## applies to

unit AtomicRegions with [[value types|value type]]:
-   [[uint8]] or
-   [[uint16]]

## applies to

The names of the regions ([[subitems|subitem]] of the third argument of the [[overlay]] function need to match with the values of the [[PartioningName]] argument.

## example
<pre>
unit&lt;uint8&gt; lu_type: nrofrows = 3
{ 
   attribute&lt;string&gt; PartioningName: ['Living','Working','Nature'];
}
container regionSets
{
   attribute&lt;regMaps/p1&gt; Nature  (DomainGrid) := regMaps/p1Map;
   attribute&lt;regMaps/p1&gt; Living  (DomainGrid) := regMaps/p1Map;
   attribute&lt;regMaps/p2&gt; Working (DomainGrid) := regMaps/p2Map;
}
unit&lt;uint16&gt; AtomicRegions := overlay(lu_type/PartioningName, DomainGrid, regionSets);
</pre>
