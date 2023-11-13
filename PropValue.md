*[[Miscellaneous functions]] PropValue*

## syntax

- PropValue(*item*, *property*)

## definition

PropValue(*item*, *property*) results in a string [[parameter]] with the **value** of the ***[[property]]*** [[argument]] for the *[[item|tree item]]* argument.

## applies to

*item* can be any tree item, not being itself or one of it's [[subitems|subitem]] (an invalid recursion error is generated)

There is a list of all [[properties|property]].

## example

<pre>
attribute&lt;meter&gt; A (ADomain) := B + C,  descr = "A is the sum of B and C";
{
   parameter&lt;string&gt; name       := <B>PropValue(</B>A, 'name'<B>)</B>;       <I>result = 'A'</I>
   parameter&lt;string&gt; valuesunit := <B>PropValue(</B>A, 'ValuesUnit'<B>)</B>; <I>result = 'meter'</I>
   parameter&lt;string&gt; expr       := <B>PropValue(</B>A, 'expr'<B>)</B>;       <I>result = 'B + C'</I>
   parameter&lt;string&gt; descr      := <B>PropValue(</B>A, 'descr<B>)</B>;       <I>result = 'A is the sum of B and C'</I>
}
</pre>

## see also

- [[SubItem_PropValues]]
- [[Inherited_PropValues]]