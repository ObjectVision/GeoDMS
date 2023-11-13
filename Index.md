*[[Relational functions]] index*

## syntax

- index(*a*)

## definition

index(*a*) results in an index [[attribute]] based on the <B>sort order of the values in attribute *a*</B>.

If two values are identical in attribute *a*, the first occurring value receives the lowest [[index number|index numbers]].

## applies to

- attribute *a* with Numeric, Point, uint2, uint4, bool or string [[value type]]

## examples

### _1. defining the index_
<pre>
attribute&lt;Region&gt; indexTemp (Region) := <B>index(</B>Temperature<B>)</B>;
</pre>

| Temperature |**indexTemp** |
|------------:|-------------:|
| 12          | **2**        |
| 11          | **5**        |
| null        | **1**        |
| 11          | **3**        |
| 14          | **0**        |
| null        | **4**        |
| 14          | **6**        |

*domain Region, nr of rows = 7*


### _2. using the index to sort an attribute:_

<pre>
attribute&lt;degrees&gt; Temperature_sorted (Region) := Temperature<B>[indexTemp]</B>;
</pre>

| Temperature |indexTemp |**Temperature_sorted** |
|------------:|---------:|----------------------:|
| 12          | 2        |**null**               |
| 11          | 5        |**null**               |
| null        | 1        |**11**                 |
| 11          | 3        |**11**                 |
| 14          | 0        |**12**                 |
| null        | 4        |**14**                 |
| 14          | 6        |**14**                 |

*domain Region, nr of rows = 7*


### _3. using index and unique to make a new sorted domain:_

<pre>
unit&lt;uint32&gt; Region: nrofrows = 7
{
   attribute&lt;degrees&gt; Temperature :[12,11,null,11,14,null,14];
   attribute&lt;.&gt;       indexTemp   := index(Temperature);
}

unit&lt;uint32v Region_sorted := unique(Region/indexTemp )
{
   attribute&lt;float32&gt; Temperature:= (Region/Temperature [Region/indexTemp])[values];
}
</pre>

| Values|**Temperature**|
|------:|--------------:|
| 0     |**null**       |
| 1     |**null**       |
| 2     |**11**         |
| 3     |**11**         |
| 4     |**12**         |
| 5     |**14**         |
| 9     |**14**         |

*domain Region_sorted, nr of rows = 7*
