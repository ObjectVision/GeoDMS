Metric in the GeoDMS relates to the units of [measurement known from Physics](https://en.wikipedia.org/wiki/Unit_of_measurement). The
[International System of Units (SI)](https://en.wikipedia.org/wiki/International_System_of_Units) comprises a coherent system built around seven base units, an indeterminate number of unnamed coherent derived units, and a set of prefixes that act as decimal-based multipliers.

[[Values units|values_unit]] in the GeoDMS can be configured as [[base unit]] or [[derived unit]] by using [[expressions|expression]]. The metric of the unit is derived from the configured expression and is presented in the [[GeoDMS GUI]] > Detail Pages > General. In calculations with [[data items|data item]], base units and derived units can be used in the same manner.

The metric is useful to inform how values of data items need to be interpreted, but also to check inconsistencies in calculations.

For example, a data item expressed in meters can not meaningfully be summed with a data item expressed in seconds. This also apply to another data_item  expressed in kilometers, although this item could be summed with the data item expressed in meters, if it was first divided by 1.000 meter per kilometer. For data items defining quantities it is strongly advised (although not obligatory) to configure expressions for [[units|unit]], resulting in a metric. Although considered a little new in the beginning, it is key function of the GeoDMS to manage and check the metric of (in between) results. Consistency in dealing with the metric of units in calculations is a key issue in good modeling.

## without metric

Except from data items describing quantities, the GeoDMS also supports data items with [[index numbers]], numeric codes, string values or booleans. These 
 data items also need values units, but usually no metric. For these units there are two options:

Example 1: Explicitly configured values unit

<pre>
unit&lt;uint32&gt;        CityCode: cdf = "ProvinceClass/ClassBreaks";
attribute&lt;CityCode&gt; DutchCityCodes (City);
</pre>

Example 2: Value type as values unit

<pre>
attribute&lt;string&gt; Name (City);
</pre>

In the first example, a values unit is configured explicitly, without an expression. This option is useful if, although the unit has no metric, it is still relevant to configure the unit because of other relevant properties. In the example a [[cdf]] [[property]] is configured for the values unit, see also classifications.

In the second example, a value type is used directly as values unit. This option is advised if there is no need to configure a values unit explicitly as it contains no relevant characteristics. Boolean and string items are often configured this way.