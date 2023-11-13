*[[File, Folder and Read functions]] ReadLines*

## syntax

- ReadLines(*source*, *domainunit*, *startposition*)

## definition

ReadLines(*source*, *domainunit*, *startposition*) results in a [[data item]], with the **concatenation of the values of the rows** of the *source* [[argument]].

The second argument *domainunit* configures the [[domain unit]] of the resulting data item, for [[parameters|parameter]] this [[argument]] is [[void]].

The third argument *startposition* configures the read position in the *source*.

The number of lines read is defined by the number of elements of the domain unit. In the resulting data item the delimiter between each value is the string representation of the tab character (\\t).

The ReadLines function also generates a uint32 parameter [[subitem]], called ReadPos. This parameter indicates the position of the next value after the value being read by the ReadLines function, see the example.

## applies to

- parameter *source* with string [[value type]] 
- [[unit]] *domainunit* with value type from group CanBeDomainUnit
- parameter *startposition* with uint32 value type

## example

<pre>
container ReadLines
{
   parameter&lt;string&gt; file: StorageName = "%projDir%/data/text.txt", StorageType = "str";

   unit&lt;uint32&gt; BDomain: nrofrows = 6;

   parameter&lt;string&gt; Header             := <B>ReadLines(</B>file, void, 0<B>)</B>; <I>result = 6 9.02 Test True</I>
   attribute&lt;string&gt; BodyRows (BDomain) := <B>ReadLines(</B>file, BDomain, Header/ReadPos<B>)</B>;
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

|                                   |
|-----------------------------------|
|                                   |
| **25000275001100061001440030070** |
| **30000325001200062001450032072** |
| **35000375001300063001460034074** |
| **40000425001400064001470036076** |
| **45000475001500065001480038078** |
| **50000525001600066001490040080** |

*BodyRows, BDomain, nr of rows = 6*

## see also

- [[ReadValue]]
- [[ReadElems]]
- [[ReadArray]]