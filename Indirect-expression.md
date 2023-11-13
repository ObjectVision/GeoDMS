In modelling the same modelling logic is often applied to different (sets of) parameters. A model might for instance be calculated for multiple years, regions etc.

In these cases you would like to configure the model logic once. For this purpose, the GeoDMS uses the concept of meta modelling, with indirect [[expressions|expression]].

An indirect expression is defined as an expression in which one or more elements are based on the contents of other items in the configuration. The resulting direct expressions are generated using the indirect expression logic and the items used in the configuration.

## syntax

<pre>
attribute&lt;bool&gt; total (rdc_100l) := ='or('+asItemList(ggEndogenous/Name)+')';
</pre>

An indirect expression always starts with an equals sign (**=**) character. In these expressions two elements are distinguished:

1. fixed, non generated elements, configured between single quotes. In the example the or function will be part of the evaluated expression.
2. generated elements, that need to result in a string. In the example the function asItemList will be evaluated and the results of this evaluation will be part of the evaluated expression.

## two step processing

An indirect expression is processed in two steps:

1. First the indirect expression is evaluated to a direct expression. In the example this evaluation results in the expression: _"or(woon_bc, woon_cd,..., agr_kweekerij)"_. Evaluated expressions are visible in the [[GeoDMS GUI]], Detail Page > General > CalculationRule.
2. The direct expression is calculated.

Indirect expressions are a powerful feature for the configuration of generic modelling logic. But be cautious with the indirect expressions, as they tend to make configurations more difficult to understand and to debug (see also [[Error-tracking-in-indirect-expressions|Error-tracking-in-indirect-expressions]])

## use in conditions

Indirect expressions can be used to configure a condition (iif function) with the following advantages:

1. In the evaluation of an [[iif]] function, both the then and else [[subexpressions|subexpression]] are calculated. If an indirect expression is used, the resulting expression will only contain the then or else subexpression, the other  statement does not have to be calculated.
2. Some functions like [[GeoDMSVersion]] are session-specific. This means all results of expressions directly or indirectly using the results of an expression in which these functions is used, will not be stored persistently and need to be recalculated each time the application is rerun. 

## be aware

The evaluation of indirect expressions is executed when the meta/scheme information is generated in the [[GeoDMS GUI]]. If for this evaluation (large) primary files are read, this becomes times consuming. Expanding tree items in the treeview becomes slow. Therefor we advice to use the contents of large primary data file (or complex calculations) as little as possible in indirect expressions.       