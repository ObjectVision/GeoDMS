*[[Relational functions]] merge*

## syntax

- merge(*option*,*valuesunit*, *option1values*, .. , *optionnvalues*)

## definition

merge(*option*, *valuesunit*, *option1values*, .. , *optionnvalues*) results in a [[data item]] with the [[domain unit]] of data items:

- *option*, *option1values*, .. , *optionnvalues* and as
- *[[valuesunit]]* the second [[argument]]: *valuesunit*.

The resulting values are the one of the values of the set:
*option1values*, .. , *optionnvalues*.

The first argument (*option*) indicates from which *optionvalue* [[attribute]] the values are selected. So an option of 0 means the values are selected from the option1values attribute and an option of 2 means they are selected from the option3values attribute.

## applies to

- [[data items|data item]] with Numeric or value types

## conditions

1.  option attribute with uint8 [[value type]] 
2.  domain units of all data items must match or be [[void]].
3.  all *optionvalues* arguments must have matching:
    - value type
    - [[metric]]

## since version

7.184

## example
<pre>
unit&lt;float32&gt;  eur;
attribute&lt;eur&gt; TransportCosts (ADomain) := <B>merge(</B>TransportOption, eur, car, public, bike<B>)</B>;
</pre>

Transport<BR>Option|car(eur),<BR>sequencenr: 0|public(eur),<BR>sequencenr: 1|bike(eur),<BR>sequencenr: 2|**Transport Costs**|
|-----------------:|-------------------------:|----------------------------:|--------------------------:|------------------:|
|0                 |18                        |15                           |12                         |**18**             |
|2                 |28                        |25                           |22                         |**22**             |
|1                 |38                        |35                           |32                         |**35**             |
|null              |48                        |45                           |42                         |**null**           |
|1                 |58                        |55                           |52                         |**55**             |

*ADomain, nr of rows = 5*

## See Also

- [[raster_merge]]