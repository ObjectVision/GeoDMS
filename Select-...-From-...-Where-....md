*Relational model versus Semantic arrays [[DML]]*

The *Select ... From ... Where ...* statement is used to select records from a table/view that meet the condition defined in the Where Clause.

In the GeoDMS first a new [[domain unit]] needs to be configured with the e.g. the [[subset]] function, resulting in a [[subitem]] called Nr_OrgEntity with as [[values unit]] the [[index numbers]] of the original domain. This Nr_OrgEntity [[attribute]] can be used to get the data from the original  domain for the new subset domain, using the [[lookup]] function (see examples).

## single attribute selection

Assume the following SQL Statement:

**Select** Street, Number, Zipcode, Town **From** Appartment **Where** Town = 'BTown'

This statement can be applied on our [[Relational model versus Semantic arrays]], resulting in the following data:

[[images/Relation_select_from_where1.png]]

GeoDMS configuration (the Appartment domain unit is configured in a src container):

<pre>
unit&lt;uint32&gt; singleAttSelection := 
   select_with_attr_by_cond(
      DDL_Create/Apartment
     ,DDL_Create/Apartment/Town == 'BTown'
   );
</pre>

In the example the [[select_with_attr_by_cond]] function is used to select elements from the DDL_Create/Apartment domain unit. The part _with_attr_ in the function name indicates all attributes from the first argument (DDL_Create/Apartment) become available for the selection domain. The condition is configured as second argument.

Other [[selection functions]] can also be used.

## multiple attributes selection

Assume the following SQL Statement:

**Select** Street, Number, Zipcode, Town **From** Appartment **Where** ZipCode = 'AA6681' **And** Number = 3

This statement can be applied on our relation model, resulting in the following data:

[[images/Relation_select_from_where2.png]]

GeoDMS configuration (the Appartment domain unit is configured in a src container):

<pre>
unit<uint32> MultipleAttSelection := 
   select_with_attr_by_cond(
      DDL_Create/Apartment
     ,DDL_Create/Apartment/ZipCode == 'AA6681' && DDL_Create/Apartment/Number == 3
   );
</pre>