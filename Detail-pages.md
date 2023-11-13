_[[User Guide GeoDMS GUI]]_ - detail pages

## introduction
Detail Pages present a detailed overview on the active [[Tree Item]]. 

## generic properties
[[images/GUI/detail_page_generic.png]]

This page presents the most relevant [[properties|property]] of a Tree Item. The page shows:
- The full name and if configured: label, description.

And, if the tree item is a [[data item]]: 
- A calculation rule (expression) for a data item for which an expression is configured, and if applicable an instantiated from reference. 
- if applicable a storage for a data item read from a data source, with a ReadOnly indicator and optional a SqlString.
- The values unit for the data item, including if applicable the element type, the metric, the range of the values, the [[number of elements|cardinality]] and, if configured, the classification scheme for the values unit. 
- The [[value type]], including range information like the range and/or the NrElements 
- The domain unit for which the item is available, including the element type, if relevant the geographic projection and the range (nr elements) of the domain.

## explore accessible namespaces 
[[images/GUI/detail_page_explore.png]]<br>
This detail page shows the name, description and storage type of the items in the NameSpace of the active Tree Item.

## all/non-default properties 
[[images/GUI/detail_page_properties.png]]

This detail page has a toggle state with two options:
- **All**: all relevant properties for the active tree item are presented.
- **Only non-default values**: all properties with a configured value (not the default value) for the active Tree Item are presented.

## configuration
[[images/GUI/detail_page_configuration.png]]

The detail page shows the configuration syntax of the active tree item (and if available, it’s subitems).

## source description 
[[images/GUI/detail_page_source.png]]

This detail page describes, if relevant, information about all sources used to calculate a data item. If a source property is configured for a data item, this information is presented, of not the data sources names (files) is presented

This detail page has a toggle state with four options:
- **Configured**: For storages for which a Source property is configured, the contents of these properties are shown. If not, the file/db names are presented.
- **ReadOnly** : All file/db names are presented used to read data from (configured with StorageReadOnly = True).
- **Non ReadOnly** : All file/db names are presented used to write data to. 
- **All** : All file/db names are presented.

## meta information reference 
[[images/GUI/detail_page_meta.png]]

For more information on a tree item, an URL [[property]] can be configured that refers to a document on the local machine or on the web. A reference to this document is shown in this detail page for the item for which the URL property is configured as well as for each [[subitem]].

 