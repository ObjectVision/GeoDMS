*[[discrete_alloc function|Allocation functions]], argument 10: Threshold*

## definition

*Treshold* is the tenth [[argument]] of the discrete_alloc function.

Treshold is a [[parameter]] defining the **minimum suitability value that a land use type should have to get allocated**.

The [[values unit]] of this parameter needs to match with the [[values unit]] of the [[subitems|subitem]] of the SuitabilityMaps argument.

If a land unit has a suitability value less than the *Treshold* for all land use types, this land unit will not be allocated.

The return value from the discrete_alloc function for this land unit will be the value [[Null]].

## applies to

parameter  *Treshold* with value type: int32

## example

<pre>
parameter&lt;EurM2&gt; treshold := 0[EurM2];
</pre>