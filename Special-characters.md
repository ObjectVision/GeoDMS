*[[Configuration examples]] Special characters*

## escape characters

In the GeoDMS [escape characters](https://en.wikipedia.org/wiki/Escape_character) can be used to configure special characters, see the examples:

<pre>
parameter&lt;string&gt; SingleQuote := '\''';
parameter&lt;string&gt; DoubleQuote := '\"'; <I>
//  The double quote with an escape character is needed in the expression syntax with expr = ".."
//, as the double quote is also used in this syntax to indicate the start and end of the expression.</I>
parameter&lt;string&gt; tab         := '\t';
parameter&lt;string&gt; newline     := '\n';
</pre>

## missing data

Missing data (null values) can be configured, based on the [[value types|value type]], in different ways, see the examples:

<pre>
parameter&lt;uint32&gt; null_u32    := 4294967295;
parameter&lt;uint32&gt; null_u32    := 0 / 0;
<I>// The null value for uint32 is used as transparent color in items with color visualisation style.</I>
   parameter&lt;uint8&gt;  null_u8     := 255b;
or parameter&lt;uint32&gt; null_u8     := 0b / 0b;
   parameter&lt;string&gt; null_string := string(0 / 0);

parameter&lt;string&gt; emptystring := '';
</pre>

The emptystring [[parameter]] does not configure a null value, but a string with no characters.