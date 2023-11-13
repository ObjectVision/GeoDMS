Available [[visualisation style]] [[subitems|subitem]] for [[arc]] data:

<pre>
parameter&lt;uint32&gt; PenColor      := rgb(255,0,0), DialogType = "<B>PenColor</B>";
parameter&lt;int16&gt;  PenWidth      := 2s          , DialogType = "<B>PenWidth</B>";
parameter&lt;meter&gt;  PenWorldWidth := 5[meter]    , DialogType = "<B>PenWorldWidth</B>";
parameter&lt;int16&gt;  PenStyle      := 0s          , DialogType = "<B>PenStyle</B>";
</pre>

## description

-   **PenColor**: a data item with [[value type]] uint32 and as [[expression]] a (set) of rgb values.
-   **PenWidth**: a data item with value type int16 and as expression a (set) of values indicating the width of the arc.
-   **PenWorldWidth**: a data item with the same values unit as the values unit of the unit for the 
[[coordinate system|How to configure a coordinate system]]. The PenWorldWidth specifies a size in e.g. meters, which is dependent on the zoom level in the map view. With this property a zoom dependent width can be configured.
-   **PenStyle**: a data item with value type int16 and as expression values between 0 and 5, indicating the line style. The following examples show the     different penstyles:

[[images/Arc_visualisation.png]]

Style 5 does not show the arc at all (which can be useful in a classification scheme where some arcs need to be hidden).