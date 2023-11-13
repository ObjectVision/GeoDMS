Available [[visualisation style]] [[subitems|subitem]] for [[polygon]] data: For the outline of the polygon the same styles can be configured as for
[[Arc visualisation|Arc visualisation]]).

Available visualisation style subitems for the interior of the polygon:

<pre>
parameter&lt;uint32&gt; BrushColor := rgb(255,0,0), DialogType = "<B>BrushColor</B>";
parameter&lt;int16&gt;  HatchStyle := 0s          , DialogType = "<B>HatchStyle</B>";
</pre>

## description

-   **BrushColor**: a [[data item]] with [[value type]] uint32 and as [[expression]] a (set) of rgb values. Configure BrushColor := 4294967295 for a transparant brushcolor (the value represents the nulll value for this value type. The BrushColor [[attribute]] is also in use for [[grid data]].
-   **HatchStyle**: a data item with value type int16 and as expression values between 0 and 6, indicating the hatching style. The following examples show the different hatching styles:

[[images/Poly_visualisation.png]]