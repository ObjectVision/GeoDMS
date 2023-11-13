*[[discrete_alloc function|Allocation functions]], argument 3: SuitabilityMaps*

## definition

SuitabilityMaps is the third [[argument]] of the  discrete_alloc function.

This argument needs to refer to [[container]] with as [[subitems|subitem]] [[attributes|attribute]] for each land use type.

These attributes define the suitability for the land use type for each land unit. It is advised to configure a monetary [[values unit]] (e.g. EuroPerHa) for these attributes.

The [[domain unit]] of these attributes need to be the LandUnitDomain.

## applies

The values unit of the each SuitabilityMap with value type:int32

## conditions

The names of the SuitabilityMap attributes need to match with the values of the TypeNames argument.

## example

<pre>
container source
{
   container Suitability
   {
     attribute&lt;EurM2&gt; Living (GridDomain): 
       [
        1 , 2,  5,  4,  3, 1,
        2,  5,  8,  9,  7, 3,
        4, 10, 12, 13, 12, 6,
        5, 11, 13, 14, 12, 6,
        4,  9,  9,  5,  3, 2,
        2,  2,  4,  3,  1, 1
       ];
       attribute&lt;EurM2&gt; Working (GridDomain):
       [
        1, 1, 2,  3,  4,  6,
        2, 3, 4,  6,  8,  9,
        2, 4, 9, 11, 12, 10,
        1, 3, 5,  9, 10,  6,
        2, 4, 5,  5,  3,  2,
        1, 1, 2,  1,  1,  1
       ];
       attribute&lt;EurM2&gt; Nature (GridDomain):
       [
        3, 3, 3, 2, 2, 2,
        3, 3, 2, 2, 2, 2,
        3, 2, 1, 1, 1, 1,
        3, 2, 1, 1 ,1, 2,
        3, 3, 2, 1, 2, 2,
        3, 3, 3, 3, 3, 3
       ];
    }
}

container Compacted
{
   unit&lt;uint32&gt; ADomain := select_with_org_rel(FreeLand = True), label = "allocation domain";

   unit&lt;uint32&gt; SuitabilityMaps := Adomain
   {
      attribute&lt;EurM2&gt; Living  := Suitability/Living[ADomain/org_rel];
      attribute&lt;EurM2&gt; Working := Suitability/Working[ADomain/org_rel];
      attribute&lt;EurM2&gt; Nature  := Suitability/Nature[ADomain/org_rel];
    }
}
</pre>