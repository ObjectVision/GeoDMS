*[[Configuration examples]] Overlay versus Combine_data*

This script explains the difference between the [[overlay]] and the [[combine_data]] operator.

The overlay operator makes values depending on the occurence of a unique combination in multiple [[data items|data item]]. The [[unique]] operator
on the UnionData [[subitem]] of the overlay result, results in the set of unique occurences of combinations.

The combine_data operator combines data items and result in index number for the combined unit, the first argument of the [[combine]] function. This combined unit combines two units and makes entries for each possible combination.

So the overlay function looks at the actual equal combinations in the data items, where the combine_data looks at all possible combinations.

## how to use

It is adviced to use the overlay function if the number of actual unique combinations in the data is much less than the possible combinations (for instance in determining the set of atomic regions in a land use model).

Use the combine_data function if the number of possible combinations is limited and you want to use the same [[Visualisation Styles|Visualisation_Style]] and labels for the same combinations.

## download

- [configuration/data](https://www.geodms.nl/downloads/GeoDMS_Academy/concepts_overlay_versus_combine_data.zip)

## functions

- [[overlay]]
- [[combine]]
- [[combine_data]]