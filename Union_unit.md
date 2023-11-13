*[[Relational functions]] union_unit*

## syntax

- union_unit(*a*, *b*, ... , *n*)

## definition

union_unit(*a*, *b*, ... , *n*) results in a uint32 [[domain unit]], based on the domain units: *a*, *b*, .., *n*.

The number of elements of the new domain unit is the <B>sum of the number of elements</B> of the domain units: *a*, *b*, .., *n*.

## description

Use the [[union_data]] function to union [[data items|data item]] for new domain units.

The union_unit function results in a uint32 domain unit. 
Use a [[union_unit_uint8_16_32_64]] function to configure a domain of another [[value type]].

## applies to

Units *a*, *b*, .., *n* with value types from the group CanBeDomainUnit

## example

<pre>
unit&lt;uint32&gt; HollandCity := <B>union_unit(</B>NHCity, ZHCity<B>)</B>;  Result = domain unit with 8 rows

unit&lt;uint32&gt; NHCity: nrofrows = 3;
unit&lt;uint32&gt; ZHCity: nrofrows = 5;
</pre>

## see also

- [[union_unit_uint8_16_32_64]]
- [[union_data]]