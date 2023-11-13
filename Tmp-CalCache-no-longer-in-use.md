*[[Recent Developments]]: tmp CalCache no longer in use*

The .tmp part of the CalcCache was used to store non persistent intermediary results on disk, during a GeoDMS session.

Since 7.196 all non-persistent intermediary results are not longer stored on disk, but in the (system page file backed) heap.

EmptyWorkingSet is called when in stress. Removing the tmp CalcCache increases performance as it limits the disk I/O.