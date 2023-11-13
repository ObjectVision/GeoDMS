*[[Unit functions]] upperbound*

## syntax

- upperbound(*unit*)

## definition

upperbound(*unit*) results in **the maximum allowed value** for the *unit* [[argument]].

If a range is configured for a *unit*, the upperbound function results in the maximum value of this range.

If not, the upperbound results in the default maximum value for the [[value type]] of the *unit*.

## applies to

- [[unit]] *unit* with Numeric or Point value type

## example

<pre>
unit&lt;float32&gt; unit_defined            := range(float32, 2.0, 7.5);
unit&lt;float32&gt; upperbound_unit_defined := <B>upperbound(</B>unit_defined<B>)</B>;
</pre>

*result upperbound_unit_defined = 7.5*

## see also

- [[lowerbound]]
- [[boundcenter]]
- [[boundrange]]