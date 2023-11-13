*[CalcCache](CalcCache "wikilink") Cached Results*

## storing calculation results

Calculated results are stored persistently in the
[CalcCache](CalcCache "wikilink"), if the following conditions are met:

-   The results relate to the calculation rule of a named [tree
    item](tree_item "wikilink"). Results of
    sub-[expression](expression "wikilink") might be stored in the
    transient cache (only available within session) but not stored
    persistently.
-   The data size of the results exceeds a specified threshold (small
    results are by default not stored, see StoreData
    [property](property "wikilink")). The threshold has a default value
    of 4.000 bytes. This value can be changed in the [GeoDMS
    GUI](GeoDMS_GUI "wikilink") with the Tools > Options menu. Open the
    Current Configuration tab and edit the *Minimum size for DataItem
    specific swapfiles in the CalcCache* option. It is advised to keep
    this value limited.
-   The value of the FreeData [property](property "wikilink") is False.
-   For the derivation of the results, no
    [session-specific](session-specific "wikilink") function was used.
-   The calculation result is not a [data item](data_item "wikilink")
    that results directly from a [composite
    function](composite_function "wikilink") (both root result and
    [subitem](subitem "wikilink")) or a direct reference thereto.
    Results derived indirectly from a [composite
    function](composite_function "wikilink"), will be stored
    persistently if they meet all other conditions.

Calculation Results from [MetaScript
functions](MetaScript_functions "wikilink") are new [tree
items](tree_item "wikilink"), for which resulting data can be stored in
the CalcCache, if they meet the above conditions.

## transient CalcCache

The [CalcCache](CalcCache "wikilink") contains a transient part, with
calculation results that might be re-usable during a session, but that
are deleted when the application is closed. This transient part of the
[CalcCache](CalcCache "wikilink") is stored in the .tmp subfolder of
the CalcCache root folder. This part functions as an extension of the
page file mechanism, useful to the relieve the use internal memory and
to cope with the 1 gigabyte limit in the 32 bits environment.

== StoreData Property (default value = "False") == If the
[property](property "wikilink") StoreData = "True" is configured,
condition two (data size) does not longer apply. Use the StoreData
[property](property "wikilink") to store results that have a data size
less than the threshold, but are time consuming to recalculate. If the
StoreData [property](property "wikilink") is not configured (or
explicitly configured as "False"), only [data
items](data_item "wikilink") with a data size above the threshold are
stored.

== FreeData property (default value = "True" since version 7.122) == The
[property](property "wikilink") FreeData = "False" need to be
configured for a [data item](data_item "wikilink") to store the
calculation results of the item and it's [subitems](subitem "wikilink")
in the [CalcCache](CalcCache "wikilink"). We advice to configure
FreeData = "False" only for those items or containers that require
substantial calculation time, have steady definitions and are used more
often than they need to be recalculated. This will limit the
[CalcCache](CalcCache "wikilink") size and the disk I/O. Never
configure FreeData = "False" for results of the [id](id "wikilink"),
[const](const "wikilink") and similar functions as they are faster to
recalculate as reading them from a disk.

== KeepData property (default value = "False") == The KeepData property
has no effect on the [CalcCache](CalcCache "wikilink"). If the KeepData
is configured to True, the results of the item is kept in memory even if
there is no interest from any view. Configuring the KeepData
[property](property "wikilink") becomes more relevant when less items
are Cached. Configure this [property](property "wikilink") to True on
items that are relevant for interactive working. We advice to configure
this especially for items used in [for_each](for_each "wikilink")
functions, which also results in configurations that will expand faster.