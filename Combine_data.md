*[[Relational functions]] combine_data*

## syntax

- combine_data (*combined_domain*, *a_rel*, *b_rel*)

## definition

combine_data(*combined_domain*, *a_rel*, *b_rel*) results in an [[attribute]] with [[index numbers]] to the *combined_domain* [[argument]], <B>combining the values of [[data items|data item]] *a_rel* and *b_rel*.</B>

## description

The first argument *combined_domain* is usually made with a [[combine]] function.

The combine_data function results in an attribute with an index to *combined_domain* for each element of the domain of *a_rel* and *b_rel* by as [[values unit]] the first [[argument]] of the function (*domainunit*) and as [[domain unit]] the domain unit of data items: *a*, *b*, .. *n*.

## applies to

- [[unit]] *combined_domain* with value type from the group CanBeDomainUnit.
- attributes *a_rel*, *b_rel* with value type from group CanBeDomainUnit.

## conditions

- The domain units of the data items *a_rel* and *b_rel* must match (or be all [[void]]).
- values unit of *b_rel* must match the 2<sup>nd</sup> argument of the *combine* or *combine_unit* with which the combined_domain has been defined.

## since version

7.131

## combining multiple indices

combine_data can only be used to combine two indices

When 3 (or more ) indices need to be combined, chain them:

- unit<uint32>   BC               := combine_unit(B,  C);
- unit<uint32>   ABC              := combine_unit(A, BC);
- attribute<ABC> ABC_rel (domain) := combine_data(ABC, domain/A_rel, combine_data(BC, domain/B_rel, domain/C_rel));

## example

<pre>
unit&lt;uint32&gt; NHCityYear := combine(NHCity, YearDomain);
attribute&lt;NHCityYear&gt; NHCityYear_rel (Adomain) := <B>combine_data(</B>NHCityYear, NHCity_rel, YearDomain_rel<B>)</B>;
</pre>

| NHCity_rel | YearDomain_rel | NHCity    | Year | **NHCityYear_rel** |
|-----------:|---------------:|-----------|-----:|-------------------:|
| 0          | 0              | Amsterdam | 1995 | **0**              |
| 0          | 1              | Amsterdam | 2005 | **3**              |
| 1          | 1              | Haarlem   | 2005 | **4**              |
| 2          | 0              | Alkmaar   | 1995 | **2**              |
| 2          | 1              | Alkmaar   | 2005 | **5**              |

*ADomain, nr of rows = 5*

| nr_1 | nr_2 | NHCityName | Year |
|-----:|-----:|-----------:|-----:|
| 0    | 0    | Amsterdam  | 1995 |
| 0    | 1    | Amsterdam  | 2005 |
| 1    | 0    | Haarlem    | 1995 |
| 1    | 1    | Haarlem    | 2005 |
| 2    | 0    | Alkmaar    | 1995 |
| 2    | 1    | Alkmaar    | 2005 |

*domain NHCityYear, nr of rows = 6*

| NHCity/name |
|-------------|
| Amsterdam   |
| Haarlem     |
| Alkmaar     |

*domain NHCity, nr of rows = 3*

| YearDomain/Year |
|-----------------|
| 1995            |
| 2005            |

*Yeardomain, nr of rows = 2*

## see also

- full configuration example: [[Overlay versus Combine data]]