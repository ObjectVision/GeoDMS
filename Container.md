A container is a structuring [[tree item]], not being a [[unit]] or [[data item]].

A container often has a role as a [folder](https://en.wikipedia.org/wiki/Directory_(computing)#Folder_metaphor) in a directory structure (although in a GeoDMS configuration also units and data items can contain [[subitems|subitem]]).

To configure a container use the keyword container, followed by it's name (see example).

Curly brackets { and } are used to configure the subitems of a container.

## instantiation

Containers are also used for [[template]] instantiations.

To configure a container for a [[case instantiation]], use the keyword container, followed by it's name and as [[expression]] the name of the template with between brackets the [[case parameters|case parameter]] (see example 2).

## example

<pre>
1. container SourceData
2. container Yr2050 := AllocatieLandUse('yr2050');
</pre>

The first example indicates that this branche of the tree is used to configure source data items, often read from external storages.

The second example is a case instantiation. The template AllocatieLandUse is here instantiated with one case parameter: a year
[[parameter]] as string.