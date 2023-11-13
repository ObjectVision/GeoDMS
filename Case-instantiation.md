A case or [[template]] instantion is defined as using a template in an expression and setting the values for all [[case parameters|case parameter]].

## configuration

*Example:*

<pre>
container Cases
{
    container FarestDistSuper_PT := CombineDistWithANDcond(DistSupermarket/dist, DistPublicTransport/dist);
}
</pre>

A case instantiation is configured with an [[expression]] configured to a [[container]]. In this expression the template name is used and the values for each case parameter are set between brackets.

Each case_parameter is separated by a comma.