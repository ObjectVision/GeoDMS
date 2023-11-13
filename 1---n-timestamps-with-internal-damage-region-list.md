## Description

The major difference between this design and the current design is that instead of using a globally managed timestamp, multiple indepdendent timestamps are used. Each operation manages its own timestamp. Because the timestamps of multiple operations are no longer in sync, consumers must use a different tracking timestamp for each source: 1 (operation) + n (one for each source) timestamps per operation.

The primary data sources still use one centrally managed timestamp in the same way as is done in the current model.

To be able to implement incremental updating, postponing of updates and minimizing recalculation, each operation keeps an internal list with damage regions describing what parts must be recalculated. This list is updated with the unprocessed damage regions of all the sources during the update process.

Note that the text below assumes complex operations. Simpler operations will be treated the same but generic values for damage regions (completely changed) and areas of interest (everything) will be used instead.

## Global algorithm for operations

When an operation is asked to update itself the following procedure is used:

1.  Retrieve the unprocessed damage regions from the sources. The tracking timestamp for each source is used to determine what the unprocessed damage regions are. After retrieving the damage regions the tracking timestamp is set to the current timestamp of the source.
2.  Merge the retrieved damage regions with the internal list of damage regions. If needed the damage regions of the sources are translated to the damage regions of the operation. During the merge the damage regions are coalesced to prevent overlapping regions (and subsequent redundant recalculation).
3.  Retrieve the damage regions matching the area of interest from the list and process them.
4.  When finished (continue flag returns 'false', area of interest recalculated, or damage region list emtpy) generate damage regions for each of the outputs based on the progress that has been made. Assign the output damage regions the current timestamp of the operation. Also any unprocessed parts of the last damage region are readded to the damage region list so they will be processed during the next update.
5.  Increase the timestamp of the operation.

The order in which the actions of the procedure above are performed may be significantly different in the actual implementation.

## Global algorithm for update driver

Update drivers use the following procedure to update the operations:

1.  If the operation is in the set of up-to-date operations, return immediatly.
2.  Ask the operation for its sources and corresponding areas of interest based on the area of interest.
3.  Run this procedure for each source.
4.  Tell the operation to update the area of interest.
5.  If the operation indicates that it is fully up-to-date, add it to the set of up-to-date operations.

## Postponing expensive operations

If a fast update of some outputs is needed, and showing the current data is not necessary, the calculation of expensive operations can be postponed.

Postponing is very simple. The set of operations not to be recalculated is passed to the update driver. The update driver will simply add the operations to the set of up-to-date operations, preventing these operations from recalculating their outputs.

Note that any output damage regions of postponed operations that were generated during previous update runs will be processed by consumers of the operation.

## Area of interest

To be able to only update the visible region of views before other regions, views must be able to determine their area of interest, and views and operations must be able to specify which regions of their sources they need so they can update the requested area of interest.

The update driver, views and operations needs to change to make it possible to limit recalculation to the areas of interest of the views. The update driver algorithm has to be changed to the following:

1.  If the operation is in the set of up-to-date operations, return immediately.
2.  Ask the operation for the sources that are needed by the operation to translate the requested area of interest to areas of interests for the sources of the operation.
3.  Run this procedure for each sources using the generic area of interest 'everything'.
4.  Ask the operation for its sources and the corresponding areas of interest based on the area of interest.
5.  Run this procedure for each source.
6.  Tell the operation to update the area of interest.
7.  If the operation indicates that it is fully up-to-date, add it to the set of up-to-date operations.

Note that the update path for sources needed by views and operations to determine the area of interest, must contain only cheap operations to ensure the calculation of the areas of interest won't delay updates to the views.

## Incremental updates of views

Adding incremental updates to views is easy because the algorithm already supports areas of interest. The only thing that needs to be changed is that the update driver asks for a list of areas of interest from the views. The view can then decide how many areas of interest to return. For example: one if zoomed in on a small area, or many if zoomed out to view lots of data. The update driver will then process the returned areas of interest one by one, resulting in an incrementally updated view.

## TODO

-   Determining if data is stale
-   Scenario's