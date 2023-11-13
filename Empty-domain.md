An empty domain is a [[domain|domain unit]] with zero elements.

The following example shows how such a domain and a set of [[attributes|attribute]] for this domain can be configured.

<pre>
unit&lt;uint32&gt; empty_domain : nrofrows = 0
{
   attribute&lt;uint32&gt; int_data         : [ ];
   attribute&lt;dpoint&gt; poly_data (poly) : [ ];
}
</pre>