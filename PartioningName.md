*[[discrete_alloc function|Allocation functions]], argument 5: PartioningName*

## definition

*PartioningName* is the fifth [[argument]] of the discrete_alloc function.

*PartioningName* is an [[attribute]] that maps each [[PartioningAttribute]] to a [[partioning|relation]] name.

The [[domain unit]] of this attribute is the set of land use types.

## applies

-   attribute *PartioningName* with value type: string.

## example

<pre>
unit&lt;uint8&gt; lu_type: nrofrows = 3
{ 
   attribute&lt;string&gt; PartioningName: ['Living','Working','Nature'];
}
</pre>