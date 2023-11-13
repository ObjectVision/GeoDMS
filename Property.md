This page contains a complete list of all property definitions.

-   Bold properties can be configured to (some) [[tree items|tree item]]
-   Italic properties are derived properties, not meant to be configured

Be aware, not all properties apply to all item types.

## all properties

-   **[[cdf]]**: a classification scheme for a [[values unit|values unit]] or [[data item]]. If configured to a values unit, all data items with this values unit will use the classification (by default) in map and graph views. If the cdf property is configured to a data item, it is used in a graph or map view of this data item and overrules a cdf property configured to the values unit of the data item. The cdf property always needs to refer
    to the ClassBreaks item of a classification scheme.
-   *ConfigFileColNr*: the vertical column position of the configured item in the configuration file.
-   *ConfigFileLineNr*: the horizontal line position of the configured item in the configuration.
-   *ConfigFileName*: the name of the configuration file, including the relative path from the configuration directory of the project.
-   **Descr**: a description for a [[tree item]] that is shown in the detail pages and as tool tip. It is advised to keep a description limited to one sentence. More descriptive information can better be stored in a (HTML) file and configured as metadata with the url property.
-   **[[DialogData]]**: Used in combination with the DialogType property to configure information on how to present data items. If the DialogType property is configured to Map, the DialogData should refer to the data item to which the geographic data is configured. This is needed to inform the GeoDMS how to visualise the geographic [[domain unit] of the requested data item. The DialogData property can also be
    configured for a values unit describing the coordinate system used for map views. In that case the DialogData can be configured with one or multiple [[attributes|attribute]] refering to geographic information. These attributes will then be grouped into a background layer that is by default presented in each map view.
-   **[[DialogType]]**: Together with the DialogData property, this property defines how data is visualised. The following options are available:
    -   Classification: Defines a ClassBreaks item of a classification scheme. Configure this option for ClassBreaks items, to inform the GeoDMS the attribute is used for class boundaries of classifications and can be viewed/modified with the Classification and Palette editor in the [[GeoDMS GUI]]. See ClassBreaks item in a classification scheme for more information on ClassBreaks items.
    -   BrushColor: Defines a style item of a classification scheme. Configure this option for palette items, to inform the GeoDMS the attribute is used for the definition of colors in classifications and can be viewed/modified with the Classification and Palette editor in the GeoDMS GUI . See Visualisation style items in a classification scheme for more information on palette items.
    -   Labels: Defines a labels item of a classification scheme. Configure this option for labels items, to inform the GeoDMS the attribute is used for the labels in classifications and can be viewed/modified with the Classification and Palette editor in the GeoDMS GUI . See Labels item in a classification scheme for more information on labels items.
    -   Map: This option is configured for domain units, to inform the GeoDMS that all data items with this domain unit can be visualised in a map view. The DialogData property should refer to the [[feature attribute]].
    -   SymbolSize: Defines the size in pixels for [[point]] data in the GeoDMS GUI map view. See Visualisation styles for point data
    -   SymbolWorldSize: Defines the worldsize in the coordinate system unit for point data in the GeoDMS GUI  map view. See Visualisation styles for point data
    -   SymbolColor: Defines the color in rgb values for point data in the GeoDMS GUI  map view. See Visualisation styles for point data
    -   SymbolIndex: Defines the index value in the font for point data in the GeoDMS GUI  map view. See Visualisation styles for point data
    -   SymbolFont: Defines the font for point data in the GeoDMS GUI  map view. See Visualisation styles for point data
    -   PenWidth: Defines the width in pixels for [[arc]] data in the GeoDMS GUI map view. See Visualisation styles for arc data
    -   PenWorldWidth: Defines the worldwidth in the coordinate system unit for arc data in the GeoDMS GUI  map view. See Visualisation styles for arc data
    -   PenColor: Defines the color in rgb values for arc data in the GeoDMS GUI  map view. See Visualisation styles for arc data
    -   PenStyle: Defines the style for arc data in the GeoDMS GUI  map view. See Visualisation styles for arc data
    -   BrushColor: Defines the color in rgb values for the interior of [[polygon]] data in the GeoDMS GUI  map view. See Visualisation styles for polygon data
    -   HatchStyle: Defines the style for the interior of polygon data in the GeoDMS GUI  map view. See Visualisation styles for polygon data
    -   LabelText: Defines the labels as strings for point and polygon data in the GeoDMS GUI  map view. See Visualisation styles for labels
    -   LabelSize: Defines the size in pixels for labels in the GeoDMS GUI  map view. See Visualisation styles for labels
    -   LabelWorldSize: Defines the worldsize in the coordinate system unit for labels in the GeoDMS GUI  map view. See Visualisation styles for labels
    -   LabelColor: Defines the color in rgb values for labels in the GeoDMS GUI  map view. See Visualisation styles for labels
    -   LabelFont: Defines the font for labels in the GeoDMS GUI  map view. See Visualisation styles for labels
    -   MinPixSize: Defines the lower limit of the scale range to visualise a layer. See Scale dependent visualisation
    -   MaxPixSize: Defines the upper limit of the scale range to visualise a layer. See Scale dependent visualisation
-   **[[DisableStorage]]**: Property used to configure whether or not a data item needs to be stored in an external storage. If a storage is configured to an item (or any parent item), the property definition: DisableStorage ="True" indicates the current item will not be stored in an external storage.
-   **[[DomainUnit]]**: The domain unit of a data item indicates the entity for which the data item is available. This domain unit is configured between () brackets for all attributes (not for [[parameters|parameter]]). A domain unit is usually configured in a Units container.
-   **[[ExplicitSuppliers]]**: This item creates a dependency relation towards another item, that needs to be updated first, before the current item is updated.
-   **Expr**: The [[expression]] property is used to configure a calculation rule for a tree item.
-   **FreeData**: This Boolean property determines if data results that are maintained in the CalcCache will be stored persistent or will be released after their interest count goes down to zero. The result data for data items with a calculation rule are by default
    stored persistent if they are stored in the CalcCache. data items that are read by a [[StorageManager]] are by default not persistent. Intermediate results are never persistent. Non persistent CalcCache data is stored in the
    subfolder tmp of the CalcCache folder. In combination with the KeepData = "True" property, the FreeData="True" condition determines that calculated data will be freed at the end of a project session but not earlier. See also: CalcCache Guide.
-   *FullName*: This property is derived from the name of the tree item and it's hierarchical structure (all parent container item names). The property is not meant to be configured.
-   *HasCalculator*: Read only property indicating if the data item has an expression configured or the item is the result of an expression configured to the [[parent item]].
-   *InHidden*: The property inHidden is derived from the configured isHidden = "True" property for the item itself or to any parent of the item. The inHidden property indicates if a item is visible (false) or not visible (true) if the option Show hidden items in the GUI Options dialog is set to false.
-   *InTemplate*: This property is derived from a parent property for which the IsTemplate property is configured. With the InTemplate property is indicated if the item is part of a calculation scheme, that can not be calculated.
-   *IsEndogenous*: The read only property IsEndogenous = "True" indicates a data item is calculated and not read from a source file.
-   *IsHidden*: The property isHidden = "True" hides tree items in the TreeView if the option Show hidden items in the GUI Options dialog is set to false. The option is meant to hide items that are not relevant to and end user.
-   *IsLoadable*: Read only property indicating if exogenous data items can be read from external source files, a StorageManager is configured for the data item and the property DisableStorage is not configured to be true.
-   *IsStorable*: Read only property indicating if endogenous data items can be stored, a storage manager is configured for the data item and the property DisableStorage is not configured to be true.
-   *IsTemplate*: This property need to be configured to the [[container]] item representing a calculation scheme. With this property is indicated that the item to which this poperty is configured and all it's subitems represent a scheme that can not be updated. The property also needs to be configured to inform the GeoDMS GUI  this item represents a calculation scheme (necessary for the case generator).
-   **KeepData**: This property can be set to true for calculations that are time consuming. If set to true the results of a calculation are kept in memory after being calculated once.
-   **Label**: This property is used to present the item in the GeoDMS GUI  (except for the treeview in which the tree item name is used). In the legend of the Map view, the header of the table view etc the label is used (if configured, else the name is used). In the label all characters are
    allowed, but it is advised to keep the size of the label limited. 
-   *Name*: This property is derived from the name of the tree item as it is configured in the GeoDMS configuration files
-   *NrSubitems*: A derived property indicating the number of first level subitems or a tree item.
-   **ParamData**: This property is used for case parameters. The content depends on the configuration of the accompanying ParamType property:
    -   If ParamType is configured to Numeric, ParamData indicates the values unit of the numeric value.
    -   If ParamType is configured to ItemList, ParamData describes the container which subitems are presented in the combo box.
    -   If ParamType is configured to ExprList, ParamData contains the expression resulting in a set of items, presented in the combo box.
    -   If ParamType is configured to SimilarList, ParamData describes the container that must match with ohter 'similar' containers based on their subitems. Furthermore the property is used for case [[containers|container]] to indicate in which subcontainer of the cases container the resulting cases will be available.
-   **ParamType**: This property is used for case parameters. The following options are available:
    -   NumericList : A textbox is shown to specify a numeric value, the values unit need to be configured in the ParamData property.
    -   ItemList : a combobox is shown to select from the list of subitems from the container configured in the ParamData property.
    -   ExprList : a combobox is shown to select from the options resulting from the expression, comma separated (expressions for this property are usually configured with the asexprList operator).
    -   SimilarList : a comboxbox is shown to select from the containers that match the configured container for the ParamData property. The match must be on the number of items, values units and domain units of the items. If the "$AllValues" condition is configured in the
-   **ParamData** property (; separated), the condition about matching values unit does not apply. This also applies to the condition "$AllDomains" with regard to the configured domain units. Default, cases are not expanded so Similarlist does not result in matches within not yet expanded case. By adding the condition "$ExpandCases" all configured cases are first expanded and also searched through.
-   **Password**: Password that needs to be specified for a connection string towards an odbc source
-   **[[SpatialReference]]**: This property is used to configure the EPSG code for the geographical coordinate system to the [[coordinate unit|How to configure a coordinate system]]
-   *SortByName*: Property used to indicate if the GeoDMS GUI  should present all subitems in a alphabetic sequence instead of the sequence of the items in the configuration file. The default value = false (no alphabetic order).
-   **[[Source]]**: This property is used to configure descriptive information for the source data used in a GeoDMS configuration. Source descriptions can be relevant for reporting purposes. The configured source property is presented in the Source Description tab for each data item in which this source item is used (based on the derived dependencies).
-   **[[SqlString]]**: This property is configured for Data sources, at the table/query level. A valid SQL Statement need to be configured. All data selections statements are valid, statements that modify tables (like SELECT INTO, UPDATE etc) are not valid.
-   StartupDesktop: obsolete
-   **[[StorageName]]**: primary data used in the GeoDMS is usually stored in external storages. With the StorageName property a reference towards a primary data source is configured (in most cases a filename).
-   **[[StorageReadOnly]]**: The StorageReadOnly property is used to indicate if a storage is only configured to read from (not to write to).
-   **[[StorageType]]**: This property indicates the StorageManager used to read and write the data, e.g. ASCII grid, BMP, DBF, ODBC. The StorageType property only has to be configured if it cannot be derived from the file extension.
-   **StoreData**:This property configured to the value True explicitly stores data items that will not be stored in the CalcCache on disk, according to the CalcCache rules.
-   **[[SyncMode]]**: This property is used to indicate which sources are read from a storage with multiple tables/attributes. The SyncMode property can be configured in three different modes:
    -   All: all tables and queries are read from the database, independent on what is configured in the GeoDMS configuration.
    -   Attr: only tables configured are read from the database, but from these table all attributes
    -   None: only tables and attribute configured are read from the database
-   *TableType*: Read only property derived from the ODBC storage manager indicating if the source is a table or query
    -   TCalculationShemaCalculator: property used by the GeoDMS to store lay out information on the saved lay out of a calculation scheme.
    -   TSubitemShemaCalculator: property used by the GeoDMS to store lay out information on the saved lay out of a subitem scheme.
    -   TSupplierShemaCalculator: property used by the GeoDMS to store lay out information on the saved lay out of a supplier scheme.
-   **Url**: the url property is used to configure relevant background information, stored in files on the local machine (preferably html, but other formats that can be viewed on the local machine are allowed as well). The file configured in the url property is shown
    in the Metadata detail page if the tree item is selected in the tree view, or if a subitem is selected, for which no url property is configured.
-   **UserName**: User name that needs to be specifief for a connection string towards an odbc source
-   **Using**: The Using statement is used to refer to other branches in the tree in order to find tree items in expressions, without having to specify a full name.
-   **[[ValueComposition|composition]]**: This property describes is used for data items with point [[value types|value type]]. It describes if each instance of the data item consits of one (single) point, for point data like
    dwellings, services, or a set of points (poly), for lines or polygons like roads or administrative areas.
-   **[[ValuesUnit]]**: The values unit describes in which unit a data item is expressed  (meter, second, Euro etc). Values units are configured between < and > brackets after the keywords attribute or parameter.
-   **ViewAction**: If a case is specified with the case generator in the GeoDMS GUI , a resulting data item can be viewed in mapview, datagrid dynacube etc. This item and the type need to be configured in the ViewAction property.


## examples

```
unit<uint32> TestCSV
: StorageName = "%ProjDir%/Data/test.csv"
, StorageType = "gdal.vect"
, StorageReadOnly = "true"
, SyncMode = "None"
{
	attribute<string>                  attr;
}
```

```
parameter<string> SnapshotDir := ='SnapshotDir'+string(BAG_jaar);

container Panden
{
	unit<uint32> import
	:	StorageName     = "= SnapshotDir+'/pand.fss'"
	,	StorageReadOnly = "True"
	{
		attribute<rdc_mm_i32>  geometry_mm (polygon) ;
		attribute<string>      identificatie;
		attribute<int16>       bouwjaar;
		attribute<WPSrc>       woonpand_type;
		attribute<string>      status;
	}
}
```

```
unit<uint32> Natura2000
:	StorageName     = "%RSLdataDir%/Beleid/EU/Natura2000_20200524_RD_reproject.gdb"
,	StorageType     = "gdal.vect"
,	StorageReadOnly = "True"
,	DialogType      = "Map"
,	Source          = "Nationaal georegister download shp-formaat op 20201013"
,	url             = "http://geodata.nationaalgeoregister.nl/natura2000/atom/natura2000.xml PS_Natura2000_as_is"
,	SyncMode        = "Attr"
,	SqlString       = "SELECT * FROM Natura2000_20200524_RD_reproject WHERE KADASTER <> 'niet van toepassing'" 	
{
	attribute<rdc_meter> Geometry (poly);
}

```

```
unit<dpoint> LatLong_Base: Format = "EPSG:4326", DialogData = "wms_layer_llh"
{
	parameter<float32> ViewPortMinSize := 100f / 3600f;
	parameter<float32> PenWorldWidth   := 10f / 3600f, DialogType = "PenWorldWidth";
	parameter<float32> LabelWorldSize  := 10f / 3600f, DialogType = "LabelWorldSize";
}
```

```
unit<float32>       s               := baseunit('s', float32)   , label = "second", cdf = "seconden/ClassBreaks";

unit<uint8> seconden : nrofrows = 3
{
	attribute<string>   Label        :  ['treated', 'control', 'other'];
	attribute<uint32>   PenColor     := Brushcolor, DialogType = "PenColor";
	attribute<uint32>   BrushColor   :  [rgb(200,0,0),rgb(0,200,0), rgb(128,128,128)], DialogType = "BrushColor";
	attribute<s>        ClassBreaks  :  [0,1080,3600], DialogType = "Classification";
}
```
