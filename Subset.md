*[[Relational functions]] subset*

<pre>
Starting form version 8.6.0, we advice to use the select_with_attr_by_org_rel or 
select_with_attr_by_cond functions in stead of the subset function, for 2 reasons:
- it is easier to configure new attributes for the resulting domain.
- the select_with_attr_by_org_rel function results in a subitem org_rel, meeting our naming conventions.

In the future the subset function becomes obsolete.
</pre>

## syntax

- subset(*condition*)

## definition

subset(*condition*) results in a new [[domain unit]] with a [[relation]] to the entries of the domain unit of the condition, for which the values of the *condition* [[argument]] are true.

The resulting [[value type]] of the domain unit is derived from the domain unit of the *condition* argument:
- uint32 for conditions with uint32, boolean, spoint or wpoint [[value type]]
- uint8 for conditions with uint8 value type
- uint16 for conditions with uint16 value type
- uint64 for conditions with uint64, ipoint or upoint value type

The explicit *subset_uint32*, *subset_uint16* and *subset_uint8* functions can be used in the same manner as the subset function, to create a new domain unit with an explicit value type.

## description

The subset function generates a [[subitem]], named *Nr_OrgEntity*. This [[data item]] contains the [[relation]] towards the domain unit of the *condition* 
 argument.

The *Nr_OrgEntity* [[data item]] can be used in a [[lookup]] function to relate [[attributes|attribute]] to the new domain unit, see the example.

See this [[overview | selection operator comparison]] for when to choose which selection operator is feasable.

## applies to

- *condition* must be a boolean attribute or [[subexpression]] resulting in boolean values.

## selecting attributes of a table

The subset function results in a new domain unit. Attributes for this new domain need to be configured explicitly, see the example.

It is possible to configure all attributes (e.g. read from a .csv file) for the Subset domain unit with a script example presented below in the example, selecting all attributes paragraph.

## example

<pre>
unit&lt;uint32&gt; ZHCities := <B>subset(</B>City/RegionCode == 200<B>)</B>
{
   attribute&lt;string&gt; name := City/Name[Nr_OrgEntity];
}
</pre>

| City/RegionCode | City/Name |
|----------------:|-----------|
| 100             | Amsterdam |
| 200             | Rotterdam |
| 300             | Utrecht   |
| 200             | Den Haag  |
| 400             | Eindhoven |
| null            | Haarlem   |
| 400             | Tilburg   |

*domain City, nr of rows = 7*

| ZHCities/nr_OrgEntity | ZHCities/name |
|----------------------:|--------------:|
| 1                     | Rotterdam     |
| 3                     | Den Haag      |

*domain <B>ZHCities</B>, nr of rows = 2*

## example, selecting all attributes

For this purpose we advice the new [[select_with_attr_by_org_rel]] or [[select_with_attr_by_cond]] functions.

## see also

- [[select]]
- [[select_data]]