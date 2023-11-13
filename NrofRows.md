*[[Unit functions]] NrofRows*

## concept

- NrofRows() is a function **resulting in the number of elements** of a [[domain unit]].
- NrofRows is a [[property]] used to **configure the number of elements** of a domain unit

The first part of this page describes the NrofRows() function, the last paragraph describes the NrofRows property

## syntax

- NrofRows(*domainunit*)
- #*domainunit*

## definition

NrofRows(*domainunit*) or \#*domainunit* results in a uint32 [[parameter]] with the **number of entries/elements ([[cardinality]])** of the [[argument]] *domainunit*.

## applies to

- domain unit *domainunit* with [[value type]] of the group CanBeDomainUnit

## example

<pre>
1. parameter&lt;uint32&gt; rowsRegions := <B>NrofRows(</B>Region<B>)</B>; *result = 5*
2. parameter&lt;uint32&gt; rowsRegions := <B>#</B>RegionDomain;    *result = 5*;
</pre>

| RegionDomain |
|-------------:|
| 0            |
| 1            |
| 2            |
| 3            |
| 4            |

*domain Region, nr of rows = 5*

## see also

- [[range]]


# property

NrofRows is a property configured for a domain unit, to inform the GeoDMS about the number of elements/entries of the domain. This number is also called the [[cardinality]].

For domain units with data read from external [[storages|StorageManager]] or with an [[expression]], the number of elements is read/calculated. So for these domain units this NrOfRows property does not have to be configured.

The [[range]] function can be used as alternative for the NrOfRows property and offers more functionality, for example to non zero based domain units).

## configuration

The NrofRows [[property]], due to historical reasons, has a little different syntax (no double quotes) as a normal property, see the example

<pre>
unit&lt;uint8&gt; WeekDays: <B>NrofRows = 7</B>
{
   attribute&lt;string&gt; name : ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
}
</pre>
