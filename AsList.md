*[[String functions]] AsList*

## syntax

- asList(*string_dataitem*, *separator*)
- asList(*string_dataitem*, *separator*, *relation*)

## definition

- asList(*string_dataitem*, *separator*) results in a string [[parameter]] with **all values** of *string_dataitem*, separated by the ***separator***     [[argument]].
- asList(*string_dataitem*, *separator*, *relation*) results in a string [[attribute]] with all values of *string_dataitem*, separated by the *separator*
argument, grouped by the *[[relation]]* argument. The [[domain unit]] of the resulting attribute is the [[values unit]] of the *relation*.

## applies to

- [[data items|data item]] *string_dataitem* and *separator* with string [[value type]]
- relation with value type of the group CanBeDomainUnit

## conditions

The domain units of arguments *string_dataitem* and *relation* must match.

## example
<pre>
1. parameter&lt;string&gt; CityListParam := <B>AsList(</B>City/Name, ';'<B>)</B>;  
<I> result = 'Groningen;Delfzijl;Winschoten;Leeuwarden;Dokkum;Bolsward;Emmen;Assen;Hoogeveen'</I>

2. attribute&lt;string&gt; CityList (Region) := <B>AsList(</B>City/Name, ';', City/Region_rel<B>)</B>;
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
| **'Groningen;Delfzijl;Winschoten**' |
| **'Leeuwarden;Dokkum;Bolsward**'    |
| **'Emmen;Assen;Hoogeveen**'         |

*domain Region, nr of rows = 3*

## **see also**

- [[AsExprList]]
- [[AsItemList]]
- [[AsDataString]]