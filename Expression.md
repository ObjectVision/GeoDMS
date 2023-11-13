The GeoDMS is intended to configure calculations, indicators, model components and chains. It offers a powerful set of [[operators and functions]] to configure and calculate the calculation/model logic in an efficient and transparent way. Quality control in the model logic is implemented by the management of dependencies and checking the consistency of calculations.

Although it is possible to execute external components with the GeoDMS, it is advised to configure especially the core components of a model in GeoDMS 
 operators & functions, to be able to use the full functionality of the GeoDMS modeling environment. External components keep their 'black box' character, which limit the transparency, efficiency and control of a the model configuration in the GeoDMS.

## characteristics
-   [[Management of dependencies]]
-   [[Constant state of data items]]
-   [[Explicit configuration of value types]]
-   [[Subexpressions|Subexpression]]
-   [[Unit metric consistency]]

## how to configure
-   [[Expression syntax]]
-   [[Indirect expressions|Indirect expression]]
-   [[External components]]

## fast calculations

The GeoDMS is used interactively, even with large data sets. This can only be achieved if calculations are performed fast, limiting the waiting time of the user for results.

Much attention in the development of the GeoDMS is being paid to efficient calculation processes. This is integral characteristic of the GeoDMS, taking into account the following aspects

-   [[data model]]
-   [[calculation management]]
-   [[algorithmic techniques]]
-   [[programming architecture]]
-   [[system architecture]]