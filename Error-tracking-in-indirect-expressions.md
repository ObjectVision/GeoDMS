If there is an error in evaluating an [[indirect expression|indirect expression]], it is difficult to find what the error is, especially in complex long expressions.

For example the following error from evaluating the indirect expression displayed below.

    FailReason  iif Error: Cannot find operator for these arguments:
    arg1 of type DataItem<Bool>
    arg2 of type DataItem<String>
    arg3 of type DataItem<Float32>
    Possible cause: argument type mismatch. Check the types of the used arguments.

<pre>
attribute&lt;Woning&gt; Restclaim0 (AllocRegio) := =
    FirstSeqIndicator && FirstIterIndicator && FirstExtentIndicator
        ? ClaimSrc_Wonen
        : FirstSeqIndicator && FirstIterIndicator && !FirstExtentIndicator
            ? 'PerExtent/'+PrevExtent+'/Restclaim0 - PerExtent/'+PrevExtent+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
            : FirstSeqIndicator && !FirstIterIndicator&& FirstExtentIndicator
                ? PrevIter+'/PerExtent/'+LastExtentName+'/Restclaim0 - '+PrevIter+'/PerExtent/'+LastExtentName+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
                : FirstSeqIndicator && !FirstIterIndicator && !FirstExtentIndicator
                    ? 'PerExtent/'+PrevExtent+'/Restclaim0 - PerExtent/'+PrevExtent+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
                    : !FirstSeqIndicator && FirstIterIndicator && FirstExtentIndicator
                        ? 'PerAllocatieSequence/'+PrevSeq+'/DisplacedWonenDoorWerken/DisplacedWonen_perAllocRegio'
                        : !FirstSeqIndicator && FirstIterIndicator && !FirstExtentIndicator
                            ? 'PerExtent/'+PrevExtent+'/Restclaim0 - PerExtent/'+PrevExtent+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
                            : !FirstSeqIndicator && !FirstIterIndicator && FirstExtentIndicator
                                ? PrevIter+'/PerExtent/'+LastExtentName+'/Restclaim0 - '+PrevIter+'/PerExtent/'+LastExtentName+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
                                : !FirstSeqIndicator && !FirstIterIndicator && !FirstExtentIndicator
                                    ? 'PerExtent/'+PrevExtent+'/Restclaim0 - PerExtent/'+PrevExtent+'/Netto_woningbouw/OverExtents/Per_AllocRegio'
                                    : const(0[woning],AllocRegio);

</pre>

Here the error is that _"const(0\[woning\],AllocRegio)" _is not between single quotes, but it can't be seen from the error dialog.