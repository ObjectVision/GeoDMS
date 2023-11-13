*[[Configuration examples]] Crosstab*

A [crosstab](https://en.wikipedia.org/wiki/Contingency_table) is a type of table in a matrix format that displays the (multivariate) frequency distribution of variables.

## example

### SourceData

| **districtname** | **nr_inhabitants** | **avg_temperature** | **statename** | **year** |
|------------------|-------------------:|--------------------:|---------------|---------:|
| RegA             | 100                | 17°                 | StateI        | 2000     |
| RegB             | 200                | 22°                 | StateII       | 2000     |
| RegC             | 150                | 25°                 | StateII       | 2000     |
| RegD             | 50                 | 13°                 | StateIII      | 2000     |
| ...              | ...                | ...                 | ...           | ...      |
| RegX             | 250                | 19°                 | StateV        | 2010     |

Assume this data is read from a .[[csv]] storage, the GeoDMS configuration would look like this:

<pre>
unit&lt;uint32&gt; DistrictTimePeriod:
   StorageName      =  "%SourceDataProjDir/districs.csv"
,  StorageType      =  "gdal.vect"
,  StorageReadOnly  = "True"
{
   attribute&lt;string&gt;          districtname;
   attribute&lt;nr_persons&gt;      nr_inhabitants;
   attribute&lt;degrees_celsius&gt; avg_temperature;
   attribute&lt;string&gt;          statename;
   attribute&lt;string&gt;          year;
}
</pre>

## result

A potential crosstab, based on the source data, could look like this (each cell value is the sum of the number inhabitants for the state in the indicated year) :

| **statename / year** | **2000** | **..** | **2010** |
|----------------------|----------|--------|----------|
| StateI               | 500      | ...    | 800      |
| ..                   | ...      | ...    | ...      |
| StateV               | 250      | ...    | 230      |

## configuration steps

1) First, if not yet configured, configure state and TimePeriod as [[domain units|domain_unit]].

2) Configure [[relations|relation]] from the DistrictTimePeriod domain unit towards the configured state and TimePeriod domain units. This results in two extra [[attributes|attribute]] in the
DistrictTimePeriod unit configuration:

<pre>
unit&lt;uint32&gt; DistrictTimePeriod: ...
{
   ...
   attribute&lt;state&gt;      state_rel      := rlookup(statename, state/label);
   attribute&lt;TimePeriod&gt; TimePeriod_rel := rlookup(year,      TimePeriod/label);
}
</pre>

3) Apply the [[for_each]] function on the domain unit you would like to see as columns (in the example TimePeriod).

In the [[expression]] you aggregate the values towards the domain unit used for the rows, with a condition the data applies to the column values. The following examples show the configuration for the:

<I>Sum of inhabitants per State and TimePeriod (see result table)</I> :

<pre>
container CrossTab_SumInhabitants :=
   for_each_nedv(
        TimePeriod/name
      ,'sum(
           DistrictTimePeriod/TimePeriod_rel == ' + string(id(TimePeriod)) +'[TimePeriod] 
              ? DistrictTimePeriod/nr_inhabitants 
              : 0[nr_persons]
         , DistrictTimePeriod/state_rel)'
      ,state
      ,nr_persons
);
</pre>

In which TimePeriod/name is a string attribute with valid [[tree item]] names for the TimePeriod domain unit. In the sum the number of inhabitants is applied if the condition on the TimePeriod is true, if
not the value 0 is summed. This works well for quantities, but not for intensive variables like temperature. For these variables, use a missing value indication (for instance 0 / 0) in stead of zero, see next example:

<I>Mean Temperature per State and TimePeriod</I>

<pre>
container CrossTab_MeanTemperature :=
   for_each_nedv(
        TimePeriod/name
      ,'mean(
           DistrictTimePeriod/TimePeriod_rel == ' + string((id(TimePeriod)) +'[TimePeriod]
             ? DistrictTimePeriod/AverageTemperature 
             : (0 / 0)[degrees_celsius]
         , DistrictTimePeriod/state_rel)'
      ,state
      ,degrees_celsius
);
</pre>