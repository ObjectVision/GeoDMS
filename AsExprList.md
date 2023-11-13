*[[String functions]] AsExpr(ession)List*

## syntax

- AsExprList(*string_dataitem*)
- AsExprList(*string_dataitem*, *relation*)

## definition

- AsExprList(*string_dataitem*, *separator*) results in a string [[parameter]] with **all values** of *string_dataitem*, **semicolon delimited** and **single quoted**.
- AsExprList(*string_dataitem*, *separator*, *relation*) results in a string [[attribute]] with all values of *string_dataitem*, semicolon delimited and single quoted, grouped by the *[[relation]]* [[argument]]. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## applies to

- [[data item]] *string_dataitem* with string [[value type]]
- relation with value type of the group CanBeDomainUnit

## conditions

The domain units of arguments *string_dataitem* and *relation* must match.

## example
<pre>
1. parameter&lt;string&gt; CityListParam := <B>AsExprList(</B>City/Name<B>)</B>;
<I>result = 'Groningen';'Delfzijl';'Winschoten';'Leeuwarden';'Dokkum';'Bolsward';'Emmen';'Assen';'Hoogeveen'</I>

2. attribute&lt;string&gt; CityList (Region) := <B>AsExprList(</B>City/Name, City/Region_rel<B>)</B>;
</pre>

| City/Name    | City/Region_rel |
|--------------|----------------:|
| 'Groningen'  | 0               |
| 'Delfzijl'   | 0               |
| 'Winschoten' | 0               |
| 'Leeuwarden' | 1               |
| 'Dokkum'     | 1               |
| 'Bolsward'   | 1               |
| 'Emmen'      | 2               |
| 'Assen'      | 2               |
| 'Hoogeveen'  | 2               |

*domain City, nr of rows = 9*

| **CityList**                                    |
|-------------------------------------------------|
| **'Groningen**';**'Delfzijl**';**'Winschoten**' |
| **'Leeuwarden**';**'Dokkum**';**'Bolsward**'    |
| **'Emmen**';**'Assen**';**'Hoogeveen**'         |

*domain Region nr of rows = 3*

## see also

- [[AsList]]
- [[AsItemList]]
- [[AsDataString]]