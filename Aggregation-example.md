*[[Configuration examples]] Aggregation*

In modelling, data often need to be [aggregated](https://en.wikipedia.org/wiki/Aggregate_function). This example presents how to aggregate data with the GeoDMS from the [CBS](http://www.cbs.nl) neighborhood level towards the municipality level.

## domain units and relation

An aggregation in GeoDMS terms means data is aggregated from a source [[domain unit]] to a target domain unit  (usually with less entries).

To make the aggregation, a [[relation]] is needed that relates the source domain unit to the target domain unit.

*Example*

<pre>
container aggregation
{
   container units
   {
      unit&lt;uint32&gt;  nr_inh := BaseUnit('inhabitant', uint32);
      unit&lt;float32&gt; ratio  := float32(nr_inh) / float32(nr_inh);
      unit&lt;float32&gt; perc   := 100f * ratio;
   }
   unit&lt;uint32&gt; neighborhood
   : StorageName = "%SourceDataDir%/CBS/2017/neighborhood"
   , StorageType = "gdal.vect"
  {
     attribute&lt;string&gt;       code;
     attribute&lt;units/nr_inh&gt; nr_inhabitants
     attribute&lt;units/perc&gt;   perc_0_14_yr;

      <I>// defined(_def) variants used to set missing values to zero</I>
     attribute&lt;units/nr_inh&gt; nr_inhabitants_def := 
        nr_inhabitants &gt;= 0i ? nr_inhabitants[units/nr_inh] : 0[units/nr_inh] / 0;
     attribute&lt;units/perc&gt;   perc_0_14_yr_def   :=  (perc_0_14_yr &gt;= 0i && perc_0_14_yr &lt;= 100i) 
           ? perc_0_14_yr[units/perc] 
           : 0[units/perc] / 0f;

     attribute&lt;string&gt;       municipality_code := substr(neighborhood/code,0,5);
     attribute&lt;municipality&gt; municipality_rel  := rlookup(municipality_code, municipality/values);
  }
  unit&lt;uint32&gt; municipality := unique(neighborhood/municipality_code)
  {
     attribute&lt;units/nr_inh&gt; sum_nr_inhabitants         := 
       <B>sum</B>(neighborhood/nr_inhabitants_def , neighborhood/municipality_rel);
     attribute&lt;units/perc&gt;   mean_perc_0_14_yr          := 
       <B>mean</B>(neighborhood/P_00_14_JR_DEF, neighborhood/municipality_rel);
     attribute&lt;units/perc&gt;   mean_weighted_perc_0_14_yr := 
        <B>sum</B>(neighborhood/P_00_14_JR_DEF * float32(buurt/AANT_INW_DEF) , neighborhood/municipality_rel) 
      / <B>sum</B>(float32(buurt/AANT_INW_DEF), neighborhood/municipality_rel);
  }
}
</pre>
## explanation

-   The [[attributes|attribute]] <I>nr_inhabitants</I> and <I>perc_0_14_yr</I> are aggregated from the neighborhood towards the  municipality domain.
-   For that reason a *municipality_rel* [[relational attribute|relation]] is configured with as domain unit : <I>neighborhood</I> and as [[values unit]] : <I>municipality</I>. The data is calculated with the [[rlookup]] function.
-   For the nr_inhabitants attribute the actual aggregation is configured with the [[sum]] function, resulting in the <I>sum_nr_inhabitants</I> attribute.
-   For the perc_0_14_yr attribute two aggregations are configured:

1.  <I>mean_perc_0_14_yr</I> : the aggregation is configured with the [[mean]] function, resulting in the mean <I>perc_0_14_yr</I> attribute of all neighborhoods in a municipality.
2.  <I>mean_weighted_perc_0_14_yr</I>: the aggregation is configured with two sum functions, resulting in the weighted mean <I>perc_0_14_yr</I> attribute of all neighborhoods in a municipality. The nr_inhabitants is used as weight.

## see also

In relational databases, SQL Group By statements are often used for aggregations. See the example [[Select ... From ... Group By ...]] for more information.