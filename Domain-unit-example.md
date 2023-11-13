_[[Configuration examples]] Domain unit/Table_

The one-dimensional (table) [[domain unit]] corresponds to what in a database usually is called a table. The domain unit unit defines the table and the **number** [[cardinality]] and **order** of entries. [[Attributes|attribute]] of this table have the configured unit as domain unit.

## configuration

Domain units can be configured in a GeoDMS configuration in three ways:

### explicitly configuring the cardinality and order in the configuration

*example*
<pre>
unit&lt;uint8&gt; direction: nrofrows = 4
{
   attribute&lt;string&gt; name  : ['North','East','South','West'];`
   attribute&lt;string&gt; label := Name;`
}
</pre>

In this example the domain unit direction is configured with 4 entries (rows).

- The attribute *name* is used to name each entry in the table (it is advised to use name for an attribute, of which the values can be used as [[tree item]] names, see also [[naming conventions]].
- An attribute with as name: *label* is configured. This is advised, as these labels are presented every time the id of the domain or a [[relation]] towards this domain is requested in a table. The label is presented after each [[index numbers]] between normal brackets. The labels are not copied to the clipboard or exported.

For domain units with a small number of elements, this way of configuring is fine. If the number of entries/rows and/or attributes increases, a configuration for a more user friendly way of editing the data is presented in the: [[tabular data|configuration file]] paragraph of the page.
as [[data source]].

### deriving the [[cardinality]] and order with an [[expression]]

*Example*

<pre>
unit&lt;uint32&gt; region := unique(GridDomain/regioncode);
</pre>

In this example the domain unit region is derived from the unique occurrences of the regioncode attribute of a GridDomain domain unit.

We advice to use the [[unique]] function mainly to explore your dataset. If the set of entries of a domain is known, configure this set explicitly (example 1). This make the configuration less dependent on the occurrences of (all) values in your data and the indexing persistent.

Except from the unique function, domain units are also often defined with the [[select_with_org_rel]], [[combine]] or [[union_unit]] functions.

### deriving the cardinality and order from a [[storage|data source]]

*Example*

<pre>
unit&lt;uint32&gt; region
: StorageName = "%SourceDataProjDir%/region.shp"
, StorageType = "gdal.vect";
</pre>

In this example the cardinality and order of the values in the domain unit are derived from the region [[Shapefile|ESRI Shapefile]].

If source data is read from a storage, this is the advised way of configuring the domain.