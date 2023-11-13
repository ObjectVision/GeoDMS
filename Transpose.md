*[[Configuration examples]] Transpose*

Transposing a table means changing the rows to columns and columns to rows.

## example

<pre>
container PerActor := for_each_nedv(
   Classifications/Actors/name
  , 'PerActorType/' + Classifications/Actors/name + '/sum'
  , Classifications/WoningType
  , float32
 );

container PerWoningType := for_each_nedv(
    Classifications/WoningType/name
   , 'union_data(Classifications/Actors, '
     + replace(
         AsItemList('PerActor/'+Classifications/Actors/name+ 
                          '[Classifications/WoningType/V/@WT@]')'
                    ,'@WT@', Classifications/WoningType/Name)+')'
    , Classifications/Actors
    , float32
  );
</pre>