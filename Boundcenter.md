*[[Unit functions]] boundcenter*

## syntax

- boundcenter(*unit*)

## definition

boundcenter(*unit*) results in **the mean value** for the *unit* [[argument]].

If a range is configured for a *[[unit]]*, the boundcenter function results in the minimum value of this range.

If not, the boundcenter results in the default minimum value for the [[value type]] of the *unit*.

## applies to

- unit *unit* with Numeric or Point value type

## example

<pre>
unit&lt;float32&gt; unit_defined             := range(float32, 2.0, 7.5);
unit&lt;float32&gt; boundcenter_unit_defined := <B>boundcenter(</B>unit_defined<B>)</B>;
</pre>

*result boundcenter_unit_defined = 2.0*

## see also

- [[upperbound]]
- [[lowerbound]]
- [[boundrange]]