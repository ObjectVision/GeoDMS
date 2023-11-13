*[[Relational functions]] overlay*

## syntax

- overlay(*a*, *domainunit*, *container*)
- overlay32(*a*, *domainunit*, *container*)

## definition

- overlay(*a*, *domainunit*, *container*) results in a new uint16 [[domain unit]]
- overlay32(*a*, *domainunit*, *container*) results in a new uint32 domain unit

The first [[argument]] *a* contains the names of the source [[attribute]]. These attributes need to be configured in the [[container]] argument. 
Argument *domainunit* is the domain unit of the source and the resulting attributes.

The overlay(32) functions results in the following subitems:

- a [[subitem]] UnionData is generated for this domain unit, containing different [[index numbers]] of this domain unit for each unique combinations of the occurring values in the source attributes.
- subitems for each name of the source attributes from the argument *a*. These subitems contain [[relations|relation]] to the domain unit of the attributes need configured in the [[container]] argument.

## description

The overlay(32) function is for example used to create unique regions in the [[discrete allocation]] function. In the GeoDMS the [[unique]] function is used to find the unique values of one [[attribute]], the [[overlay32]] function can be used for multiple attributes.

## applies to

- attribute *a* with string [[value type]]
- domain unit *domainunit* with value type from group CanBeDomainUnit
- container *container* with as subitems the attributes to be overlayed. For each entry in the argument *a* an attribute with the same name need to be configured with a uint8 or unit16 value type and with *domainunit* argument as their domain units.

## conditions

The domain unit of all attributes to be overlayed need to match. [[Null]] values are not allowed in the source attributes.

## since version

5.51

## example

<pre>
unit&lt;uint8&gt; OverlayRegios: nrofrows = 2
{
   attribute&lt;string&gt; names: ['NoordZuid', 'OostWest'];
}
container OverlayGrids
{
  attribute&lt;uint8&gt; NoordZuid (GridDomain): StorageName = "%projdir%/data/overlayNZ.asc";
  attribute&lt;uint8&gt; OostWest  (GridDomain): StorageName = "%projdir%/data/overlayOW.asc";
}

unit&lt;uint16&gt; NoordZuidOostWest := <B>overlay(</B>OverlayRegios/names, GridDomain, OverlayGrids<B>)</B>;
</pre>

*NoordZuid*:

| | | | | |
|-|-|-|-|-|
|0|0|0|0|0|
|0|0|0|0|0|
|1|1|1|1|1|
|1|1|1|1|1|
|1|1|1|1|1|

*GridDomain, nr of rows = 5, nr of cols = 5*


*OostWest:*

| | | | | |
|-|-|-|-|-|
|0|0|0|1|1|
|0|0|0|1|1|
|0|0|0|1|1|
|0|0|0|1|1|
|0|0|0|1|1|

*GridDomain, nr of rows = 5, nr of cols = 5*


*NoordZuidOostWest/UnionData:*

|     |     |     |     |     |
|-----|-----|-----|-----|-----|
|**0**|**0**|**0**|**2**|**2**|
|**0**|**0**|**0**|**2**|**2**|
|**1**|**1**|**1**|**3**|**3**|
|**1**|**1**|**1**|**3**|**3**|
|**1**|**1**|**1**|**3**|**3**|

*GridDomain, nr of rows = 5, nr of cols = 5*

## see also

- full configuration example: [[Overlay versus Combine data]]
- [[overlay32]]
- [[overlay64]]
