## how to configure

[[Expressions|expression]] can be configured in two ways:

### advised syntax

The advised syntax (since version 7.105) is to configure an expression after the combination of a colon and an equals sign (:=). 
Expressions are not quoted in this syntax.

See the following examples:

<pre>
attribute&lt;NrHa&gt;   ForestAndNature (rdc_100l) := LandUseClass/Forest + LandUseClass/Nature;
unit&lt;float32&gt;     meter                      := baseunit('m', float32);
parameter&lt;string&gt; capitol                    := 'Amsterdam';
parameter&lt;meter&gt;  marathon                   := 42195[meter];
</pre>

### original syntax

The original syntax (and only syntax until GeoDMS Version 7.104) is to configure an expression with the "expr" [[property]]. With this syntax expression are always configured between double quotes ("").

See the following examples:
<pre>
attribute&lt;NrHa&gt;   ForestAndNature (rdc_100l) : expr = "LandUseClass/Forest + LandUseClass/Nature";
unit&lt;float32&gt;     m                          : expr = "baseunit('m', float32)";
parameter&lt;string&gt; capitol                    : expr = "'Amsterdam'";
parameter&lt;meter&gt;  marathon                   : expr = "42195[meter]";
</pre>

This syntax is still valid, but the new syntax is advised since GeoDMS version 7.105 as:

1.  It is shorter.
2.  It is better understood by grammar configuration files for example in Notepad ++ and or Crimson Editor.
3.  Double quotes in strings can be used without indirections.

## components

Expressions consist of one or a combination of the following components:

1.  other [[data items|data item]], see example 1
2.  [[Operators and functions]], see example 1,2,4
3.  constants, see example 3 and 4