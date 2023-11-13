*[CalcCache](CalcCache "wikilink") [Managing
Files](CalcCache_Managing_Files "wikilink")*

## FreeData property

Since [GeoDMS](GeoDMS "wikilink") version 7.122 calculation results are
by default not any longer stored persistently in the
[CalcCache](CalcCache "wikilink"). The default value for the FreeData
[property](property "wikilink") is changed to: true.

By configuring the FreeData [property](property "wikilink") to false,
calculation results, if they meet [all other
conditions](Cached_Results "wikilink"), are stored persistently in the
[CalcCache](CalcCache "wikilink"). If the
[property](property "wikilink") is configured for an
[item](tree_item "wikilink") it applies to this item and all it's
[subitems](subitem "wikilink").

In general it is advised to store only results that are time consuming
to recalculate and that are re-used in the same of in new sessions. This
keeps your [CalcCache](CalcCache "wikilink") size limited and results
might be available faster, as less disk I/O is needed. It requests some
experience on which [expressions](expression "wikilink") are
time-consuming to calculate.