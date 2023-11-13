*[[Unit functions]] range*

## syntax

- range(*valuetype*, *startvalue*, *endvalue*)

## definition

range(*valuetype*, *startvalue*, *endvalue*) **sets the range for a unit** with [[arguments|argument]]:

- *valuetype*, the [[value type]] for the configured [[unit]]
- *startvalue*, the lowest value of the allowed range
- *endvalue*, the highest value of the allowed range

The range function configures a half open range. This means the value of argument: *startvalue* is the first value that is part of the range. Argument endvalue is the first value that falls outside the given range.

## description

The number of entries/elements of a [[domain unit]] can be configured with this range function or with the [[NrofRows]] [[property]]. The range function is more flexible as:

- It can also be used for two-dimensional domain units, [[grid domains|grid domain]], with a [[value type]] of the pointGroup;
- all arguments can be configured explicitly, but can also be the results of calculations.
- the *startvalue* argument can be different than the default value of 0.

In earlier GeoDMS versions also a range property was in use. This property was used to specify the range a unit with two arguments, a *startvalue* and *endvalue*. This property is nog longer supported.

## applies to

- [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameter]] *valuetype* can be any value type.
- literals or parameters *startvalue* and *endvalue* of the configured *valuetype* argument

## example

1. uint32 domain unit

<pre>
unit&lt;uint32&gt; Province := <B>range(</B>uint32, 1, 13<B>)</B>
{
   attribute&lt;.&gt; id := id(.);
}
</pre>

| Province/id |
|------------:|
| 1           |
| 2           |
| 3           |
| 4           |
| 5           |
| 6           |
| 7           |
| 8           |
| 9           |
| 10          |
| 11          |
| 12          |

*domain Province, nr of rows = 12*

2. spoint domain unit

<pre>
unit&lt;spoint&gt; GridDomain := <B>range(</B>spoint, point(10s, 14s), point(15s, 19s)<B>)</B>
{ 
   attribute&lt;.&gt; id := id(.); 
}
</pre>

GridDomain/id

|         |         |          |         |         |
|--------:|--------:|---------:|--------:|--------:|
| (10,14) | (10,15) | (10,16)  | (10,17) | (10,18) |
| (11,14) | (11,15) | (11,16)  | (11,17) | (11,18) |
| (12,14) | (12,15) | (12,16)  | (12,17) | (12,18) |
| (13,14) | (13,15) | (13,16)  | (13,17) | (13,18) |
| (14,14) | (14,15) | (14,16)  | (14,17) | (14,18) |

*GridDomain, nr of rows = 6, nr of cols = 5*