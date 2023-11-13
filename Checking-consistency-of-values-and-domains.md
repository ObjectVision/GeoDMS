The GeoDMS calculates with data items, which are arrays of values that map ordinals of a specific domain to values that are mostly assumed to be quantities or measures with a specific unit of a metric system.

When applying operations, the arguments are mostly checked for domain-consistency and consistency of the metric specification of the values, such as with the addition operator; also for some operations, the values unit of one argument is checked on domain-consistency with another argument, such as with the lookup operator.

Furthermore, for an attribute<values_unit> x(domain) with a calculation rule, the domain of the result of the calculation rule is checked on domain-consistency with the formal domain and the values unit of that result is checked on metric consistency with the formal resulting unit.
Domain consistency mainly implies that the calculation rule and range are equivalent and therefore implies metric consistency, but not vice versa.

This model implicitly assumes that values of attributes are quantities or measures of a cardinal number of a specified metric unit.

However, aside from such cardinal numbers, numeric values can also be ordinal numbers and nominal numbers, which are usually without specified metric. See also <https://www.mathsisfun.com/numbers/cardinal-ordinal-nominal.html>

Sets of ordinal number and sets of nominal numbers are also considered as units in the GeoDMS, whereas ordinal numbers can be the domain of other data items and a data item of nominal numbers can be used to define a new ordinal set with the [[Unique]] operator.

## Issue

Data items with calculation rules that result in ordinal [[relations|relation]] to other domains or nominal values can now be configured with a different [[values unit]] as the values unit of the resulting relation, without an error.

The following example illustrates this:

<pre>
unit&lt;uint8&gt; EnergyRegion: NrofRows = 30;
unit&lt;uint8&gt; Province:     NrofRows = 12;

attribute&lt;EnergyRegion&gt; EnergyRegion_rel (city) := modus(neighboorhoud/EnergyRegion_rel, neighboorhoud/city_rel);
attribute&lt;EnergyRegion&gt; province_rel     (city) := modus(neighboorhoud/province_rel, neighboorhoud/city_rel);
</pre>

The first [[data item]] has an [[expression]] resulting in an [[attribute]] with as [[domain unit]] *city* and as [[values unit]] *EnergyRegion*. This is correctly configured.

The second data item has an expression resulting in an attribute with as domain unit *city* and as values unit *Province*. Although *EnergyRegion* is configured as values unit of this second attribute, no error is generated, as the unit Province has the same metric (none) as the unit EnergyRegion.

## Domain unit metric?

The reason why no error is generated in the second attribute is that for both domain units *EnergyRegion* and *Province*, no [[metric]] is configured. A check on metric  consistency does not result in an error.

It is technically possible to configure also metric for domain units, with the [[baseUnit]]. If the unit  *EnergyRegion* and *City* were configured as base units, the second attribute  would result in an error. 
Although this would solve this issue, configuring a metric for domain units has multiple other issues and is not advised. It comes down to that quantities of specified units should not be confused with nominal or ordinal numbers.

## How to proceceed

In the GeoDMS, metric is used for consistency checks on quantities, [cardinal numbers](https://www.mathsisfun.com/numbers/cardinal-ordinal-nominal.html) of units of a metric system. Furthermore, coordinate projections are used to assess the consistency combining locational values.

A relation is a data item with an ordinal values unit, which is or can be the domain of other data items, for which other types of checks are useful. And then there are nominals, which can be used in unique, rlookup and operations that can deal with all (2nd argument of lookup).

To improve checks on ordinals and nominals, we first need to label units as such and work out consequences for operator argument consistency checks and calculation ruel consistency checks.

Related issue is the confusion between Population and population density in rasters with cells of equal area.