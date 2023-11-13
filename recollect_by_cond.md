*[[Relational functions]] recollect_by_cond*

## description

Recollects attribute values of a selection back to an original set using the condition used for the selection.

## syntax

1. recollect_by_cond(condition: D->Bool, values: S->V, fallbackValues: D->V)
1. recollect_by_cond(condition: D->Bool, values: S->V, fallbackValue: ->V)
1. recollect_by_cond(condition: D->Bool, values: S->V)

## definition

recollect_by_cond results in an attribute D->V having values taken from the values argument where a condition is true and using fallbackValue or fallbackValues where a condition is false, or undefined when no fallbackValues are provided.

## example

```
attribute<JunctionFreeSection> JunctionFreeSection_rel := 
   recollect_by_cond(ConnectedParts/IsJunctionFreeSection, ID(JunctionFreeSection));
```