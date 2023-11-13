*[[Unit functions]] BaseUnit*

## concept

1. BaseUnit is a type of [[values unit]], for which the [[metric]] and [[value type]] are explicitly configured.
2. BaseUnit() is a function to configure a values unit as base unit.

This page describe the BaseUnit() function.

## syntax

- BaseUnit(*metric*, *valuetype*)

## definition

BaseUnit(*metric*, *valuetype*) configures **values units as [[base units|base unit]]**, e.g. meter or second.

The metric of the new [[unit]] is configured as first [[argument]].

The value type of the new unit is configured as second argument.

## applies to

- [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *metric* as string value
- *valuetype* with any value type, although usually only value types of the NumericGroup are used in base units.

## example
<pre>
container units
{
   unit&lt;float32&gt; m  := <B>BaseUnit(</B>'meter', float32<B>)</B>;
   unit&lt;float32&gt; m2 := m * m;
}
</pre>
<I> result: configured base unit <B>m</B> and used in a derived unit m2.</I>
