*[[Relational functions]] collect_by_cond*

## syntax

- collect_by_cond(*selection domain unit*, *condition*, *attribute*)

## definition

collect_by_cond(*selection domain unit*, *condition*, *attribute*) results in a **the values of [[attribute]] *attribute*, with the *selection domain unit* as [[domain unit]], for which the values of the *condition* [[argument]] are true**.
 

## applies to
* *selection domain unit*: a [unit](https://github.com/ObjectVision/GeoDMS/wiki/Unit) with [value type](https://github.com/ObjectVision/GeoDMS/wiki/Value-type) of the group CanbeDomainUnit
* *condition*: must be a boolean attribute or subexpression resulting in boolean values.
* *attribute*: can be any attribute with the same domain unit as the condition [[argument]] 

## since version
8.8.0

## example

<pre>
unit&lt;uint32&gt; City: StorageName = "city.csv", StorageType = "gdal.vect", StoragReadOnly = "True"
{
   attribute&lt;string&gt; name
   attribute&lt;uint32&gt; RegionCode;
}
unit&lt;uint32&gt; ZHCities := select_with_attr_by_cond(City, City/RegionCode == 200);

attribute&lt;string&gt; name (ZHCities) := <B>collect_by_cond(<B>ZHCities, City/RegionCode == 200<B>, City/name<B>)</B>;
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

|name          |
|--------------|
|**Rotterdam** |
|**Den Haag**  |

*domain <B>ZHCities</B>, nr of rows = 2*

## see also

- [[collect_attr_by_org_rel]]
- [[collect_attr_by_cond]]
