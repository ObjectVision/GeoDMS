*[CalcCache](CalcCache "wikilink") Purpose*

The [GeoDMS](GeoDMS "wikilink") [CalcCache](CalcCache "wikilink") is
in use for two purposes:

## performance

Although the [GeoDMS](GeoDMS "wikilink") is designed to calculate fast
with large datasets (see [Expression](Expression "wikilink"), Fast
Calculations), calculating some results, for instance a 100-meter grid
allocation for large countries in Europe, can take some time.

The [CalcCache](CalcCache "wikilink") can store (part of) these results
on disk. This allows the [GeoDMS](GeoDMS "wikilink") to re-use these
results after being calculated once, provided they are still valid (no
input data or [expression](expression "wikilink") of any
[supplier](supplier "wikilink") is changed). The results can be made
available in the session with which the results are calculated, but also
after restarting the application in a new session.

## calculating with large datasets

The limit of around 1 gigabyte internal memory that can be addressed by
any process in a 32 bits Windows environment became a problem in for
instance the allocation model of the Netherlands at a 100-meter grid
level. The total amount of data needed to calculate all steps in this
model exceeds this limit of 1 gigabyte. The solution is to store interim
results of the calculation process on disk.

The [CalcCache](CalcCache "wikilink") provides the
[GeoDMS](GeoDMS "wikilink") the opportunity to calculate models with
large datasets also in a 32 bits environment, where the total amount of
data used exceeds the 1 gigabyte internal memory limit.

In a 64 bits environment, this limit of 1 gigabyte is not an issue, but
still the physical amount of the internal memory is limited. So also in
64 bits environments the [CalcCache](CalcCache "wikilink") is useful
for this purpose.