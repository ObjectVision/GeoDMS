To structure data, different [data models](https://en.wikipedia.org/wiki/Data_model) are used. As the GeoDMS works with geographic data, [geographic
data models](https://en.wikipedia.org/wiki/Data_model#Geographic_data_model) are relevant to structure this type of data, see topic [[Geography]].

Another relevant and well-known data model, used in many databases is the [relational model](https://en.wikipedia.org/wiki/Relational_model). Much data is structured according to this model and many data modelers and database designers are familiar with this model. The focus of this chapter will be on assisting these modelers how the relational model relates to the GeoDMS datamodel and how relational operations can be configured in the GeoDMS.

All examples in this chapters are also visible in a complete configuration example, which can be downloaded
[here](http://www.objectvision.nl/outgoing/GeoDMS_Academy/geodms_academy_relationalmodel_20190813.zip).

## download

-   [configuration/data](https://www.geodms.nl/downloads/GeoDMS_Academy/geodms_academy_relationalmodel_20230817.zip)

## example

In the examples of this chapter we will use the following relational model, derived from the Dutch [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG) model.

[[ images/Relation_example_small.png]]

Each apartment has a unique identifier (Id, [primary key](https://en.wikipedia.org/wiki/Relational_database#Primary_key)) and an administrative address (Street, Number, Zipcode and Town). A boolean [[attribute]] (IsResidential) indicates if the apartment has a residential function. The last attribute is the Surface (in square meters) of the apartment.

Each building has also a unique identifier (Id, primary key), a construction year and a footprint of the building in square meters. The footprint can also be calculated with the area function in the GeoDMS.

Apartments can be related to multiple buildings and buildings can consist of multiple apartments, the relation is therefore a n-n relation, implemented in a third relational table called ApartmentBuildingRelation. In this table ApartmentId and BuildingId are [foreign keys](https://en.wikipedia.org/wiki/Relational_database#Foreign_key).

## relation model versus semantic arrays

A relation in the relational model is defined as a subset of the Cartesian product of n domains. These relations are represented as tables/views. In a relation a primary key uniquely specifies a tuple. Foreign keys are used to represent 1-n or n-n relations between relations. Structured Query Languages (like [SQL](https://en.wikipedia.org/wiki/SQL)) are used to:

-   define/modify or delete database objects ([DDL](https://nl.wikipedia.org/wiki/Data_definition_language)), see [[DDL]] for how to configure DDL statementes in the GeoDMS.
-   query/modify/delete data from tables/views ([DML](https://en.wikipedia.org/wiki/Data_manipulation_language)), see [[DML]] for how to configure DML statementes in the GeoDMS.

The GeoDMS uses a data model with semantic arrays. Semantics means each array is associated with meta information, used to derive calculation characteristics and check calculation logic. Semantic arrays are mappings between sets, in GeoDMS terms typed by [[domain unit]] and [[values unit]].

Relevant differences between the attributes of relations in the relational model and semantic arrays:

-   domain units need to be configured explicitly. As a semantic array is defined by its domain and values units, these units need to be configured explicitly before the arrays can be defined. In the relation model a SQL statement results in a new view/relation with a subset of the source table(s)/views. In the GeoDMS such an operation is performed in two steps:

1.  use a relation function to define the new domain unit.
2.  define the relevant attributes for the new domain unit.

See for example the Select From Where example.

-   [[index numbers]] (mappings) are used to relate domains. [[Aggregation|Aggregation functions]] and [[Relational|Relational functions]] functions always work with these index numbers. Foreign keys in relations are therefore first translated to the index numbers of the relating domain (with the
[[rlookup]] function). Such an attribute with domain unit A and as the index numbers of domain B is called a [[partitioning]].

-   As the GeoDMS uses arrays, the sequence of elements matter. In the relation model there is no meaning in the ordering of the rows of a relation. A domain unit in the GeoDMS can be defined, in terms of the relational model, as an ordered relation. The sequence of the elements is relevant and if the     sequence of elements would be different, they also need to have a different domain unit. This ordering can for instance be used in cumulative functions.

## GeoDMS configuration examples

-   See [[DDL]] for how to configure SQL data definition statements (Create Table, Alter Table, ..)
-   See [[DML]] for how to configure SQL data manipluation statements (Select Statements)