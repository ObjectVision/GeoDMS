The GeoDMS Engine is the core of the GeoDMS software.

[[images/GeoDMSEngine_w400.png]]

The Engine parses GeoDMS [[configuration files|Configuration file]] and controls an efficient calculation process.

Results are calculated when requested by views in the [[GeoDMS GUI]] or by updates from the [[GeoDMSRun]]. Requested results are translated by the engine in a depedency tree of items that need to be calculated.

In the configuration a large set of [[Operators and functions]] can be configured. Efficient algorithms to calculated with these operators and functions are part of the Engine.

## software

The engine consists of a set van dll's written in [C++](https://en.wikipedia.org/wiki/C%2B%2B).