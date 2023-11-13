*[[Miscellaneous functions]] regex_search*

## syntax

- regex_search(*source*, *searchsyntax* , *optionalflag*)

## definition

regex_search(*source*, *searchsyntax* , *optionalflag*) results in a string [[data item]] with a substring of the *source* [[argument]] that meets the searchsyntax argument.

A third optional argument *optionalflag* can be configured to control how the source string is matched against the *searchsyntax* expressed by the *syntax* argument.

## description

regex_match uses the [boost 1.51 regex_match](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/regex_match.html) function. Click
[here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/syntax/perl_syntax.html) for valid regex syntax rules and semantics.

The third, *optionalflag* argument is optional. More information on this argument can be found [here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/match_flag_type.html).

In the GeoDMS this argument is passed as a uint32 [[parameter]], see the [enumeration](https://en.wikipedia.org/wiki/Enumeration) of [[regex flags]].

## applies to

- data items *source* and *searchsyntax* with string [[value type]]
- parameter *optionalflag* with uint32 value type

## since version

7.011

## example
<pre>
parameter&lt;string&gt; TextOk := dquote('Corop') + ';' + dquote('CoropLabel');
<I>// the following regex_search function searches the TextOk </I>
<I>// parameter by the configured syntax </I>

parameter&lt;bool&gt; FindSubString := <B>regex_search(</B>Regex_match/TextOk,'"[^"]*+"',0<B>)</B>; <I>result = "Corop"</I>
</pre>
## see also

- [[regex_match]]
- [[regex_replace]]