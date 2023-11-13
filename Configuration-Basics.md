## getting acquainted

The GeoDMS configures how to calculate and visualise projects results in (a set of) configuration file(s).

An example is shown here:

<pre>
container ExampleProject
{
   container Units
   {
       unit&lt;float32&gt; m  := BaseUnit('m', float32),  cdf  = "Classifications/M3K/Classes";
       unit&lt;float32&gt; m2 := m * m;
   }
   #include &lt;Classifications.dms&gt;
   container SourceData: Using = "Units"
   {
       <I>// BAG is the registration of buildings and addresses in the Netherlands</I>
       container BAG
       {
           unit&lt;uint32> vbo: StorageName = "%SourceDataDir/BagSnapshot/20190101/vbo.fss"
           {
               attribute&lt;string&gt;   identificatie;
               attribute&lt;units/m2&gt; oppervlakte;
           }
       }
   }
}
</pre>

## hierarchical structure

Each GeoDMS configuration has a hierarchical structure, used to [[name|tree item name]] items (called [[tree items|tree item]] in a context, also known as [[NameSpace]]).

Curly bracket characters { and } introduce new levels, all elements within these brackets are [[subitems|subitem]] of a higher level item ([[parent item]]).

The use of tabs to indicate the levels is not obligatory but strongly advised. The convention is to position the brackets at the same level as the parent item, subitems are positioned one tab position to the right.

## keywords

In configurations we can find keywords:

- [[container]]
- [[unit]]
- [[attribute]]
- [[parameter]]
- [[template]]
- [[include]]
- [[using]]

Each item is configured with one of the first four keywords, a name and optionally one or more [[properties|property]]. The definition of an item is ended by a semicolon, unless the item has subitems.

## comments

Two forward slashes(//) in a row indicates that the following text is interpreted as comments (like the line : _BAG is the registration of buildings and addresses in the Netherlands in the example_).

To comment blocks with multiple lines, use /\* to start and \*/ to finish a comment block.