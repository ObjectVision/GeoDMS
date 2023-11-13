The [[GeoDMS GUI]] contains an interactive map view component for visualising both [[grid data]] and [[vector data]] ([[point]], [[arc]] and [[polygon]] features). To visualise [[data items|data item]] in a map view, they must have a [[Geographic|Geography]] [[domain unit]].

Various configurable aspects can be used to configure how data tems are visualised:

-   **size/width**: size of symbols or width of arcs / polygon outlines
-   **color**: color of [[grid]] cells, point symbols, arc, polygon outlines and the fill color of polygons.
-   **hedging**: for the interior of polygons
-   **[[font type|SymbolFont]]**: the font used for the point symbols and labels
-   **font character**: the font character used for the point symbols and labels.

Each aspect has a default style, which can be overruled by configuring these styles in the configuration. Colors can also be edited with the GeoDMS GUI.

## configuration of visualisation styles

Visualisation styles are configured for data items with the [[DialogType]] property. This [[property]] indicates which aspect is configured.

For [[parameters|parameter]] it is advised to configure visualisation styles values as [[expression]] (than they can be edited with the expression dialog in the GeoDMS GUI).

Visualisation styles items are configured in two ways:

### As [[subitem]] of the [[values unit]] of the item to be visualised

By configuring visualisation styles items as subitem of the values_unit of the items to be visualised, these visualisation styles apply as default style for all data items with these values units. This is a generic way of configuring visualisation styles.

*Example:*
<pre>
unit&lt;string&gt; vrz_lbl: DialogType = "LabelText"`
{
  parameter&lt;float32&gt; LabelSize := 8[fontsize], DialogType = "LabelSize";
}
</pre>

In this example a values unit for text labels is configured with as subitem a parameter for the fontsize. The DialogType for the values unit is set to "LabelText" to indicate the values unit is used for labels in the map.

The subitem parameter has a DialogType = "LabelSize" configured to inform the GeoDMS that data items with this values unit need to be visualised with a font size of 8 pixels.

The name (LabelSize) of the subitem parameter has no functional meaning, but for transparency reasons it is advised to use a clear name like the name of
the DialogType, see also [[naming conventions]].

The visualisation style item is often a part of a [[Classification]]. This item can be an attribute with the number of entries of the class unit, or a parameter with one value for all classes.

### As subitem of the data item or it's domain unit to be visualised

For each data_item or it's domain unit visualisation styles can be configured, overruling an optional visualisation style item configured to it's values unit.

If a visualisation style is configured as subitem of a Geographic domain unit, the style will be applied for the all data items of the same domain unit. Example:

<pre>
attribute&lt;point_rd&gt; locatie := point(PresYCoord, PresXCoord, point_rd)
{
  parameter&lt;uint32&gt;  SymbolColor := rgb(255,0,0), DialogType = "SymbolColor"
  parameter&lt;float32&gt; SymbolSize  := float32(8),   DialogType = "SymbolSize";
}
</pre>

In this example, two subitems are configured to set the color and size for symbols. In the next paragraphs different style items are described for point, arc, polygon layers and for labels in the map.

For grid data only the visualisation style for color, called BrushColor (see also polygon) item is relevant.

## styles for

-   [[point visualisation]]
-   [[arc visualisation]]
-   [[polygon visualisation]]
-   [[label visualisation]]
-   [[scale dependent visualisation]]
-   [[background layer]]