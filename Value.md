*[[Conversion functions]] value*

## syntax

- value(*a*, *values unit*)
- *a*\[*values unit*\]

## definition

The value(*a*, *values unit*) function configures or converts **the values unit of data item or literal *a***.

## description

The value function is a synonym for the [[convert]] function.

The value function is usually used to define [[values units|values unit]] for new [[data items|data item]], the convert function to convert data items to other values units.

Since GeoDMS version 7.015 the value function can be used for [[coordinate conversions]] to convert the values of related [[geographic|Geography]] domains, for example to make a [[relational attribute|relation]] from a [[vector|vector data]] to a [[grid]] domain.

## applies to

- data item with Numeric, Point, uint2, uint4 or bool [[value type]]

## conditions

1. The value function is not (yet) implemented to cast integer values units to strings (use the [[string]] function instead).
2. The number of dimensions of the resulting value type should match with the source value type (e.g. a one-dimensional data item can not be converted with the value function to a two-dimensional data type).

## since version

- 5.15
- syntax: *a*[*values unit*]: since 5.97

## example
<pre>
1. parameter&lt;euro&gt; amount := <B>value(</B>7, euro<B>)</B>;
2. parameter&lt;euro&gt; amount := 7<B>[</B>euro<B>]</B>;
</pre>

With these expressions, the GeoDMS becomes aware that the values of these data items are expressed in euros (with the value type and [[metric]] as configured for the euro values unit).

The data item can now be e.g. be summed with other data items expressed in euros, but not with other data items expressed e.g. in dollars or kg.

## see also

- [[convert]]