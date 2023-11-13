The focus in the GeoDMS is on a controlled and fast calculation process. Still, when the number of calculation steps increases and the datasets become larger, the time to calculate end results can become substantial.

As the GeoDMS is also intended to work with in an interactive manner, in which multiple alternatives need to be calculated, evaluated and (re)adapted, it is essential to limit the calculation speed but also to make full potential of the fact that certain (intermediate) results are already calculated and can be re-used. For this reason, the GeoDMS contains an intelligent incremental update mechanism. This mechanism guarantees that if data results are requested, they are always (made) valid, minimizing the calculation speed. Only invalidated results are recalculated.

## (in)validated results

Calculation results are invalidated due to changes in:

1. The direct and indirect [[expressions|expression]] used to calculate results;
2. The [[source data|StorageManager]] used.

After updating(recalculating) the invalidated calculation steps, all results are valid again and can be presented to the user. Valid here means the results are consistent with the most recent version of the source data and configured expressions.