*Relational model versus Semantic arrays [[DML]]*

Joins relate tables/views to combine data from multiple tables/views in a new view, based on a [JOIN](https://en.wikipedia.org/wiki/Join_(SQL)) condition. A typical [inner join](https://en.wikipedia.org/wiki/Join_(SQL)#Inner_join) in our [[relation model|Relational model versus Semantic arrays]] would be to relate building information to the appartments that are related to these buildings, see the following SQL statement:

<pre>
<B>Select</B> Appartment.Id, Appartment.Street, Appartment.Number, Appartment.ZipCode
     , Appartment.Town, Building.ConstructionYear, Building.Footprint
<B>From</B>  Building
<B>Inner Join </B> 
 (Appartment Inner Join AppartmentBuildingRelation 
   On Appartment.Id = AppartmentBuildingRelation.AppartmentId)
<B>On</B> Appartment.Id = AppartmentBuildingRelation.AppartmentId <BR>
</pre>

resulting in the following data:

[[images/Relation_join_1.png]]

The first 5 [[attributes|attribute]] result from the Appartment table, the last 2 attributes from the Building table. The AppartmentBuildingRelation table is used to relate the tables in the Join condition.

The resulting view has the same number of rows as the AppartmentBuildingRelation table (as the combination of the AppartmentId/BuildingId in this table is unique).

## GeoDMS

A relation in the relational model is defined as a subset of the Cartesian product of n domains. The result of the inner join is a relation and therefore also such a subset. The resulting [[domain unit]] the GeoDMS of an [inner join](https://en.wikipedia.org/wiki/Join_(SQL)#Inner_join) SQL
statement can be configured with the [[combine]] function (to create the Cartesian product) and the [[subset]] (to make the relevant subset), see example 2.

In this case the subset domain is already configured in the GeoDMS. As mentioned, the resulting view from the inner join has the same number of rows and sequence as the AppartmentBuildingRelation table. Therefor the AppartmentBuildingRelation domain unit is the domain of the result, see example 1.

Example 1, resulting domain unit already configured (the AppartmentBuildingRelation domain unit is configured in a src container)

<pre>
unit&lt;uint32&gt; resultdomain := src/AppartmentBuildingRelation
{
  attribute&lt;string&gt; AppartmentId := src/AppartmentBuildingRelation/AppartmentId;
  attribute&lt;string&gt; BuildingId   := src/AppartmentBuildingRelation/BuildingId;
  attribute&lt;string&gt; Street       := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Street);
  attribute&lt;uint32&gt; Number       := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Number);
  attribute&lt;string&gt; ZipCode      := rjoin(AppartmentId, src/Appartment/id, src/Appartment/ZipCode);
  attribute&lt;string&gt; Town         := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Town);

  attribute&lt;units/Year&gt; ConstructionYear := rjoin(BuildingId, src/Building/id, src/Building/ConstructionYear); 
  attribute&lt;units/m2&gt;   Footprint        := rjoin(BuildingId, src/Building/id, src/Building/Footprint);
}
</pre>

The [[rjoin]] function is used to relate attributes of two domains, Appartments and Building, using a third domain: AppartmentBuildingRelation.

Example 2, resulting domain unit is configured with combine and select_with_org_rel functions

<pre>
unit&lt;uint32> CartesianProduct := combine(src/Appartment,src/Building)
{
   attribute&lt;string&gt; AppartmentId  := src/Appartment/id[nr_1];
   attribute&lt;string&gt; BuildingId    := src/Building/id[nr_2];
   attribute&lt;bool&gt;   JoinCondition := 
      isDefined(
         rlookup(
            AppartmentId + '_' + BuildingId
            ,  src/AppartmentBuildingRelation/AppartmentId + '_' + 
               src/AppartmentBuildingRelation/BuildingId
        )
    );
}

unit&lt;uint32&gt; ResultDomain := select_with_org_rel(CartesianProduct/JoinCondition)
{
   attribute&lt;string&gt; AppartmentId := CartesianProduct/AppartmentId[org_rel];
   attribute&lt;string&gt; BuildingId   := CartesianProduct/BuildingId[org_rel];
   attribute&lt;string&gt; Street       := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Street);
   attribute&lt;uint32&gt; Number       := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Number);
   attribute&lt;string&gt; ZipCode      := rjoin(AppartmentId, src/Appartment/id, src/Appartment/ZipCode);
   attribute&lt;string&gt; Town         := rjoin(AppartmentId, src/Appartment/id, src/Appartment/Town);

   attribute&lt;units/Year&gt; ConstructionYear := rjoin(BuildingId, src/Building/id, src/Building/ConstructionYear); 
   attribute&lt;units/m2&gt;   Footprint        := rjoin(BuildingId, src/Building/id, src/Building/Footprint);
}
</pre>
