*[[MetaScript functions]] for_each_ind*

## syntax

- for_each_ind(*options*, *other_arguments*)

## definition

The for_each_ind(*options*, *other_arguments*) group, the successor of the [[for_each]] function group, represents a family of functions, used to **generate a set of [[items|tree item]]** for each element of a [[domain unit]].

The suffix _ind indicates the for_each_ind is an indirect variant, in which the first [[argument]] *options* is a string of characters with the following options:<B>
n[e|t][d[n]v[n]][l][d][a[t][r]][s][c][u].

In this notation the name characters within brackets are optional.

The | character indicates one of the two namecharacters split by this | character need to be selected.

The namecharacters specified need to match with the relevant postfixes.
They represent:

- **n**: **[[name|tree item name]]** for each new item;
- **e:** **[[expression]]** for each new item;
- **i**: **[[IntegrityCheck]]** for each new item;
- **t**: **[[template]]** instantiated for each new item;
- **d**\[n\]: **domain unit** for each item, with optional a n for the name of the [[container]] where the domain units can be found, in case a set of different domain units are requested;
- **v**\[n\]: **[[values unit]]** for each item, with optional a n for the name of the container where the values units can be found, in case a set of different values units are requested;
- **x**\[n\]: **[[unit]]** spec for creating units.
- **l**: **label** for each new item;
- **d**: **description** for each new item;
- **a**: **[[storagename]]** for each new item, with an optional t for the storage type;
- **s**: **[[sqlstring]]** for each new item.
- **c**: **[[cdf]]** for each new item
- **u**: **url** [[Property]] for each new item

## description

A for_each_ind function is used to derive a set of new tree items, based on the occurring values of [[data items|data item]] in a domain unit.

Different data items of the same domain unit can be used to specify the name, label, description etc. for the new set of tree items.

A [[template]] is used if the new items also request [[subitems|subitem]]. Often an expression is configured for the new items, in case for this expression elements from the origin domain are needed, the expression is usually [[indirect|indirect expression]].

If as domain unit a [[parameter]] is configured, a values unit is also obligatory and vice versa.

If a template is ONLY used to configure the domain and values unit for the resulting tree items, it is advised to use the domain and values unit [[arguments|argument]] in stead of a template (as in the example).

The attribute used for naming the new items should contain values that are allowed as item names. If not, an error is generated. Use the [[AsItemName]] function if your item names do not meet this condition. 

Starting from 7.163 the [[for_each]] function is in a process of being substituted by this for_each_ind function, more variants of the for_each function group will become obsolete with the new GeoDMS versions

## since version

7.163

## be aware

The evaluation of a for_each_ind is executed when the meta/scheme information is generated in the [[GeoDMS GUI]]. If for this evaluation (large) primary files are read, this becomes times consuming. Expanding tree items in the treeview becomes slow. Therefor we advice to use the contents of large primary data file (or complex calculations) as little as possible in the arguments of a for_each_ind function.   

## examples

<pre>
1: 
container regions := <B>for_each_ind(</B> 
       'nedvld'                                <I>// options </I>
      ,Relational/Region/Name                  <I>// name</I>
      ,'Relational/Region/NrInhabitants[rel]'  <I>// expression</I>
      ,void                                    <I>// domain unit</I>
      ,uint32                                  <I>// values unit</I>
      ,Relational/Region/Label                 <I>// label</I>
      ,Relational/Region/Descr                 <I>// description</I>
   <B>)</B>;
</pre>

| Name         | rel | NrInhabitants | Label | Descr |
|--------------|----:|--------------:|-------|-------|
| NoordHolland | 0   | 550           |       |       |
| ZuidHolland  | 1   | 1025          |       |       |
| Utrecht      | 2   | 300           |       |       |
| NoordBrabant | 3   | 300           |       |       |
| Gelderland   | 4   | 0             |       |       |

*result: Container regions with a parameter for each province, with as value the NrInhabitants for this province. The name, expression, domain and values unit, label and description for these parameters are determined by the for_each_nedvld function.*

<pre>
2: 
container FactorData := <B>for_each_ind(</B> 
       'ndvndacu'                                 <I>// options</I>
      ,MetaData/Factors/Name                      <I>// name</I>
      ,Geography/Albers1kmGrid                    <I>// domain unit</I>
      ,Units                                      <I>// container with configuration of values units</I>
      ,MetaData/Factors/ValuesUnit                <I>// values units</I>
      ,MetaData/Factors/Descr                     <I>// description</I>
      ,'%sourceDataProjDir%.ASC/'+FileName+'.asc' <I>// storage name</I>
  <B>)</B>;   
</pre>

*result: container Factordata with an attribute for each factor. These factors are read from .asc storages. The name, domain and values unit, description, storagename and cdf for these attributes are determined by the for_each_ndvndacu function. The values units read from the data item: MetaData/Factors/ValuesUnit(fourth argument) need to be direct subitems of the Units(third argument) container.*

<pre>
3: 
container RegionGrids := <B>for_each_ind(</B>
       'nedvn'                                           <I>// options</I>
      ,UniqueRegionRefs/Values                           <I>// name</I>
      ,'JrcFactorData/'+URR/Values+'[domain/grid_rel]'   <I>// expression</I>
      ,domain                                            <I>// domain unit</I>
      ,Geography/Regions                                 <I>// container with configuration of values units</I>
      ,UniqueRegionRefs/Values                           <I>// values units</I>
  <B>)</B>; 
</pre>

*result: container RegionGrids with an attribute for each UniqueRegionRefs. The name, expression, domain and values unit for these attributes are determined by the for_each_nedvn function. The values units read from the data item: UniqueRegionRefs/Values(fifth argument) need to be direct subitems of the Geography/Regions(fourth argument) container.*

<pre>
4:
container RegionGrids := <B>for_each_ind(</B>
      'ndvdatru'                                      <I>// options</I>
     ,period/Name                                     <I>// name</I>
     ,griddomain                                      <I>// domain unit</I>
     ,nrpersons                                       <I>// values unit</I>
     ,'urban population per cell'                     <I>// description</I>
     ,'%SourceDataDir%/pop/' + period/name + '.tif'   <I>// storage name (name of tiff files)</I>
     ,gdal.grid                                       <I>// storage type</I>
     ,true                                            <I>// StorageReadOnly status</I>
     , '%SourceDataDir%/meta/' + period/name + '.html' <I>// url, name of html files</I>
   <B>)</B>; 
</pre>

*result: container RegionGrids with an attribute for each period, read from a tif file per period. The StorageReadOnly property is set to True for each file. Furthermore descriptions and urls are configured for each subitem.*