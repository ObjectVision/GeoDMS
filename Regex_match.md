*[[Miscellaneous functions]] regex_match*

## syntax

- regex_match(*source*, *syntax*, *optionalflag*)

## definition

regex_match(*source*, *syntax*, *optionalflag*) results in a boolean [[data item]] indicating if the *source* data item matches the *syntax* expressed by the *syntax* [[argument]].

A third optional argument *optionalflag* can be configured to control how the source string is matched against the *syntax* expressed by the *syntax* argument.

## description

regex_match uses the [boost 1.51 regex_match](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/regex_match.html) function. Click [here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/syntax/perl_syntax.html) for valid regex syntax rules and semantics.

The third, *optionalflag* argument is optional. More information on this argument can be found [here](https://www.boost.org/doc/libs/1_51_0/libs/regex/doc/html/boost_regex/ref/match_flag_type.html).

In the GeoDMS this argument is passed as a uint32 [[parameter]], see the [enumeration](https://en.wikipedia.org/wiki/Enumeration) of [[regex flags]].

## applies to

- data items *source* and *syntax* with string [[value type]]
- parameter *optionalflag* with uint32 [[value type]]

## since version

7.011

## example

<pre>
parameter&lt;string&gt; Text  := dquote('Corop') + ';' + dquote('CoropLabel');
parameter&lt;string&gt; Quote :=  quote('Corop') + ';' +  quote('CoropLabel');
parameter&lt;string&gt; Comma := dquote('Corop') + ',' + dquote('CoropLabel');

<I>// the following regex_match functions check if the text parameters only</I>
<I>// contain double quoted column names separated by semicolons</I>
<I>// and no other characters</I>

parameter&lt;bool&gt; T := <B>regex_match(</B>text,  '"[^"]*+"(;"[^"]*+")*+'<B>)</B>; <I>result = True</I>
parameter&lt;bool&gt; Q := <B>regex_match(</B>quote, '"[^"]*+"(;"[^"]*+")*+'<B>)</B>; <I>result = False (due to single quote)</I>
parameter&lt;bool&gt; C := <B>regex_match(</B>comma, '"[^"]*+"(;"[^"]*+")*+'<B>)</B>; <I>result = False (due to comma)</I>
</pre>

## see also

- [[regex_search]]
- [[regex_replace]]