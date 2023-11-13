For [[point]] and [[polygon]] data, labels can be visualised in the map view. Available [[visualisation style]] [[subitems|subitem]] for labels:

<pre>
parameter&lt;string&gt;  LabelText      := 'Netherlands'   , DialogType = "<B>LabelText</B>";
parameter&lt;uint32&gt;  LabelColor     := rgb(255,0,0)    , DialogType = "<B>LabelColor</B>";
parameter&lt;uint32&gt;  LabelBackColor := rgb(255,255,0)  , DialogType = "<B>LabelBackColor</B>";
parameter&lt;float32&gt; LabelSize      := 8f              , DialogType = "<B>LabelSize</B>"
parameter&lt;meter&gt;   LabelWorldSize := 10[meter]       , DialogType = "<B>LabelWorldSize</B>";
parameter&lt;string&gt;  LabelFont      := 'Arial'         , DialogType = "<B>LabelFont</B>";
</pre>

## description

-   **LabelText**: a [[data item]] with [[value type]] string and as [[expression]] the labels to be visualised.
-   **LabelColor**: a data item with value type uint32 and as expression a (set) of rgb values, used to configure the color of the text.
-   **LabelBackColor**: a data item with value type uint32 and as expression a (set) of rgb values, used to configure to background frame color around the text.
-   **LabelSize**: a data item with value type float32 and as expression a (set) of values indicating the fontsize in pixels
-   **LabelWorldSize**: a data item with the same values unit as the values unit of the base unit for the 
[[coordinate system|How to configure a coordinate system]]. The LabelWorldSize specifies a size in e.g. meters, which is dependent on the zoom level in the map view. With this [[property]] a zoom dependent size can be configured.
-   **LabelFont**: a data item with value type string and as expression the font to be used.