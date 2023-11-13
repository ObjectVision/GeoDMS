*[[Unit functions]] ValuesUnit*

## concept

1. [[Values Unit]] is a [[unit]] with as role to define the [[value type]] and [[metric]] of the values of a [[data item]].
2.  ValuesUnit() is a function to get the Values Unit of an [[attribute]].

This page describes the ValuesUnit() function.

## syntax

- ValuesUnit(*a*)

## definition

ValuesUnit(*a*) results in a unit with a reference to the **[[values unit]]** of attribute *a*.

## example
<pre>
unit&lt;uint32&gt; RefADomain := <B>ValuesUnit(</B>A<B>)</B>;
</pre>

## see also

- [[DomainUnit]]
- [[PropValue]]