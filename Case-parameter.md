Each [[template]] has at least one case parameter.

Case parameters are the [[arguments|argument]] of a template. Case parameters are configured as the first set of [[subitems|subitem]] of the 
template.

Case parameters in the GeoDMS can be any [[tree item]] (not only [[parameters|parameter]]). They are not explicitly marked in the GeoDMS syntax. Therefore it is a good habit to mark the begin and the end of the case parameters with comments, as in  this example.

<pre>
Template CombineDistWithANDcond
{
  <I>// begin case parameters</I>
  attribute&lt;meter&gt; dist1 (bag/vbo);
  attribute&lt;meter&gt; dist2 (bag/vbo);
  <I>// end case parameters</I>

  attribute&gt;meter&gt; farest_dist (bag/vbo) := max_elem(dist1, dist1);
  container aggregations := Aggtemplate(farest_dist);
}
</pre>

In this example dist1 and dist2 are the case parameters:

To actually use this template to calculate the CombineDistWithANDcond, values need to be set for the dist1 and dist2 attributes.

With a [[case instantiation]], values are set for each case variable and the template is used in a [[expression]].

## default values

An expression or [[primary data|configuration file]] can be configured for a case parameter. The default values are then used if in the case instantiation no values are set for the case parameter.

If case parameters are set in the case instantiation, they will overrule the default values.