*[[Miscellaneous functions]] regex_replace*

## syntax

- regex_replace(*source*, *syntax*, *newvalues*, *optionalflag*)

## definition

regex_replace(*source*, *syntax*, *newvalues*, *optionalflag*) results in a string [[data item]] in which the **substrings** of the *source* [[argument]] that meet the *syntax* argument are **replaced by** the *newvalues* argument.

A fourth optional argument **optionalflag** can be configured to control how the *source* string is matched against the *syntax* expressed by the *syntax* argument.

## description

regex_match uses the [boost 1.51 regex_match](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/regex_match.html)
function. Click [here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/syntax/perl_syntax.html) for valid regex syntax rules and semantics.

The fourth, *optionalflag* argument is optional. More information on this argument can be found [here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/match_flag_type.html).

In the GeoDMS this argument is passed as a uint32 [[parameter]], see the [enumeration](https://en.wikipedia.org/wiki/Enumeration) of [[regex flags]].

## applies to

- data items *source*, *syntax* and *newvalues* with string value type 
- parameter *optionalflag* with uint32 [[value type]]

## since version

7.011

## example
<pre>
parameter&lt;string&gt; Text := dquote('Corop') + ';' + dquote('CoropLabel')";

<I>// the following regex_replace function replaces double quoted headers</I>
<I>// by the newvalue: 'NewLabel'</I>

parameter&lt;bool&gt; NewLabels := <B>regex_replace(</B>Text, '\"[^\"]*+\"',quote('NewLabel'));
</pre>

*result = 'NewLabel';'NewLabel*'

## see also

- [[regex_match]]
- [[regex_search]]
- [[replace]]