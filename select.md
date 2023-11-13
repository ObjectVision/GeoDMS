*[[Selection functions]] select*

## syntax

- select(*condition*)
- select_uint8(*condition*)
- select_uint16(*condition*)
- select_uint32(*condition*)
- select_uint64(*condition*)

## definition

Like the [[subset]] function, the select(*condition*) function results in a new [[uint32]] [[domain unit]] for which the values of the *condition* [[argument|argument]] are true.

The difference with the subset/[[select_with_org_rel]] function is that the select does <b>not</b> result in a generated subitem (*nr_OrgEntity* for subset, *org_rel* for select_with_org_rel).

This makes the select function useful to apply on large domain units for which the *nr_OrgEntity* or *org_rel* could use a substantial amount of memory.

Between versions 7.305 and 8.7.2 the name [[select_unit]] was used for the select function. 

The explicit *select_uint8*, *select_uint16*, *select_uint32* and *select_uint64* functions can be used in the same manner as the select function, to create a new domain unit with the explicit value type.

## description

Use the [[select_data]] function to relate [[attributes|attribute]] to the new domain unit, see the example.

## applies to

- *condition* must be a boolean attribute or [[subexpression]] resulting in boolean values.

## since version

8.8.0

## example

<pre>
unit&lt;uint32&gt; ZHCities := <B>select(</B>City/RegionCode == 200<B>)</B>
{
   attribute&lt;string&gt; name := select_data(., City/RegionCode == 200, City/Name);
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

| **ZHCities/name** |
|-------------------|
| **Rotterdam**     |
| **Den Haag**      |

*domain <B>ZHCities</B>, nr of rows = 2*

## see also

- [[subset]]
- [[select_data]]