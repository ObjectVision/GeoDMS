In modelling with data, it is often useful to apply the same model logic on different sets of parameters. The GeoDMS therefore uses templates.

*A template is defined as a set of calculation rules (model logic) with at least one [[case parameter]].*

A template can be compared with a function in a programming language.

In a template [[data items|data item]] can not be calculated, as the value(s) for the [[case parameters|case parameter]] are not known. To calculate the results, values need to be set for the case parameters.

*A case is defined as an instantiated template with values set for each case parameter.*

With the GeoDMS results of cases can easily be compared, as results of multiple cases can be available as different [[tree items|tree item]] simultaneously.

## how to configure a template

The following example shows a template used to:

-   Calculate the farest distance of two distance attributes and
-   [[Instantiate another template|Case instantiation]] that aggregates the results to different spatial levels

<pre>
Template CombineDistWithANDcond
{
  <I>// begin case parameters</I>
  attribute&lt;meter&gt; dist1 (bag/vbo);
  attribute&lt;meter&gt; dist2 (bag/vbo);
  <I>// end case parameters</I>

  attribute&lt;meter&gt; farest_dist (bag/vbo) := max_elem(dist1, dist1);

  container aggregations := Aggtemplate(farest_dist);
}
</pre>
## name

The template CombineDistWithANDcond is the parent item of the template and also in use as name of the template.

Don't use [[function|Operators and functions]] names for templates.

## case parameters

In the example two case parameters are configured: dist1 and dist2. Case parameters are always configured as the first [[subitems|subitem]] of the template.

## model logic

After the configuration of the case parameter, an [[attribute]] and a [[container]] are configured. These items contain the model logic of this template. The case parameters are [[suppliers|supplier]] of the first attribute. This first attribute is supplier in the [[expression]] of the second item, the
container aggregations. This container calls another template, called Aggtemplate and uses the first attribute as case parameter for this Aggtemplate to
configure a case instantiation.

This way a nested structure of templates calling other templates can be set up.

## original syntax

Until GeoDMS version 7.120 the syntax:

<pre>container templateName: IsTemplate = "True" </pre>

was used.

This syntax is still supported, but the new syntax is advised as it is shorter.