*[[Grid functions]] raster_merge*

## syntax

- raster_merge(*target_grid_domain*, *valuesunit*,*sub_grid_data_item_1*, ..., *sub_grid_data_item_n*)
- raster_merge(*indexmap*, *valuesunit*, *valuesunit*,*sub_grid_data_item_1*, ..., *sub_grid_data_item_n*)

## definition

raster_merge(*target_grid_domain*, *valuesunit*, *sub_grid_data_item_1*,..., *sub_grid_data_item_n*) results in raster-data merged from the given sub_grid_data_items.

- The [[domain unit]] of the resulting [[data item]] is configured as first [[argument]].
- The [[values unit]] of the resulting data item is configured as second argument.

This variant can be used if the projection information can be derived from the domain units of the sub_grid domains and the cell size of the *target_grid_domain* equals the cell size of the sub_grid domains. This raster_merge variant can be used both to merge data from smaller sub_grid domains to a larger target grid domains or vice versa.

raster_merge(*indexmap*, *valuesunit*, *valuesunit*, *sub_grid_data_item_1*, ..., *sub_grid_data_item_n*) results in raster-data merged from the given sub_grid_data_items.

- The domain unit of the resulting item is equal to the domain unit of the first argument *indexmap*. This *indexmap* argument contains sequence numbers of the configured sub_grid_data_items, 0 refers to *sub_grid_data_item_1*, 1 to *sub_grid_data_item_1*, ..., n-1 to*sub_grid_data_item_n*..
- The values unit of the resulting data item is configured as second argument.

At locations with *indexmap* values >= n or outside the [[range]] of the domain unit of the indicated sub_grid, the result will be [[null]].

## applies to

- [[attribute]] *indexmap* with uint16 [[value type]]
- *valuesunit* must have a numeric value type
- *sub_grid_data_item_1*, ..., *sub_grid_data_item_n* must have the same values unit
- The value type of the *target_grid_domain* and the sub_grids must match.

## conditions

1. The *indexmap* argument must have a domain unit with a Point value type of the group CanBeDomainUnit.
2. all sub_grid_data_items must have a domain unit with a Point value type of the group CanBeDomainUnit.
3. all sub_grids must have a compatible projections as the *target_grid_domain* and the domain unit of the *indexmap* argument.

## since

- raster_merge(target_domain, valuesunit, subgrid_1, ..., subgrid_n): 7.101
- raster_merge(indexmap, valuesunit, subgrid_1, ..., subgrid_n): 7.013

## example

<pre>
attribute&lt;uint16&gt; indexmap (DomainM):=
  switch(
     case(pointRow(id(DomainM)) &lt;  2s && pointCol(id(DomainM)) &lt;  2s, 0)
    ,case(pointRow(id(DomainM)) &gt;= 2s && pointCol(id(DomainM)) &lt;  2s, 1)
    ,case(pointRow(id(DomainM)) &gt;= 2s && pointCol(id(DomainM)) &gt;= 2s, 2)
    ,3
  );

container SubGrids
{
   unit&lt;spoint&gt; DomainA := range(DomainM, point(0s,0s), point(2s,2s));
   unit&lt;spoint&gt; DomainB := range(DomainM, point(2s,0s), point(5s,2s));
   unit&lt;spoint&gt; DomainC := range(DomainM, point(3s,2s), point(5s,5s));
   unit&lt;spoint&gt; DomainD := range(DomainM, point(0s,2s), point(3s,5s));
}

unit&lt;uint8&gt; codes;

attribute&lt;codes&gt; ToBeMergedI   (SubGrids/DomainA) := const(10, SubGrids/DomainA, codes);
attribute&lt;codes&gt; ToBeMergedII  (SubGrids/DomainB) := const(20, SubGrids/DomainB, codes);
attribute&lt;codes&gt; ToBeMergedIII (SubGrids/DomainC) := const(30, SubGrids/DomainC, codes);
attribute&lt;codes&gt; ToBeMergedIV  (SubGrids/DomainD) := const(40, SubGrids/DomainD, codes);

Example I:
attribute&lt;codes&gt; raster_merged (DomainM) := 
   <B>raster_merge(</B>DomainM, codes, ToBeMergedI, ToBeMergedII, ToBeMergedIII, ToBeMergedIV<B>)</B>;

Example II:
attribute&lt;codes&gt; raster_merged (DomainM) := 
   <B>raster_merge(</B>indexmap,codes, ToBeMergedI, ToBeMergedII, ToBeMergedIII, ToBeMergedIV<B>)</B>;
</pre>

*indexmap*
|      |      |      |      |      |
|-----:|-----:|-----:|-----:|-----:|
| 0    | 0    | 3    | 3    | 3    |
| 0    | 0    | 3    | 3    | 3    |
| 1    | 1    | 3    | 3    | 3    |
| 1    | 1    | 2    | 2    | 2    |
| 1    | 1    | 2    | 2    | 2    |

*DomainM, nr of rows = 5, nr of cols = 5*

*ToBeMergedI*
|      |      |
|-----:|-----:|
| 10   | 10   |
| 10   | 10   |

*DomainA, nr of rows = 2, nr of cols = 2*

*ToBeMergedII*
|      |      |
|-----:|-----:|
| 20   | 20   |
| 20   | 20   |
| 20   | 20   |

*DomainB, nr of rows = 3, nr of cols = 2*

*ToBeMergedIII*
|      |      |      |
|-----:|-----:|-----:|
| 30   | 30   | 30   |
| 30   | 30   | 30   |

*DomainC, nr of rows = 2, nr of cols = 3*

*ToBeMergedIV*
|      |      |      |
|-----:|-----:|-----:|
| 40   | 40   | 40   |
| 40   | 40   | 40   |
| 40   | 40   | 40   |

*DomainD, nr of rows = 3, nr of cols = 3*

**raster_merged**
|        |        |        |        |        |
|-------:|-------:|-------:|-------:|-------:|
| **10** | **10** | **40** | **40** | **40** |
| **10** | **10** | **40** | **40** | **40** |
| **20** | **20** | **40** | **40** | **40** |
| **20** | **20** | **30** | **30** | **30** |
| **20** | **20** | **30** | **30** | **30** |

*DomainM, nr of rows = 5, nr of cols = 5*

## See also

- [[merge]]
