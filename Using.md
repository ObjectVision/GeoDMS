As described on the [[NameSpace]] page, the GeoDMS uses a search path to find referred [[tree items|tree item]]. [[Parent item]] names can be added to find in other branches.

Some branches are used multiple times in references, for example units in a Units container are often referred to from other branches. In stead
of having to configure the parent item Units/ for each item in the reference, the Using property can be added to make the GeoDMS aware items can also be found in another branche.

*The Using [[property]] is used to configure in which container, except from the GeoDMS source path, referred items can be found.*

Example:
<pre>
container Units
{
   unit&lt;float32&gt; meter;
   unit&lt;spoint&gt;  EuroPoints;
}
container SourceData: <b>Using</b> = "Units"
{
   attribute&lt;meter&gt; altitude (EuroPoints): StorageName = "%projDir%/data/grid/Elevation.asc";
}
</pre>

The [[units|unit]] meter and EuroPoints can not be found in the search path of the altitude item. Two options to solve this:

1.  Add the parent name Units to both items meter and EuroPoints
2.  Configure the Using = "Units" property to the SourceData container, as in the example. This makes the GeoDMS aware items can also be found in the Units container.

Items in the Using property can be combined with the [semicolon (;)](https://en.wiktionary.org/wiki/semicolon) delimiter, e.g. Using =
"Units;SourceData" refers to both the Units and the Sourcedata container.

Although Using properties are powerful to reduce code, they should be used carefully. Multiple Using properties make search paths complex and confusing. In most configurations they are only in use for Units, Classifications and sometimes Geography containers.