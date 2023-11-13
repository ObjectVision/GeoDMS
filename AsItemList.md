*[[String functions]] AsItemList*

## syntax

- AsItemList(*string_dataitem*)
- AsItemList(*string_dataitem*, *relation*)

## definition

- AsItemList(*string_dataitem*) results in a string [[parameter]] with **all values** of *string_dataitem*, **comma delimited**.
- AsItemList(*string_dataitem*, *relation*) results in a string [[attribute]] with all values of *string_dataitem*, , comma delimited, grouped by the     [[relation]] [[data item]]. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *[[relation]]* [[argument]].

## applies to

- data items *string_dataitem* with string [[value type]]
- *relation* with value type of the group CanBeDomainUnit

## conditions

The domain units of the arguments *string_dataitem* and *relation* must match.

## example

<pre>
1. parameter&lt;string&gt; CityListParam := <B>AsItemList(</B>City/Name<B>)</B>; 
<I>result = 'Groningen,Delfzijl,Winschoten,Leeuwarden,Dokkum,Bolsward,Emmen,Assen,Hoogeveen'</I>

2. attribute&lt;string&gt; CityList (Region) := <B>AsItemList(</B>City/Name, City/Region_rel<B>)</B>;
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

| **CityList**                        |
|-------------------------------------|
| **'Groningen,Delfzijl,Winschoten**' |
| **'Leeuwarden,Dokkum,Bolsward**'    |
| **'Emmen,Assen,Hoogeveen**'         |

*domain Region, nr of rows = 3*

## see also

- [[AsList]]
- [[AsExprList]]
- [[AsDataString]]