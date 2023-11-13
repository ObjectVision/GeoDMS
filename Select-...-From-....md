_Relational model versus Semantic arrays [[DML]]_

The basic *Select ... From ...* statement is used to select one or more attributes from a table. Assume the following SQL Statement:

**Select** Street, Number, Zipcode, Town **From** Appartment

This statement can be applied on our relation model, resulting in the following data:

[[images/Relation_select_from.png]]

For such a SQL statement, in the GeoDMS no new [[domain unit]] needs to be configured. The source domain is identical to the domain of the result. The SQL statement can be configured in the GeoDMS in two ways (the Appartment domain unit is configured in a src container):

*example 1:*

<pre>
container result
{
   attribute&lt;string&gt; Street  (src/Appartment) := src/Appartment/Street;
   attribute&lt;uint32&gt; Number  (src/Appartment) := src/Appartment/Number;
   attribute&lt;string&gt; ZipCode (src/Appartment) := src/Appartment/ZipCode;
   attribute&lt;string&gt; Town    (src/Appartment) := src/Appartment/Town;
}
</pre>

*example 2:*

<pre>
unit&lt;uint32&gt; resultdomain := src/Appartment
{
   attribute&lt;string&gt; Street  := src/Appartment/Street;
   attribute&lt;uint32&gt; Number  := src/Appartment/Number;
   attribute&lt;string&gt; ZipCode := src/Appartment/ZipCode;
   attribute&lt;string&gt; Town    := src/Appartment/Town;
 }
</pre>

In the first example a result container is configured. The [[subitems|subitem]] are the fields to be selected. As the resulting items are identical to the source items, the simple equal to (=) operator is used in the [[expressions|expression]]. The domain units of the resulting items are not their parent items, therefore they need to be configured explicitly.

in the second example first a resultdomain domain unit is configured, equal to the source domain (src/Appartment). The same subitems are now configured, but as their parent items are now a domain unit equal to the src domain, the domain units do not have to be configured explicitly. This second approach is advisable if multiple subitems are requested.

## Select * From ...

In SQL an * asterisk is used to refer to all fields of a table. In the GeoDMS attributes with the same domain unit can be configured at different locations in the configuration. The meaning of an * asterisk is less clear and therefore not used in the GeoDMS.

With the [[Subitem_PropValues]] / [[Inherited_PropValues]] / [[SubTree_PropValues]] it is possible to make a list of all (full) names of data items with the same domain unit in a container, branche or whole configuration. With a [[for_each]] expression such a list can be used to select all these items.