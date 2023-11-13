*[[discrete_alloc function|Allocation functions]], argument 9: MaxClaims*

## definition

MaxClaims is the ninth [[argument]] of the discrete_alloc function.

This argument needs to refer to a [[container]] with as [[subitems|subitem]] [[attributes|attribute]] for each land use type.

These attributes define**the maximal amount of land units that need to be allocated** for the land use type per region.

The [[values unit]] for these attributes is the number of land units. The [[domain units|domain unit]] of these attributes are the domain units of the regions for which the claims are available.

## applies

The values unit of the each MaxClaim attribute with [[value type]]: uint32

## conditions

The names of the MaxClaims attributes need to match with the values of the TypeNames argument.

## example
<pre>
container region
{
   unit&lt;uint8&gt; p1: nrofrows = 1;
   unit&lt;uint8&gt; p2: nrofrows = 2;
}
container claim_source
{
   unit&lt;float32&gt;      Meter      := BaseUnit('m', float32);
   unit&lt;float32&gt;      Ha         := 10000.0 * Meter * Meter;
   parameter&lt;float32&gt; nrHaPerCel := 1[claim_sources/Ha];
   
   container p1
   {
      attribute&lt;Ha&gt; Nature_min (region/p1): [12];
      attribute&lt;Ha&gt; Living_min (region/p1): [5];
   }
   container p2
   {
       attribute&lt;Ha&gt; Working_min (regionMaps/p2): [6,2];
   }
 }
container claims_max: Using = "claim_source"
 {
    attribute&lt;uint32&gt; Living  (region/p1) := uint32(p1/Living_min  / nrHaPerCel);
    attribute&lt;uint32&gt; Working (region/p2) := uint32(p2/Working_min / nrHaPerCel);
    attribute&lt;uint32&gt; Nature  (region/p1) := uint32(p1/Nature_min  / nrHaPerCel);
}
</pre>