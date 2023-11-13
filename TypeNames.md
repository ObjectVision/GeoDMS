*[[discrete_alloc function|Allocation functions]], argument 1: TypeNames*

## definition

*TypeNames* is the first [[argument]] of the discrete_alloc function.

*TypeNames* is an attribute with the name of each land use type.

The [[domain unit]] of this attribute is the set of land use types.

## applies to

attribute TypeNames with [[value type]]: string

## conditions

The names of the land use types need to match with the name of the [[subitems|subitem]] of the following arguments of the discrete_alloc function:

-   *[[SuitabilityMaps]]*
-   *[[MinClaims]]*
-   *[[MaxClaims]]*

## example

<pre>
unit&lt;uint8&gt;> lu_type: nrofrows = 3
{
   attribute&lt;string&gt; Name: ['Living','Working','Nature'];`
}
</pre>