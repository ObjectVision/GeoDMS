*[[File, Folder and Read functions]] ReadArray*

## syntax

- ReadArray(*source*, *domainunit*, *valuetype*, *startposition*, *decimal-seperatorflag*)

## definition

ReadArray(*source*, *domainunit*, *valuetype*, *startposition*, *decimal-seperatorflag*) results in an [[attribute]], with **row(s) of values**, read from the *source* [[argument]].

The second *domainunit* configures the [[domain unit]] of the resulting attribute.

The third argument *valuetype* configures the [[value type]] of the resulting attribute.

The fourth argument *startposition* configures the read position in the *source*. Spaces/tabs are used as delimiter.

The numbering of the positions is counted from left to right, meaning position two is the position of the second character in the first row.

The ReadArray function also generates a uint32 [[parameter]] [[subitem]], called ReadPos. This parameter indicates the position of the next value after the value being read by the ReadArray function, see the example.

## description

The ReadArray function is used to read a specific row from a text file, like an ini file.

Use the [[ReadValue]] function to read specific values from a text file.

## applies to

- parameter *source* with string value type
- [[unit]] *domainunit* with value type from group CanBeDomainUnit
- *valuetype* can be any value type
- parameter *startposition* with uint32 value type
- *decimal-separator*, an optional additional uint32 parameter or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) with two possible values:
    - *0* (default value): indicates a point is interpreted as decimal seperator
    - *1*: indicates a comma is interpreted as decimal seperator.

## example

<pre>
container ReadArray
{
   parameter&lt;string&gt;  file:  StorageName = "%projDir%/data/text.txt",  StorageType = "str";

   unit&lt;uint32&gt; HDomain: nrofrows = 4;
   unit&lt;uint32&gt; BDomain: nrofrows = 6;

   attribute&lt;string&gt; Header   (HDomain) := <B>ReadArray(</B>File, HDomain, string, 0<B>)</B>;
   attribute&lt;string&gt; BodyRow1 (BDomain) := <B>ReadArray(</B>File, BDomain, string, Header/ReadPos<B>)</B>;
};
</pre>

|       |       |       |      |       |     |     |
|------:|------:|------:|-----:|------:|----:|----:|
| 6     | 9.02  | Test  | True |       |     |     |
| 25000 | 27500 | 11000 | 6100 | 14400 | 300 | 70  |
| 30000 | 32500 | 12000 | 6200 | 14500 | 320 | 72  |
| 35000 | 37500 | 13000 | 6300 | 14600 | 340 | 74  |
| 40000 | 42500 | 14000 | 6400 | 14700 | 360 | 76  |
| 45000 | 47500 | 15000 | 6500 | 14800 | 380 | 78  |
| 50000 | 52500 | 16000 | 6600 | 14900 | 400 | 80  |

*text.txt*

| **Header** |
|------------|
| **6**      |
| **9.02**   |
| **Test**   |
| **True**   |

*HDomain, nr of rows = 4*

| **BodyRow1** |
|-------------:|
| **25000**    |
| **27500**    |
| **11000**    |
| **6100**     |
| **14400**    |
| **300**      |
| **70**       |

*BDomain, nr of rows = 6*

## see also

- [[ReadValue]]
- [[ReadElems]]
- [[ReadLines]]