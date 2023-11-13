Modelling in GeoDMS terms is the process of writing/editing a (set of) configuration file(s). 
In these files usually [[source data|data source]] is read and [[calculations|expression]] are configured.

The [[GeoDMS GUI]] can be used as a tool in developing your model.

It is a good habit to also document your configuration.

## project

A GeoDMS project usually consists of:
-   (a set of) configuration files
-   source data
-   documentation

## configuration files

The GeoDMS uses a configuration language to configure source data, calculation steps, visualisation styles, export settings and relevant descriptive information. This information is stored in [[configuration files|configuration file]].

## declarative language

The GeoDMS configuration language is a [declarative](https://en.wikipedia.org/wiki/Declarative_programming) language. This means you configure what needs to be calculated and how the results needs to be presented. How calculations are performed in an efficient way is encapsulated in the implementation of each [[operator and function|Operators and functions]] that can be configured in an [[expression]].

## topics
-   [[Configuration Basics]]
-   [[Unit]]
-   [[Data source]]
-   [[Numeric data type]]
-   [[Expression]]
-   [[Geography]]
-   [[Relational model versus Semantic arrays]]
-   [[Template]]
-   [[Classification]]
-   [[Visualisation Style]]
-   [[Export]]
-   [[Naming Conventions]]
-   [[Value Type]]
-   [[Folders and Placeholders]]
-   [[Property]]