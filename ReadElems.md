*[[File, Folder and Read functions]] ReadElems*

## syntax

- ReadElems(*sourceLines*, *valuesunit*, *startpositions*, *flagsvalue*)

## definition

ReadElems(*sourceLines*, *valuesunit*, *startpositions*, *flagsValue*) results in an [[attribute]], with **column(s) of values** of the *sourceLines* [[argument]], for the [[domain unit]] of the first and third argument.

The resulting [[data item]] contains the values of the *sourceLines* argument, starting at the *startpositions* that need to be configured for each element of the domain unit.

Spaces, tabs, comma's(if comma is not configured as decimal separator) and semicolons are used as field delimiters.

This function also generates an uint32 attribute for the same domain unit, called readPos, containing the read positions after parsing a value from each line.

The position is after reading the delimiter that ended the scanning of the read element, so this can be directly used to read the next field form a set of lines.

## applies to

- attribute *sourceLines* with string [[value type]]
- *valuesunit* is the [[unit]] that is attributed to the resulting values and their value type. This type determines how characters are scanned, ie. integer are read until a non integer character is scanned, floating points can contain a decimal separator (default is the period) and can be in scientific notation (i.e. 1.23e+4). Strings can be quoted and quotes (single or double) will be removed during reading.
- *startpositions* is a an attribute with uint32 value type
- *flagsValue* is an optional uint32 parameter that indicates reading flags, which can be an additive combination of any of the following values:
    - 1: commaAsDecimalSeparator: indicates a comma is interpreted as decimal separator, without this flag, the point is assumed to be the decimal-separator.
    - 2: noNegatives: indicates that a hyphen will be interpreted as a separate single character punctuation token and not as the start of a numeric value (starting from 7.411)
    - 4: noScientific: indicates that scientific number representation (with 'e') is not an option when reading a number (starting from version 7.411)
    - 8: noTabAsValueSeparator (starting from version 7.411)
    - 16: noSpaceAsStringTerminator (starting from version 7.411)

## conditions

- The domain unit of the *sourceLines* and *startpositions* arguments must match.

## example

<pre>
container ReadElems
{
   parameter&lt;string&gt;  file: StorageName = "%projDir%/data/text.txt",  StorageType = "str";

   unit&lt;uint32&gt;       BDomain: nrofrows = 6;
   parameter&lt;float32&gt; Body:= ReadLines(File, BDomain, 0);

   attribute&lt;uint32&gt; FirstColumn (ReadLine/BDomain) := <B>ReadElems(</B>Body, uint32, const(0, BDomain)<B>)</B>;
   attribute&lt;uint32&gt; LastColumn  (ReadLine/BDomain) := <B>ReadElems(</B>Body, uint32, FirstColumn/readPos<B>)</B>;
};
</pre>

|       |       |       |      |       |     |     |
|------:|------:|------:|-----:|------:|----:|----:|
| 25000 | 27500 | 11000 | 6100 | 14400 | 300 | 70  |
| 30000 | 32500 | 12000 | 6200 | 14500 | 320 | 72  |
| 35000 | 37500 | 13000 | 6300 | 14600 | 340 | 74  |
| 40000 | 42500 | 14000 | 6400 | 14700 | 360 | 76  |
| 45000 | 47500 | 15000 | 6500 | 14800 | 380 | 78  |
| 50000 | 52500 | 16000 | 6600 | 14900 | 400 | 80  |

*text.txt*

| **FirstColumn** | **LastColumn** |
|----------------:|---------------:|
| **25000**       | **70**         |
| **30000**       | **72**         |
| **35000**       | **74**         |
| **40000**       | **76**         |
| **45000**       | **78**         |
| **50000**       | **80**         |

*BDomain, nr of rows = 6*

## see also

- [[ReadValue]]
- [[ReadArray]]
- [[ReadLines]]
- [[TableChopper (read ascii file)]]