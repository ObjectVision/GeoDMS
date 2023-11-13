To make the calculation steps of a model transparent, it is important that all intermediate results can be visualized.

Therefore data items have a constant state in a calculation process. Constructions often used in programming languages like A = A + 1 are therefore explicitly forbidden, as the value of variable A in such a construction is dependent on the progress in the calculation process and intermediate results can not easily be requested.

In a GeoDMS configuration each new value for variable A requests a new data item.

## states

In the GeoDMS a [[data item]] can have one of the following states:

-   **Not Calculated** (not yet calculated or a supplier is not anymore valid)
-   **MetaData updated** (the metadata of a data item is ready. If the item is not red, it means the expression is ok in terms of it's metadata)
-   **Calculating**
-   **Valid** (if the calculations finished successfully)
-   **Failed** (in case an error occurred at the meta or primary data level or an integrity check failed)