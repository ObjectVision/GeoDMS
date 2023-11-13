## available [[visualisation styles|visualisation style]] [[subitems|subitem]] for [[point]] data

<pre>
parameter&lt;uint32&gt;  SymbolColor      := rgb(255,0,0)  , DialogType = "<B>SymbolColor</B>";
parameter&lt;float32&gt; SymbolSize       := 8f            , DialogType = "<B>SymbolSize</B>";
parameter&lt;meter&gt;   SymbolWorldSize  := 10[meter]     , DialogType = "<B>SymbolWorldSize</B>";
parameter&lt;string&gt;  SymbolFont       := 'GeoDMS Font' , DialogType = "<B>SymbolFont</B>";
parameter&lt;int16&gt;   SymbolIndex      := 35s           , DialogType = "<B>SymbolIndex</B>";
</pre>

Description:

-   **SymbolColor**: a [[data item]] with [[value type]] uint32 and as [[expression]] a (set) of rgb values.
-   **SymbolSize**: a data item with value type float32 and as expression a (set) of values indicating the symbol character size in pixels.
-   **SymbolWorldSize**: a data item with the same values unit as the values unit of the unit for the 
[[coordinate system|How to configure a coordinate system]]. The SymbolWorldSize specifies a size in e.g. meters, which is dependent on the zoom level in the map view. With this [[property]] a zoom dependent size can be configured.
-   **[[SymbolFont]]**: a data item with [value type] string and as expression the font to be used. The GeoDMS Font, named in the example, is a font that is installed with a GeoDMS application. It contains different symbols to be used for point data in the map view. See your Windows reference for more information on how to use fonts. The GeoDMS font is the default value for this visualisation style.
-   **SymbolIndex**: a data item with value type int16 and an expression that indicates the character symbol in the font.