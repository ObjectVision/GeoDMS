Base units are root [[units|unit]] from which other units can be derived. In [Physics](https://en.wikipedia.org/wiki/Physics) usually [SI units](https://en.wikipedia.org/wiki/International_System_of_Units) are used as base units. The GeoDMS is also used in other research areas, so also other base units can be configured, like monetary units (Euro, Dollar).

The following rules apply to the definition of base units:

-   If possible, use [SI units](https://en.wikipedia.org/wiki/International_System_of_Units) as base units,
-   See [[naming conventions]] for advised names.
-   Use other base units like monetary units, nrInhabitants, nrHouses etc. if it is relevant within your model to distinguish their quantities.

BaseUnits are configured with the [[BaseUnit]] function.

## example

<pre>
unit&lt;float32&gt; meter  := BaseUnit('meter','float64');
unit&lt;float64&gt; second := BaseUnit('seconde','float64');
</pre>

In the examples two [[values units|values unit]] are configured as base units. The [[value type|value type]] of the first unit is float32, of the second float64.

## be aware

The evaluation of a base unit is executed when the meta/scheme information is generated in the [[GeoDMS GUI]]. If for this evaluation (large) primary files are read, this becomes times consuming. Expanding tree items in the treeview becomes slow. Therefor we advice to use the contents of large primary data file (or complex calculations) as little as possible in configuring base units.   