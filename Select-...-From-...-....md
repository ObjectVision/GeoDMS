*Relational model versus Semantic arrays [[DML]]*

[Cross joins](https://en.wikipedia.org/wiki/Join_(SQL)#Cross_join) combine data from multiple tables/views in a new view with the Cartesian Product of the tables/views mentioned in the FROM part.

A cross join can be defined as an Inner join without join condition. The following SQL Statement will make the Cartesian Product of the Apartment and Building tables.


**Select** Apartment.Id, Apartment.Street, Apartment.Number, Apartment.ZipCode
         , Apartment.Town, Building.ConstructionYear, Building.Footprint<BR> 
**From** Apartment, Building 

resulting in the following data:

[[images/Relation_join_cross.png]]

The first 5 [[attributes|attribute]] result from the Apartment table, the last 2 attributes from the Building table.

## GeoDMS 
The resulting domain unit in the GeoDMS of an cross join SQL statement can be configured with the [[combine]] function, see the example:

## Example 

<pre>
  unit&lt;uint32&gt; CartesianProduct := combine(src/Appartment,src/Building)
  {
    attribute&lt;string&gt; AppartmentId  := src/Appartment/Id[nr_1];
    attribute&lt;string&gt; Street        := src/Appartment/Street[nr_1];
    attribute&lt;uint32&gt; Number        := src/Appartment/Number[nr_1];
    attribute&lt;string&gt; ZipCode       := src/Appartment/ZipCode[nr_1];
    attribute&lt;string&gt; Town          := src/Appartment/Town[nr_1];
 
    attribute&lt;units/Year&gt; ConstructionYear := src/Building/ConstructionYear[nr_2];
    attribute&lt;units/m2&gt;   Footprint        := src/Building/Footprint[nr_2];
 }
</pre>