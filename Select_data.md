*[[Selection functions]] select_data*

## syntax

- *select_data*(*[[domain unit]]*, *condition*, *[[attribute]]*)

## definition

The select_data(domain unit, condition, attribute) function results relates the attribute [[argument]] to the *domain unit* argument, based on the  *condition* argument.

The select_data is mostly used together with the [[select_with_attr_by_cond]] function.

## applies to

- unit domain unit with [[value type]] [[uint32]]
- *condition* must be a boolean attribute or [[subexpression]] resulting in boolean values.
- attribute: an attribute that must have matching value type and [[metric]] with the resulting attribute.

## since version

7.305

## example
<pre>
unit&lt;uint32&gt; ZHCities := select_with_attr_by_cond(City, City/RegionCode == 200)
{
   attribute&lt;string&gt; name := 
      <B>select_data(</B>., City/RegionCode == 200, CityLabels/Name<B>)</B>;
}
</pre>

| City/RegionCode | CityLabels/Name |
|----------------:|-----------------|
| 100             | Amsterdam       |
| 200             | Rotterdam       |
| 300             | Utrecht         |
| 200             | Den Haag        |
| 400             | Eindhoven       |
| null            | Haarlem         |
| 400             | Tilburg         |

*domain City, nr of rows = 7*

| **ZHCities/name** |
|------------------:|
| **Rotterdam**     |
| **Den Haag**      |

*domain <B>ZHCities</B>, nr of rows = 2*

## see also

- [[select_with_attr_by_cond]]