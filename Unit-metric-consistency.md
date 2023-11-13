## Unit_metric consistency in calculations

The GeoDMS controls the model logic by checking the [[metric]], [[domain units|domain unit]] and [[values units|values unit]] of each calculation step.

Three types of consistency checks are illustrated by the next example:

<pre>
container UnitConsistencies
{
   container Units
   {
      unit&lt;uint32&gt;  NrInhabitants      := BaseUnit('NrInhabitants', 'uint32');
      unit&lt;uint32&gt;  NrCows             := BaseUnit('NrCows', 'uint32');
      unit&lt;float32&gt; NrHa               := BaseUnit('NrHa', 'float32');
      unit&lt;float32&gt; NrInhabitantsPerHa := NrInhabitants / NrHa;
   }
   container RegionalInformation
   :  Using       = "Units",
   ,  StorageName = "%projDir%/data/regionalinfo.mdb"
   {
      unit&lt;uint32&gt; Country: SqlString = "SELECT * FROM Country ORDER BY CountryID"
      {
         attribute&lt;NrInhabitants&gt; Inhabitants;
         attribute&lt;NrCows&gt;        Cows;
         attribute&lt;NrHa&gt;          Area;
      }
      unit&lt;uint32> EU15:  SqlString = "SELECT * FROM EU15 ORDER BY EU15ID"
      {
         attribute&lt;NrInhabitants&gt; Inhabitants;
         attribute&lt;NrHa&gt;          Area;
      }
      container Calculations: Using = "RegionalInformation"`
      {
         attribute&lt;uint32&gt;             SumInhabitantsAndCows  (Country) := Country/Inhabitants + Country/Cows;
         attribute&lt;NrInhabitants&gt;      SumInhabitants         (Country) := Country/Inhabitants + EU15/Inhabitants;
         attribute&lt;NrInhabitantsPerHa&gt; NrInhabitantsPerHa     (Country) := Country/Inhabitants / Country/Area;
      }
   }
}
</pre>
## metric consistency

The [[metric]] indicates how the values of a data item need to be interpreted. A parameter e.g. does not only have a value 300, but a value of 300 meter (in which meter is the metric for this item).

The GeoDMS is aware for each [[operator & function|operators and functions]] if and how [[data items|data_item]] with a different metric can be combined. The parameter in meters can for instance not be summed with a parameter in seconds, but it can be divided by a parameter with as metric: seconds.

The functionality to check the metric in calculations is an important quality control aspect of the GeoDMS and already has traced multiple errors in incorrect factor and metric combinations. To facilitate this quality control, for each data item a metric needs to be configured. This metric is part of the configuration of the [[values unit]].

### error

In the example above, the first [[attribute]] in the Calculations container, *SumInhabitantsAndCows* raises the following error if the data item is requested. 

[[images/Unit_err1.jpg]]

The error generated is: "*Operator add Error: Arguments must have compatible units, but arg1 has Metric NrInhabitants and arg2 has Metric NrCows*".

This error is generated because the metric of both [[units|unit]] does not match (which is requested for the [+|add]] operator). It is not logical to add data items with a different metric. For each operator & function is defined if the data items need to match with regard to their metric, see the description of each operator & function. In case an [[expression]] is configured, conflicting with these rules, an error is generated. If it is still desired to add the number of cows with the number of people, they both should be configured in a values unit with the same metric (e.g. NrLivingCreatures).

## domain unit consistency

Tthe GeoDMS checks the consistency of the domain unit in calculations. Most operators and functions can only combine data items of the same domain
unit , except e.g. the relational functions

### error

The second attribute in the Calculations container, SumInhabitants, tries to add the attribute inhabitants per country with the attribute inhabitants for EU15. This is not a logical operation as only data items of the same domain unit can be logically summed. The following error is generated:

[[images/Unit_err2.jpg]]

The error generated is: "*Domain unification of /RegionalInformation/Country (: UInt32) with /RegionalInformation/EU15 (: UInt32) is not possible because of Domain incompatibiliy due to missing or different calculation rule*"

This error indicates an inconsistency as the two domain units (Country and EU15 in this example) differ, which is not allowed for the + operator. For each operator & function is defined if the data items need to match with regard to their domain units, see the description of each see the description of each operator & function. In case an expression is configured, conflicting with these rules, an error is generated.

Domain inconsistencies do not occur for data items combined with [[parameters|parameter]].

## value type consistency

Within the GeoDMS multiple value types are used. We advice to use a value type with the smallest size that fits to your data. This make calculations faster and less memory consuming.

Combining data items with multiple value types is not trivial. Therefore in many cases data items need to be casted to other value types, before they can be combined in expressions.

### error

The third derived attribute in the Calculations container, NrInhabitantsPerHa, seems to have a logical expression. Still an error is generated:

[[images/Unit_err3.jpg]]

The error generated is: "*div Error: Cannot find operator for these arguments. Possible cause: argument type mismatch. Check the types of the used arguments*."

This error indicates the GeoDMS can not divide directly two data items with different value types (in this case uint32 and float32).

For each operator & function is defined which value types can be combined. The applies section of the documentation of each operator/function refers to
the valid signatures, describing the allowed combinations. In case an expression is configured, conflicting with these rules, an error is generated. As the resulting data item in the example is configured with a float32 value type, the divide operator also requests float32 data_item.

The number of inhabitants supplier in the expression need to be converted to a float32 value type. See the [[conversion functions]]  for an overview of all conversion functions.

The following expression will solve the problem:

<pre>
:= float32(Country/Inhabitants) / CountryInfo/Area;
</pre>

With this expression the attribute NrInhabitantsPerHa can be calculated.