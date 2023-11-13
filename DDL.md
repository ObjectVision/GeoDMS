*[[Relational model versus Semantic arrays]] DDL*

In the relation model a [DDL (Data Definition Language)](https://en.wikipedia.org/wiki/Data_definition_language) is used to define, modify and delete database objects. [SQL](https://en.wikipedia.org/wiki/SQL) is one of the most used DDL(Data Definition Language) (as well as [DML (Data Manipulation
Language)](https://en.wikipedia.org/wiki/Data_manipulation_language) language.

The GeoDMS modelling language is a functional programming language. In projects the data structure is defined in a configuration and build up in memory each time a configuration is loaded. Programming is done with [[expressions|expression]] instead of statements, avoiding changing-state and mutable data. The GeoDMS is not meant to manage objects in external databases. Therefore there are no GeoDMS functions for SQL Alter, Rename and Drop Statements.

## SQL Create Table

In SQL Create Table statements are used to define new tables. The Create Table statement for the Building entity in our example (see next figure):

[[images/Relation_example_building.png]]

looks like this:

<pre>
Create Table Building(
   Id               Text Primary Key,
   ConstructionTear Integer,
   Footprint        Double
);
</pre>

This statement only creates the table with the three fields. It does not add data to this table. Use [SQL](https://en.wikipedia.org/wiki/SQL) Insert Into statements to add data to these tables.

## GeoDMS Domain Units

The SQL Create Table statement is, in GeoDMS terms, similar to defining a new [[domain unit]].
Defining such a unit in the GeoDMS is usually combined with reading/configuring the data. The data can be read from
multiple storage formats (see [[StorageManagers|StorageManager]], but for small amounts of data also from the configuration. The following example presents the configuration of the Building entity in the GeoDMS with it's [[attributes|attribute]] and data:

<pre>
unit&lt;uint32&gt; Building: NrOfRows = 4
{
    attribute&lt;string&gt;     Id:               ['bui001','bui002','bui003','bui004'];
    attribute&lt;units/year&gt; ConstructionYear: [2010,2012,2015,2016];
    attribute&lt;units/m2&gt;   Footprint:        [100,300,250,150];
}
</pre>

The Building entity is configured as a unit32 domain unit. As the data is read from the configuration, the number of rows need to be configured.

Each field is configured as an attribute. These attributes are mappings from the Building domain towards other domains (like the set of ConstructionYears).

The numeric attributes are not only configured with a data type (Integer, Double etc), but with a more meaningful [[values unit]] like year or m2.

In the GeoDMS [primary keys](https://en.wikipedia.org/wiki/Relational_database#Primary_key) are not explicitly configured.

### **Formatted tables**

The presented configuration structure with primary data configured between square brackets is not so user-friendly for editing tables, as the data is not presented in a tabular structure to the editor. A more editor friendly configuration of the same data is presented in the following example:

<pre>
unit&lt;uint32> Building: NrOfRows = 4
{
    attribute&lt;string&gt;      id               := data/element/values[value(id(.) * data/nrAttr, data/element)];
    attribute&lt;units/year&gt;  ConstructionYear := 
       value(data/element/values[value(id(.) * data/nrAttr + 1, data/element)], units/year);
    attribute&lt;units/m2&gt;    Footprint        := 
       value(data/element/values[value(id(.) * data/nrAttr + 2, data/element)], units/m2);
    container data
    {
        parameter&lt;uint32&gt; nrAttr := 3;
        unit&lt;uint32&gt; element := Range(uint32, 0, nrAttr * #Building)
        {
           attribute&lt;string&gt; values: [<I>
              //    id  , ConstructionYear, Footprint </I>                
               'bui001' , '2010' ,'100'
              ,'bui002' , '2012' ,'300'
              ,'bui003' , '2015' ,'250'
              ,'bui004' , '2016' ,'150'
            ];
        }
    }
}
</pre>

With this way of configuring, data can easily be edited as a table.