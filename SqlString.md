The SqlString property is used to specify which data and in which order will be selected from the database. The SqlString property requests a valid [SQL](https://nl.wikipedia.org/wiki/SQL) statement. It is used to:

-   Select the relevant [[attributes|attribute]]. If the names in the database do not correspond with the [[tree item|tree item]] names use the AS keyword to relate these names.
-   Select the table(s), view(s)/query(ies) with a FROM clause
-   If needed, make a selection with a WHERE condition
-   Define the sort order with the ORDER BY. This is important, as if no ORDER BY clause is configured, the sort order of the records read is     undefined. As the GeoDMS calculates with arrays, the sequence is relevant. Therefor it is is important to always specify the ORDER BY clause in the SqlString property.

The SqlString property is used to select data. Use only the following SQL key words: SELECT, AS, FROM, WHERE and ORDER BY. If a SQL Statement with a JOIN or a GROUP BY is needed, define this as a view or query in the database and use it as source for the GeoDMS.

We advice to make complex selections, for instance with substrings, within the GeoDMS and not in the WHERE clause.

The SqlString property can not be used to modify data, UPDATE, SELECT INTO and DELETE statements are not allowed.