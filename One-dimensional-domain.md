## table

The one-dimensional [[domain unit]] corresponds to what in a database usually is called a table. 
In the GeoDMS this domain unit defines the number of but also the order of elements.
[[Attributes|attribute]] in the table have the configured unit as domain unit.

Usually uint8 or uint32 [[value types|value type]] are used for one-dimensional domain units.

The uint8 value type is used for domains with a limited set of elements, e.g. for class units. The maximum number of elements of a uint8 domain is 254.

For domain units with more elements, up to 4.294.967.294 elements, use a uint32 value type. Other less frequently used value types as domain unit.

-   bool/uint2/uint4 types for domains with a very limited set of elements
-   uint64 in a 64 bits environment for domain with more than 4.294.967.294 elements.

See the column MaxValue in the [[value type]] table for their maximum number of elements. Note: there might be some issues with using bool/uint2/uint4 value type as domain, in that case the uint8 value type is advised.

It is advised to use value types with smaller sizes if possible and only bigger sizes if necessary, this limits the amount of memory used and increases the model performance.

## views

A set of [[relational functions]] is available to configure new domain units, based on the content of other items.

In [relational database](https://en.wikipedia.org/wiki/Relational_database) terms this relates to the concept of a view, but is not the same. In a  relational database, a view usually results in a collection of attributes, the definition of the view (number of elements and order) is implicitly derived from the query string resulting in the view.

In the GeoDMS the resulting domain unit needs to be configured explicitly with a [[relational function|relational functions]] or [[selection function|selection functions]] (for example with [[select_with_org_rel]] for a selection of data elements of an origin domain).

Examples
<pre>
unit&lt;uint32&gt; building
:   StorageName = "%SourceDataDir%/BAG/pand.shp"
,   StorageType = "gdal.vect"
{
   attribute&lt;fpoint&gt; geometry;
   attribute&lt;uint32&gt; buildingyear;
}

unit&lt;uint8&gt; ehs_class: NrOfRows = 8
{
    attribute&lt;uint32&gt; color:
   [
       rgb(255,  0,  0)
      ,rgb(255,128,  0)
      ,rgb(255,255,  0)
      ,rgb(128,128,128)
      ,rgb(  0,255,255)
      ,rgb(  0,  0,255)
      ,rgb(  0,255,  0)
      ,rgb(  0,  0,  0)
   ];
}
</pre>

In the first example (building) the data is read from a [Shapefile](https://nl.wikipedia.org/wiki/Shapefile), the number of
elements of the domain unit [[cardinality]] is derived from the number of records in the Shapefile.

In the second example (ehs_class) the NrOfRows property is configured to define the cardinality.

In both example [[subitem]] are configured (geometry, buildingyear and color). As these data item have their domain units as parent item, the domain unit is derived from the [[parent item]] and has to be configured explicitly.