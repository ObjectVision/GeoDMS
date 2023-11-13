*Relational model versus Semantic arrays [[DML]]*

Joins relate tables/views to combine data from multiple tables/views in a new view, based on a [JOIN condition](https://en.wikipedia.org/wiki/Join_(SQL)).

A typical left join in our [[relation model|Relational model versus Semantic arrays]] would be to relate ZipCode information to Appartments. Therefor we first introduce a new table with ZipCode information:

[[images/Relation_left_join.png]]

The following SQL Statement relates the ZipCode information to all Appartments:

<pre>
<B>Select</B> Appartment.Id, Appartment.Street, Appartment.Number, Appartment.ZipCode
     , Appartment.Town, ZipCode.AverageTemperature
<B>From</B> Appartment
<B>Left</B> Join ZipCode <B>ON</B> Appartment.ZipCode = ZipCode.ZipCode
</pre>

resulting in the following data:

[[images/Relation_left_join_results.png]]

The first 5 attributes result from the Appartment table, the last attribute from the ZipCode table.

The resulting data is of the same domain as the Appartments table. In here the left join differs from the inner join. If an inner join was used, the last two rows (with the null values for average temperature) would not be part of the result.

Left/right joins are usually used to relate data to a defined domain with null values if the relating data is missing for the requested entries.

## GeoDMS

As left joins are mostly used to relate data to existing domains, there is usually no need to configure new domain units. The following example show how to configure the left join SQL Statement in GeoDMS syntax:

<pre>
unit&lt;uint32> resultdomain := src/Appartment
{
  attribute&lt;string&gt; AppartmentId       := src/Appartment/Id;
  attribute&lt;string&gt; Street             := src/Appartment/Street;
  attribute&lt;uint32&gt; Number             := src/Appartment/Number;
  attribute&lt;string&gt; ZipCode            := src/Appartment/ZipCode;
  attribute&lt;string&gt; AverageTemperature := rjoin(ZipCode, src/PC6/ZipCode, src/PC6/AverageTemperature);
}
</pre>

The [[rjoin]] function is used to relate [[attributes|attribute]] of the two [[domains|domain unit]]: Appartment and PC6.

## difference

The results differ between the left join SQL Statement and the GeoDMS rjoin function in case the ZipCode attribute in the ZipCode table would nog be unique. In that case the resulting dataset of the SQL Statement would not be of the same domain as the Apartment table, but would contain more records with each combination of matching ZipCodes in both tables. This can result in records explosion and is often not desired, but can still be derived within the GeoDMS by configuring a new domain unit first (see example II on the Inner Join page)

The rjoin function in the GeoDMS will always result in an attribute for the configured domain. If the ZipCode attribute in the ZipCode table would not be
unique, the rjoin function will result in the AverageTemperature of the first matching ZipCode.