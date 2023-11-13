*[[Rescale functions]] interpolate_linear*

## syntax

- interpolate_linear(*new*, *org*, *a*)

## definition

interpolate_linear(*new*, *org*, *a*) **interpolates** the values of [[attribute]] *a* towards the [[domain unit]] of attribute *new*.

Attribute *org* contains a reference to values (often timestamps) for which attribute *a* is known.

Attribute *new* contains values in the same range as the *org* attribute. The attribute *a* will be interpolated towards these new values.

The resulting attribute has the same [[values unit]] as attribute *a* and the same [[domain unit]] as attribute *new*.

## applies to

attributes *new*, *org* and *a* with Numeric [[value type]]

## conditions

The domain units of attribute *org* and *a* must match.

## since version

5.55

## example

<pre>
attribute&lt;float32&gt; Quantity (Output) := <B>interpolate_linear(</B>Output/Yr, Decades/TimeStamp, Decades/Quantity<B>)</B>;
</pre>

| Output/Yr |**Quantity** |
|----------:|------------:|
| 2000      | **15763**   |
| 2001      | **15573.6** |
| 2002      | **15384.2** |
| 2003      | **15194.8** |
| 2004      | **15005.3** |
| 2005      | **14815.9** |
| 2006      | **14626.5** |
| 2007      | **14437.1** |
| 2008      | **14247.7** |
| 2009      | **14058.3** |
| 2010      | **13868.9** |

*domain Output, nr of rows = 11*

| Decades/TimeStamp | Decades/Quantity |
|------------------:|-----------------:|
| 2000              | 15763            |
| 2010              | 13868.9          |

*domain Decades, nr of rows = 2*