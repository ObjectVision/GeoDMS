*[[Relational model versus Semantic arrays]] DML*

In the relational model a [DML (data manipulation language)](https://en.wikipedia.org/wiki/Data_manipulation_language) is used to select, insert, update or delete data in a database. [SQL](https://en.wikipedia.org/wiki/SQL) is one of the most used [DML](https://en.wikipedia.org/wiki/Data_manipulation_language) (as well as [DDL](https://en.wikipedia.org/wiki/Data_definition_language)) language.

The GeoDMS modelling language is a [functional programming language](https://en.wikipedia.org/wiki/Functional_programming).
Programming is done with [[expressions|expression]] instead of statements, avoiding changing-state and mutable data. The GeoDMS is also not meant to manage data in external databases. Therefore there are no GeoDMS functions for SQL Insert, Update, Delete and Select Into Statements.

## SQL Select

The SQL Select is used to make selections of data from one or more tables/views in a desired sequence. The SQL statement implicitly creates a new view, which can be used as a table, with a selection of the records of the original table(s)/view(s).

As the GeoDMS works with semantic arrays, new [[domain units|domain unit]] need to be configured before the [[data items|data item]] can be configured. The GeoDMS has a set of [[relational functions]]. These functions often result in mappings towards the original domain units, which can be used to define the new data items.

## examples

-   [[Select ... From ...]]
-   [[Select ... From ... Where ...]]
-   [[Select ... From ... Order By...]]
-   [[Select ... From ... Where ... Order By...]]
-   [[Distinct]]
-   [[Select ... From ... Group By ...]]
-   [[Select ... From ... Inner Join .... On ...]]
-   [[Select ... From ... Left_Right Join ... On ...]]
-   [[Select ... From ..., ... |Select ... From ... ... ]]