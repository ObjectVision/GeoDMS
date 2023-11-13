The GeoDMS uses a hierarchical structure for [[naming|tree item name]] [[tree items|tree item]] in a configuration.

*The **subitems** of item A are the items one level lower, for which tree item A is the [[parent item]].*

The leaves in the tree of a configuration have no subitems.

Sometimes a distinction is made between direct and indirect subitems. Direct subitems are a synonym for subitems as defined above.

Indirect subitems are subitems at lower levels.

## Subitem() function

The subitem function can be used to refer to a [[tree item]], see the example:

### example

<pre>
attribute&lt;uint32&gt; att (ADomain) := <B>subitem(</B>unique/Regions,'Values'<B>)</B>;
</pre>

results in a reference to the values subitem of unique/Regions