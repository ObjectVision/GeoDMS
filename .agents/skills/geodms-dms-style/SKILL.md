---
name: geodms-dms-style
description: Writing and reviewing GeoDMS configuration code (.dms), in testcases, probes and models. Which value-type conversions are needed and which are noise (#U is uint32 or uint64, sum_uint64 counts a bool directly, arithmetic never widens), filling a small table with one value per element by union_data instead of a switch on id, counting per class, a key from two relations by combine_unit and combine_data instead of index arithmetic, unique combinations with any, unions over parts generated with asList, requesting a summary of fenced results in the same request as the work, writing a small text file, and proving a rewrite on a probe with the same container paths. Use when writing, rewriting or reviewing .dms code.
---

# Writing .dms code

Readable configuration code says each thing once. A conversion, a helper attribute or a
string trick earns its place only when the engine needs it. The rules below come from
rewriting the output counts of NetworkModel_PBL in October 2026 (its commits ec53e1d,
bee7f55 and 6de63ec, and the GeoDMS Academy page *Deepening III, Python and GeoDMS side by
side*); every claim was checked on GeoDMS 20.22.1 with a probe.

## Conversions only where the value types differ

| Write | Not | Why |
|---|---|---|
| `#R` | `uint64(#R)` | `#U` is already `uint64` for a `unit<uint64>` |
| `#U8` where a `uint32` is wanted | `uint32(#U8)` | `#U` is `uint32` for a unit of `uint8`, `uint16` or `uint32` |
| `sum_uint64(IsX)` | `sum(uint64(IsX))` | the `sum_<type>` variants count the true values of a `bool` attribute directly, and say the result type once |
| `=asList(...)` | `='' + asList(...)` | `asList` already yields the string of an indirect expression |
| `combine_data(AB, a_rel, b_rel)` | `uint64(a_rel) * uint64(#B) + uint64(b_rel)` | a key from two relations is a relation to their combined domain; see below |

Conversions that are needed:

- **Mixed widths in arithmetic.** `mul` has no `uint64 x uint32`. With `#B` a `uint32`, the
  engine refuses at once: `Cannot find operator for these arguments: arg1 of type
  DataItem<uint64>, arg2 of type DataItem<uint32>`. Convert the narrow side. (For an index
  computed from relations, do not compute at all: combine them, see below.)
- **A relation used as a number.** An attribute whose values unit is a domain unit is not a
  number; `uint64(a_rel)` makes it one. Before writing one, ask whether the arithmetic is
  needed at all.
- **A declared type that differs from the result.** `parameter<uint8> n := #U8;` fails with
  `ItemType DataItem<uint8> is incompatible with the result of the calculation which is of
  type DataItem<uint32>`.

So test a conversion by removing it: the engine rejects a real mismatch while it derives the
types, before any data is read, and a probe of a few lines settles it in seconds.

## A small table with one value per element: union_data

A table on a classification, filled per element, was often written as a switch on the
element number:

```
attribute<uint8>  Groep_nr (Groep) := uint8(id(Groep));
attribute<uint64> Telling  (Groep) := switch(
	  case(Groep_nr == 0b, uint64(#Paren))
	, case(Groep_nr == 1b, sum(uint64(Paren/A)))
	, case(Groep_nr == 2b, sum(uint64(Paren/B)))
	, uint64(0));
```

Write the values in the order of the elements instead:

```
// one count per group, in the order of Groep
attribute<uint64> Telling (Groep) := union_data(Groep
	, #Paren
	, sum_uint64(Paren/A)
	, sum_uint64(Paren/B));
```

- A parameter fills one element, so the number of parameters must equal `#Groep`. A
  forgotten element is an error (`ValidateCount(2) failed because this unit has count 3`,
  in function union_data), where the switch silently fell through to its default.
- No helper attribute for the element number, no default case.
- The order is the coupling between the values and the classification. Say so in a comment
  that names the classification, and keep the classification's `name` list close.

## Counting per class

- **A few named classes, plus a row for all.** Use the same form:
  `union_data(Soort, #R, sum_uint64(IsKeten), sum_uint64(IsWW), ...)`. Each class is a
  condition a reader can check, and the total needs no exception.
- **Many classes, or classes that come from data.** `pcount(class_rel)` is the tool: one pass,
  one count per class. Do not patch one of its elements afterwards, as in
  `pcount(rel) + (id(.) == 0 ? #R : 0)`. Keep the total as a separate item, or use the
  `union_data` form.
- **Compare with named elements, not numbers.** A classification can carry a parameter per
  element, `container V := for_each_nedv(name, String(ID(.))+'[..]', void, .);`, so a
  condition reads `R/mode_rel == Modes/V/Walking`.

## A key from two relations: combine, not index arithmetic

Two relations together identify an element of their combined domain. Say that, instead of
computing an index by hand:

```
unit<uint64>  AB          := combine_unit_uint64(A, B);
attribute<AB> AB_rel (R)  := combine_data(AB, R/a_rel, R/b_rel);
```

not

```
attribute<uint64> key (R) := uint64(R/a_rel) * uint64(#B) + uint64(R/b_rel);
```

- The numbering is the same (the first argument varies slowest), so nothing downstream
  changes, but the result is a relation with a meaning instead of a number: its values unit
  tells a reader what it indexes, and the conversions and the multiplication are gone.
- Pick the value type of the combined unit so that `#A x #B` fits: `combine_unit_uint64` for
  two `uint32` domains.
- Use `combine_unit_<type>` for large domains. `combine` itself also adds the subitems
  `first_rel` and `second_rel`, attributes on the combined domain, which can explode memory
  (the `combine` wiki page).
- More than two relations: chain them, `combine_data(ABC, a_rel, combine_data(BC, b_rel,
  c_rel))` (the `combine_data` wiki page).
- A large combined range costs nothing by itself: with 1 million places (a range of 10^12)
  and 10 million rows, `unique`, `rlookup` and `any` on the `combine_data` relation took as
  long as on the hand-made `uint64` key (0.5 s, 0.3 s and 0.1 s), with the same result.
- Where several items use the combined unit (all departure moments of a block, say), declare
  it once next to its factors, not inside each template instance: one definition for one
  meaning. It is a matter of clarity, not of necessity; in a probe, `union_data` accepted
  relations to two units with the same `combine_unit_uint64` definition.

## Unique combinations, and "at least one"

```
unit<uint64> Pairs := unique(AB_rel)
{
	attribute<bool> hasX := any(../IsX, ../Pairs_rel);
}
attribute<Pairs> Pairs_rel (R) := rlookup(AB_rel, Pairs/values);
```

- `unique` makes the domain of the distinct combinations; its `values` holds them, as
  relations to `AB`.
- `any(condition, relation)` is true for a target element when at least one of its source
  elements meets the condition. `sum_uint64(Pairs/hasX)` then counts the pairs with X.

## A union over parts, generated from a unit

```
unit<uint64> AllPairs := ='union_unit_uint64(' + asList('Part/' + Parts/name + '/Pairs', ',') + ')'
{
	attribute<AB>   AB_rel := ='union_data(., ' + asList('Part/' + Parts/name + '/Pairs/values', ',') + ')';
	attribute<bool> hasX   := ='union_data(., ' + asList('Part/' + Parts/name + '/Pairs/hasX', ',') + ')';
}
```

Then `unique` and `any` again for the distinct keys over all parts. Because the lists come
from a unit, the number of parts stays a parameter.

## A summary of fenced work goes in the same request

A `PhaseContainer` keeps what is behind the fence only while its phase runs. A summary that
reads fenced results therefore belongs in the same request as the work itself:

```
parameter<string> Generate := =asList(Blocks/name + '/Work_Fence/Generate', ' + ') + ' + Summary/Generate';
```

- Requested together, the work runs once and the summary reads its results.
- Requested alone, the summary computes only what it consumes: the rest of the fenced work,
  such as the files it writes, does not happen.
- Requested afterwards, in a separate run, the summary makes the work run again.
- Let only small results cross the fence (counts, keys with flags), so the large
  intermediates of each phase can be freed.

## A small text file

```
attribute<string> Line (Groep) := Groep/label + ';' + string(Telling);
parameter<string> File := 'groep;telling\n' + asList(Line, '\n') + '\n'
,	StorageName = "=OutputDir + '/tellingen.csv'", StorageType = "str";
```

Build headers and lines from the units (`asList` over their names) rather than spelling the
columns out; then a new element or departure moment needs no edit here.

## Proving a rewrite

A real model run can take minutes and tens of GB for one block; a probe takes seconds.

1. Take the code verbatim from the model with a small script, so the probe tests what is
   committed rather than a retyped copy.
2. Put it in a skeleton with the same container paths. Declare the inputs the code uses,
   with test data computed from `id(.)` (for example `value((i * i + 3) % 50, Places)`),
   so a second implementation can produce the expected answer.
3. Run the item that writes the result with GeoDmsRun (geodms-debug), and compare the output
   with an independent computation, in Python say, on the same formulas.
4. Code shown in documentation is tested the same way: assemble the code blocks of the page
   itself into one configuration and run it. That also proves the page defines everything it
   uses.
