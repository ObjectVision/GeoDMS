The current update model is limited in its support for GUIs. For example, it does not support postponing updates of operations to ensure quick redrawing of all GUI components, and it does not support incremental updates (e.g. tiling) for a view. As a result feedback to the user is less than optimal, and may lead to the user thinking that his actions have no effect.

To improve the support of GUIs the current update model has to be changed, or a new one has to be designed. The requirements for the new version are listed below. Below the requirements are links to pages with design proposals with their pros and cons.

## application types

Here is a list of applications where an improved update model can be used:
- GIS models and GUIs
- CRUD GUIs: updating of error indicators, enabling and disabling controls depending on input, synchronizing multiple representations of the same value
- Advanced image manipulation (Photoshop-like applications). These are similar to GIS in that they handle large arrays of 2D data on which operations are performed
- Software build systems

## existing models

### reasons not to select

A lot of existing models exist to update data. Why can't these be used and is this update model needed? The other models have other goals and/or have drawbacks, that make them unsuitable for the application types above:

- Use of events: the biggest drawback of events is that when an event is fired everything has to be updated before the next event can be handled. If multiple changes are made to the primary sources, all outputs have to be recalculated multiple times. This it too inefficient when large data sets are used.
- Data packet streams: some systems process (in most cases intermittent) streams of new data packets. They cannot handle forwarding changes from primary data to outputs.
- Streaming/real-time: these systems are used for continuous streams of values (instead of complex data packets). In most cases the values must be transformed in real-time.

### list

Here is an incomplete list of other model implementations:

- [JavaFBP](http://www.jpaulmorrison.com/fbp/#JavaFBP): streams of data packets.
- [Ptolemy](http://ptolemy.eecs.berkeley.edu/): streams of data packets (Java).

## requirements

### must have

- The developer can explicitly specify which views have to be updated in which order. Some views are more important than others. The most important views have to be updated first because the user is most likely to interact with these views.
- It must be possible to postpone recalculation of expensive operations, so the initial GUI update can be performed quickly. Initially it is more important to update all GUI components than to have the correct data shown. Up-to-date GUI components ensure the user can initiate new actions quickly and ensure users cannot invoke actions that are not applicable anymore. The data in the views can be updated in the background.
- A view must be able to split its content in multiple update regions, and the recalculations must be limited to the minimum set of data needed to update the current region. This way views showing lots of data can be updated incrementally. The user does not have to wait for everything to be recalculated before an update is visible. Instead the user sees many smaller updates, each after a short time, which gives him a feeling that progress is being made. It is also clear how far the update process is.
- No redundant recalculation of (subsets of) derived data. If data is needed but recalculation of an operation has been postponed, derived operations must not recalculate their results using the stale data of the postponed operation. It would be a waste of time as the derived operations would need to be recalculated again when the postponed operation has been recalculated.

### tentative

- If stale data is used (because of postponed recalculation of expensive operations), it must be possible to detect this. The view can then use this information to indicate to the user that the data is not up-to-date. There are two options for the indication: a boolean or a set of regions. The latter is better because it can be used to show users exactly what data has not been updated yet. With a boolean indication the user has to make an educated guess about what data is and what data is not up-to-date.

## proposals

- [[1 + n timestamps with internal damage region list]]

## notes

### tiling of data and operations

The tiling of data and operations is independent of the splitting of views in update regions (which usually means tiling). Tiling of data and operations is used so huge data sets can be paged, and operations can be split in multiple parts for parallel recalculation.

It is probably wise to make the tiles of the views align with the tiles of the underlying operations and data, but this should probably be for the developer to decide instead of views asking their underlying sources for tiling information.