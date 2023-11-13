Derived units are units expressed in [[base|base unit]] and/or other derived units. The [[expressions|expression]] of derived units contain multiplications and/or divisions with :

- scale factor(s), constants (1 cm is expressed as 0.01 * meter).
- other base or derived units (meter per seconde is expressed as meter / sec).

Always configure derived units with as expression how they are derived (do not configure them as base unit). This is necessary to make the GeoDMS aware of the [[metric]] of the unit, needed to check inconsistencies in calculations with [[data items|data item]], using these [[values unit]].

## examples

<pre>
unit&lt;float64&gt; kilometer       := 1000 * meter;
unit&lt;float64&gt; MeterPerSecondd := meter / float64(second);
</pre>

In this example two new values units are configured, both with value type: float64.

The kilometer unit is derived from the [[base unit]] meter. A scale factor of 1000 is used (interpreted as float64 value), the meter unit (see base units) is configured with value type float64 also, so no conversion function is needed to result in float64 value type.

The MeterPerSecond unit is derived from the two base units: meter and second. As the unit second was configured with a float32  [[value type]], this unit need to be converted with the float64 conversion function (the [[divide|div]] operator does support the division of a float64 and float32 unit or data item).

## configuration rules

1. Scale factors used in expressions need to be configured before units. A km unit need to be configured as 1000 * m, m * 1000 is not allowed. A centimeter unit need to be configured as 0.01 * meter, meter / 100 is not allowed.
2. The scale factor is needed to define the [[metric]] of a unit, in this metric this value is always interpreted as float64 value. It is therefore not needed (although not forbidden) to configure a scale factor with a value type (configuring 1000.0, 1000, 1000f, 1000d, .. * meter all result in a unit with the same metric).