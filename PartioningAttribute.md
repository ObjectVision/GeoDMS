*[[discrete_alloc function|Allocation functions]], argument 4: PartioningAttribute*

## definition

*PartioningAttribute* is the fourth [[argument]] of the discrete_alloc function.

*PartioningAttribute* is an [[attribute]] with the [[index number|index_numbers]] of each land use type.

The [[domain unit]] of this attribute is the set of land use types.

## applies

-   attribute *PartioningAttribute* with value type: uint8

## example
<pre>
unit&lt;uint8&gt; lu_type: nrofrows = 3
{ 
   attribute&lt;lu_type&gt; partioning := id(lu_type);
}
</pre>