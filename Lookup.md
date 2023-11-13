*[[Relational functions]] lookup*

## syntax

- lookup(*[[relation]]*, *values*)
- *values*[*relation*]
- *relation* -> *values*

## definition

lookup(*relation*,*values*) or *values*[*relation*] or *relation* -> *values* results in a <B>[[data item]] with the values of the [[argument]] *values* for the [[domain unit]] of the *relation* [[attribute]]</B>.

The *relation* argument defines which values are looked up in the domain unit of the *values* attribute.

The resulting data item has the same [[values unit]] as the *values* argument and the same domain unit as the *relation* argument.

## applies to

- *relation*: [[data item]] with the [[index numbers]] of the domain unit of the *values* attribute.
- *values*: attribute with the requested values to be looked up.

## description

The relation towards the domain unit of argument *values* can de derived from any foreign key attribute with the [[rlookup]] function.

The [[rjoin]] function can be used to relate on foreign key attributes, without the explicit use of index numbers.

## example

<pre>
attribute&lt;degrees&gt; LTemp (City) := <B>lookup(</B>City/Region_rel, Region/Temp<B>)</B>;
attribute&lt;degrees&gt; LTemp (City) := Region/Temp<B>[</B>City/Region_rel<B>]</B>;
attribute&lt;degrees&gt; LTemp (City) := City/Region_rel <B>-></B> Region/Temp;
</pre>

| City/Region_rel |**LTemp** |
|----------------:|---------:|
| 0               | **12**   |
| 1               | **11**   |
| 2               | **null** |
| 1               | **11**   |
| 3               | **14**   |
| null            | **null** |
| 3               | **14**   |

*domain City, nr of rows = 7*

| Region/Temp |
|------------:|
| 12          |
| 11          |
| null        |
| 14          |
| 13          |

*domain Region, nr of rows = 5*

## see also

- [[rlookup]]
- [[rjoin]]