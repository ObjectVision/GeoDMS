The SyncMode [[property]] is used to indicate which tables, views/queries and which [[attributes|attribute]] of these tables, views/queries or files need to be read from a database or file. The property has three possible values:

-   **AllTables** (for databases only): All tables and views/queries from the database will be read. Default values and [[domain units|domain unit]] will be used for non configured tables/views and attributes. The option is mainly useful for a quick overview of a whole database. If attributes will be used in calculations, it is advised to configure these attributes explicitly to use more meaningful [[values units|values unit]] and domain units.
-   **Attr** (default value): Only the tables and views/queries that are configured are read from the database. From these tables all attributes are read, also not configured attributes.
-   **None**: Only the configured attributes are read from the database.

The default value for this property is: **Attr**