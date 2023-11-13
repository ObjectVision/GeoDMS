*Relational model versus Semantic arrays [[DML]]*

The *Select ... From ... Group By* [[DML]] statement is used to aggregate fields to a new [[domain unit]].

Assume the following SQL Statement:

**Select** Town, **Sum**(Surface) **AS** TownSurface **FROM** Appartment **GROUP BY** Town

This statement can be applied on our [[relation model|Relational model versus Semantic arrays]], resulting in the following data:

[[images/Relation_select_from_group_by.png]]

In the GeoDMS the aggregated domain unit first needs to be configured. It can consist of a set of known defined values (e.g. all months of the year or all cities in the region, 1) or be derived from the data (2).

<pre>
1. unit&lt;uint32&gt; DefinedTownDomain: nrofrows = 3
   {
      attribute&lt;string&gt; values: ['BTown','AVillage','CCity'];
   }
2. unit&lt;uint32&gt; DerivedTownDomain := unique(src/Appartment/Town);
</pre>

In this article the DefinedTownDomain will be used as aggregated unit.

To aggregate fields from the source (Appartment) domain towards the aggregated (DefinedTownDomain) domain, a [[relation]] is needed between these two domains. In the GeoDMS such a relation, also called a [[partitioning]], needs to consist of the [[index numbers]] of the aggregated [[domain unit]], for each entry in the source domain.

Relations  are often made with the [[rlookup]] function. This function relates an [[attribute]] in the source domain (in our example the
attribute: Town) to the values in the aggregated domain and results in the index numbers of the aggregated domain:

<pre>
attribute&lt;DefinedTownDomain&gt; DefinedTownDomain_rel (src/Appartment) := 
   rlookup(src/Appartment/Town, DefinedTownDomain/Values);
</pre>

This new attribute has as [[values unit]] the index numbers of the DefinedTownDomain and as domain unit Appartment. The postfix _rel is a 
[[naming convention|naming conventions]] to indicate the result is a relational attribute that can be used to aggregate data from the Appartment domain towards the DefinedTownDomain:

<pre>
attribute&lt;units/m2&gt; TownSurface (DefinedTownDomain) := sum(src/Appartment/Surface, DefinedTownDomain_rel);
</pre>

Resulting in the sum of the surfaces of the apartments per town. In CCity there are no apartments, so the result for this entry will be zero.

## geographic relations

For aggregations (GROUP BY Statements) relations are needed. These relations can be calculated with the [[rlookup]] function, if they can be derived from the source attributes.

In Geographic applications the relation is not always available in the source attributes, but can be derived from a Geographic relation. If the geography of the apartments (points) and of the towns (polygons) is known, the [[point_in_polygon]] function can be used to calculate a similar relation.

## multiple attributes

Assume the following SQL Statement:

**Select** ZipCode, Town, **Sum**(Surface) **AS** TownSurface **FROM** Appartment **GROUP BY** ZipCode, Town

In the GeoDMS, the [[rlookup]] and [[unique]] functions do not support multiple attributes. Therefore, to configure a SQL Group BY Statement with multiple Group BY fields, first concatenate these into one new attribute(see the [[Distinct]]) example) and use this new attribute in the rlookup / [[unique]] functions.

**Select** ... **From** ... **Where** ... **Group By** ... **Having** ... **Order By** ...

More complex Group By Statements in the GeoDMS are configured in multiple steps (see also Select From Where Order By). Use the following step order:

1. Define the **WHERE** Clause with a [[selection function|Selection-functions]] on the source domain.

2. Define the **GROUP BY** relation / [[aggregation|Aggregation functions]] on the selection domain of step 1 towards the aggregated  domain (see above).

3. Define the **HAVING** Clause with a subset on the aggregated domain. This is done in a similar way as the WHERE clause selection, only on the aggregated domain.

4. Define the **ORDER BY** Clause on the subset of the aggregated domain.