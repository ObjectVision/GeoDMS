*Relational model versus Semantic arrays [[DML]]*

The *Select Distinct ... From ...* statement is used to select unique occurrences of one or more [[attributes|attribute]] from a table.

## single attribute selection

Assume the following SQL Statement:

**Select Distinct** Town **From** Appartment

This statement can be applied on our [relation model](http://wiki.objectvision.nl/index.php/Relational_model_versus_Semantic_arrays), resulting in the following data:

[[images/Relation_select_distinct_from.png]]

For a SQL Distinct statement in the GeoDMS a new [[domain unit]] needs to be configured with the [[unique]] function. This function generates a [[subitem]] called Values, containing the unique occurrences of it's [[argument]], in alphabetic order.

GeoDMS configuration (the Appartment domain unit is configured in a src container):

<pre>
unit&lt;uint32&gt; Town := unique(src/Appartment/Town);
</pre>

## multiple attributes selection

Assume the following SQL Statement:

**Select Distinct** Town, ZipCode **From** Appartment

The GeoDMS [[unique]] function does not support multiple [[arguments|argument]]. Therefore concatenate the Town and ZipCode attributes, see the example:

<pre>
unit&lt;uint32&gt; TownZipCode := unique(src/Appartment/Town + '_' + src/Appartment/ZipCode);
</pre>