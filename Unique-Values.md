*[[Configuration examples]] Unique Values*

This example shows how to read data from a .csv file and make a [[domain unit]] with the unique values of an [[attribute]].

## example

<pre>
container SourceData
{
   unit&lt;uint32&gt; indicators
   : StorageName     = "%SourceDataProjDir%/Indicators/datapacakage.csv"
   , StorageType     = "gdal.vect"
   , StorageReadOnly = "True"
   {
      attribute&lt;string&gt;           HeatOption; <I>// HeatOption must be an attribute in the csv file </I>
      attribute&lt;HeatOptionUnique&gt; HeatOptionUnique_rel := rlookup(HeatOption, HeatOptionUnique/values);
   }

   unit&lt;uint32&gt; HeatOptionUnique := unique(indicators/HeatOption)
   {
      attribute&lt;uint32&gt; number := pcount(indicators/HeatOptionUnique_rel);
   }
}
</pre>
