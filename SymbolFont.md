## own font

Windows [fonts](https://en.wikipedia.org/wiki/Computer_font) can be used to visualize [[point]] layers with symbols in a MapView. 

If no font is configured, the GeoDMS own font is used. This font is part of the GeoDMS installation.


_The font contains the following characters:_

[[images/dms_ttf.png]]  

Use the SymbolIndex [[parameter]] to configure a character from this set, see the example:

### example character in font

<pre>
parameter&lt;int16&gt; SymbolIndex := 35s, DialogType = "<B>SymbolIndex</B>"; // results in character 35, a house symbol.
</pre>

## other fonts

Other fonts can be used as well. They need to be configured with the SymbolFont parameter, see exanple:

<pre>
parameter&lt;string&gt; SymbolFont := 'Tahoma'   , DialogType = "<B>SymbolFont</B>";
parameter&lt;string&gt; SymbolFont := 'Wingdings', DialogType = "<B>SymbolFont</B>";
</pre>

Use e.g. MsWord > Insert > Symbols to find the SymbolIndex for a specific character in the installed font.

## see also
* [[Point visualisation]]