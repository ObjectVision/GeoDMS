*[[Unit functions]] lowerbound*

## syntax

- lowerbound(*unit*)

## definition

lowerbound(*unit*) results in **the minimum allowed value** for the *unit* [[argument]].

If a range is configured for a *[[unit]]*, the lowerbound function results in the minimum value of this range.

If not, the lowerbound results in the default minimum value for the [[value type]] of the *unit*.

## applies to

- unit *unit* with Numeric or Point value type

## example

<pre>
unit&lt;float32&gt; unit_defined            := range(float32, 2.0, 7.5);
unit&lt;float32&gt; lowerbound_unit_defined := <B>lowerbound(</B>unit_defined<B>)</B>;
</pre>

*result lowerbound_unit_defined = 2.0*


## see also

- [[upperbound]]
- [[boundcenter]]
- [[boundrange]]