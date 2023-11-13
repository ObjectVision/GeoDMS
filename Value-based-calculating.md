*[[Under Study]] Value based calculating*

The GeoDMS is controlled by a declarative scripting language, which is used to define data-values, and not variables with intermediary states. This helps to make the modelling logically transparent and results traceable.

There are still many changes that could affect the validity of calculated values, such as external data changes, changes of calculation rules and the introduction of new items or removal of existing items.

We will take time to think and write about a more rigorous approach to identify CalcCache entries, also how this relates to working more interactively with the [[GUI|GeoDMS GUI]].

### definitions

#### named and cached items

A named item is a item that is configured, instantiated from a template,
or representing a template instantiation argument

-   A named item has a parent (the name of the root container is nowhere used and is now irrelevant).
-   Parent is a namespace and associates a unique name with a named item.
-   A parent is also a named item or the config root
-   instantiated items are copies of template or function definition items or created by a for_each operator (or the depreciated loop operator)
-   moniking items refer to operator result sub items.

#### values and futures

A future T:

-   holds a task<T> or a T or a ErrMsg
-   has a sequence of supplying future Args..., which form a DAG
-   can be shared and over multiple threads, similar to std::shared_future<T>
-   can be constructed with a functor(Args...) -> T and a list of future<Args>...)
-   can be executed inline or by a DAG aware scheduler

A task<T>:

-   has a functor (Args...)->T that produces T or an exception,
-   results in a T or a ErrMsg,
-   P.M.: a dedicated rapid complexity bound estimator: functor() -> {
    time, intermediate memory, resulting memory }

Each named attribute<T> can:

-   produce a future\< tileDataProducer<T> >

tileDataProducer<T>

-   is a functor (0..nrTiles-1) -> future\< elemArray<T> >
-   can be implemented as an array of elemArray<T>, reduced to a singe elemArray<T> when the number of tiles = 1.

<pre>
struct tileContainer&lt;T&gt;
{
   array&lt;elemArray&lt;T&gt; m_Segs;
   future&lt;elemArray&lt;T&gt; operator(tile_id t) const { return m_Segs[t]; }
}
</pre>

-   can be implemented as

<pre>
std:shared_ptr&lt;tileDataProducer&lt; U &gt; &gt; ...argProducers; 
[argProducers](tile_id t)
{
   return future&lt; elemArray&lt;T&gt; >(
      [](elemArray&lt; U &gt; . . . args){ elemArray&lt;T&gt;  result; for(i: domain) result.emplace_back(f(args[i]...)); return result; }
      , *argProducers(t)...);
}
</pre>

elemArray<T>

-   is an array of a T for each tile-elem-index
-   once produced, can be shared over multiple threads
-   can be a heap_array<T> or a file_mapped_array<T>

elemArray\<sequence<T>\>

-   is an array of sequences of T for each tile-elem-index
-   once produced, can be shared over multiple threads
-   can be a heap_sequnce_array<T> or a file_mapped_sequence_array<T>

Each named unit<T> can have a

<pre> 
future&lt;unitdata&lt;T&gt; &gt;
</pre> 

with

<pre>
unitdata&lt;t> :=  {
  Metric or Projection; 
  range&lt;T&gt;; 
   nrTiles: tileid;  
   GetTileRange(0..nrTiles-1) -> range&lt;T&gt; 
 }
</pre>

### migration path

This scheme replaces InterestCounting, DataReadLock, DataWriteLock,
ItemReadLock, ItemWriteLock, WritableTileLock, ReadableTileLock

A (Prepared)DataRead can now contain an ItemReadLock, a DataReadLockAtom, and a ReadableTileLock. A DataWriteLock now contains a WritableTileLock

Checks are/should be added that:

-   each DataReadLock was Prepared in advance and shielded by a pre-existing ItemReadLock
-   each DataWriteLock is shielded by an ItemWriteLock
-   no TileLock is requested with the DataLock.
-   all TileLocks are shielded by a DataLock
-   unique WritableTileLock -> std::shared_ptr\<elemArray<T>\>

ReadableTileLock -> ado->GetLockedDataRead() -> future\<elemArray<T>\>.get() WritableTileLock -> ado->GetLockedDataWrite() -> sets a value and fills a promise upon commit or sets a promise to canceled when demand was gone or sets it to the currently active ErrMsg if stack was unwound exceptionally.

-   item::PrepareDataUsage should return a future<ItemReadLock>
-   OperationContext -> future<ItemReadLock>
    -   OperationContext CTOR takes m_FuncDC and Func and additinal otherSuppliers, nothing later
    -   FuncDC::m_OperContext's CreateResult stuff moves back to FuncDC 
    -   argsReady()->bool becomes the negative of hasConnectedSuppliers(); check that connectArgs
-   ItemReadLock and ItemWriteLock operate on TreeItem::m_SharableLock of type similar to ItemLocks.cpp::impl::treeitem_production_task

#### Extent of data_locks

shared_data_locks (fka ItemReadLocks) now lock container (fna parent) first, unique_data_locks (fna ItemWriteLocks) don't. This effectively implies that a unique_lock of a container doesn't allow for a shared_lock for any of it's components (fna sub-items).

-   consider reversing this
-   (now) the extent of a unique_data_lock on a cache root includes all data of its components
-   the (final) release of a shared_read_lock should consider freeing the data resources of the related item and its components. KeepData, other shared_read_locks on components, and herisitics of data size, calculation time indication, and usage patterns should be considered.
-   DC::GetResult() returns a geodms_future: i.e. a shared_ptr<OperationContext>, shared_read_lock or a ErrMsg, and should/does cover all components of the cache item.
    -   a shared_read_lock or ErrMsg can be returned when the related OperationContext is completed or when the data of the cacheItem and its components are in cache or memory. Evaluation of this should be synchronized with other shared_read_lock operations
    -   DC(subitem(x, name))::GetResult() returns, a shared_read_lock to the sub_item only, releasing ownership of x, which can release shared ownership of data of other components of x.
    -   when x::GetResult() returns the geodms_future shared_ptr<OperationContext>, DC(subitem(x, name))::GetResult() returns a new shared_ptr<OperationContext> that owns the original one and requests a shared_data_lock on named component from the shared_data_lock of the container.

#### TimeStamps and invalidations

### *ExplainValue*

P.M.

### integrity checks and data validation

Now IntegrityChecked data is the 3rd level of Progress.

Data (TI)

IcData(ConfigItem) := { Data(ConfigItem), IcMsg(ConfigItem) }

IcMsg(ConfigItem) := merge(IcMsg(Suppliers(ConfigItem))...,
IcMsg(parent(ConfigItem), IcCheck(ConfigItem))

IcCheck(ConfigItem) := IC(Item)==empty ? empty : eval(item, IC(Item))

### *source data changes, model changes and reevaluation*'

P.M.

### *commits and other external effects*'

P.M.

## related issues

- [Issue: 1064](http://mantis.objectvision.nl/view.php?id=1064)
- [[SAWEC 2.0 WP3 IT-infra]]

## breaking changes of GeoDMS 8

The following changes may break configurations that worked with GeoDms
7.4xx and might have to be updated.

-   reading unit range data from a .fss
-   processing item names that were generated from a gdal.vect storage manager for columns that have multiple operators and/or spaces, as now AsItemName function is used.

## external links

Taskflow: A Parallel and Heterogeneous Task Programming System Using Modern C++ - Tsung-Wei Huang: <https://www.youtube.com/watch?v=MX15huP5DsM&t=2050s>

Rust programming language concepts: <https://en.wikipedia.org/wiki/Rust_(programming_language)>

Functional programming: <https://en.wikipedia.org/wiki/Monad_(functional_programming)> \[^\]

I See a Monad in Your Future: <https://www.youtube.com/watch?v=BFnhhPehpKw>

Managarm: A Fully Asynchronous OS Based on Modern C++ - Alexander van der Grinten: <https://www.youtube.com/watch?v=BzwpdOpNFpQ>

Polyhedral Compilation, Domain Sets, Tiling, calculation step fusion / tile pipelining, compiler tools for DSLs, Convolution, Dependencies, Locality, Albert Cohen - PLISS, 2019:

-   (1/2): <https://www.youtube.com/watch?v=mt6pIpt5Wk0>
-   (2:2): <https://www.youtube.com/watch?v=3TNT5rFVTUY>