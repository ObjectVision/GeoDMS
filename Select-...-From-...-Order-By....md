*Relational model versus Semantic arrays [[DML]]*

The *Select ... From ... Order By* statement is used to select one or more [[attributes|attribute]] from a table with an [Order By Clause](https://en.wikipedia.org/wiki/Order_by%7COrder) (sort criterium).

## single attribute ordering

Assume the following SQL Statement:

**Select** Street, Number, Zipcode, Town **From** Appartment **Order By** ZipCode

This statement can be applied on our [[relation model|Relational model versus Semantic arrays]], resulting in the following data:

[[images/Relation_select_from_order_by1.png]]

The GeoDMS works with semantic arrays, in which the sequence of elements matter. Therefore, for a SQL Statement with an Order By clause, in the GeoDMS a new [[domain unit]] needs to be configured. The new domain unit has the same cardinality as the source domain and is often configured with the [[range]] function (see the example).

For the source domain an index [[attribute]] need to be configured with the [[index]] function, with the attribute to be sorted as [[argument]]. In the example the index attribute is called ZipOrderAtt and results in the [[index numbers]] of the source domain unit in the sorted order of the ZipCodes.

The index attribute is used in the lookup functions for each requested attribute to get the elements in the new order. The [[lookup]] results have the same domain as the source domain, with the [[union_data]] function the data is converted to the new domain (ZipOrder).

GeoDMS configuration (the Appartment domain unit is configured in a src container):

<pre>
attribute&lt;src/Appartment&gt; ZipOrderAtt (src/Appartment) := index(src/Appartment/ZipCode);

unit&lt;uint32> ZipOrder := unique(ZipOrderAtt)
{
   attribute&lt;string&gt; Street  := (src/Apartment/Street[ZipOrderAtt])[Values];
   attribute&lt;uint32&gt; Number  := (src/Apartment/Number[ZipOrderAtt])[Values];
   attribute&lt;string&gt; ZipCode := (src/Apartment/ZipCode[ZipOrderAtt])[Values];
   attribute&lt;string&gt; Town    := (src/Apartment/Town[ZipOrderAtt])[Values];
}
</pre>

The resulting domain is sorted by ZipCode. For elements with the same ZipCode, the order in the source domain is maintained.

## multiple attributes ordering

Assume the following SQL Statement:

**Select** Street, Number, Zipcode, Town **From** Appartment **Order By** ZipCode, Number

The GeoDMS configuration is similar to the single attribute ordering example, with one exception. The index function in the GeoDMS does not support multiple arguments. Therefore concatenate the ZipCode and Number attributes as strings, see the example:

<pre>
attribute&lt;src/Appartment&gt; ZipNumberOrderAtt (src/Appartment) := 
   index(src/Appartment/ZipCode + '_' + string(src/Appartment/Number));
</pre>

This ZipNumberOrderAtt attribute can now be used in the [[lookup]] functions in the same way as the ZipOrderAtt in the single attribute ordering example.