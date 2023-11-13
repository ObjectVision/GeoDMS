*[[File, Folder and Read functions]] ReadValue*

## syntax

- ReadValue(*source*, *valuetype*, *startposition*)

## definition

ReadValue(*source*, *valuetype*, *startposition*) results in a **single [[parameter]] value**, read from the *source* [[argument]].

The second argument *valuetype* configures the [[value type]] of the resulting parameter.

The third argument *startposition* configures the read position in the *source*. Spaces/tabs are used as delimiter.

The numbering of the positions is counted from left to right, meaning position two is the position of the second character in the first row.

The ReadValue function also generates a uint32 parameter [[subitem]], called ReadPos. This parameter indicates the position of the next value after the value being read by the ReadValue function, see the example.

## description

The ReadValue function is used to read a specific value from a text file, like an ini file.

Use the [[ReadArray]] function to read whole rows from a text file.

## applies to

- parameter *source* with string value type
- *valuetype* can be any value type
- parameter *startposition* with uint32 value type

## example

<pre>
container ReadValue
{
   parameter&lt;string&gt;  file: StorageName = "%projDir%/data/text.txt",  StorageType = "str";

   parameter&lt;uint32&gt;  Value1 := <B>ReadValue(</B>file, uint32,  0<B>)</B>;              <I>result = 6</I>
   parameter&lt;float32&gt; Value2 := <B>ReadValue(</B>file, float32, Value1/ReadPos<B>)</B>; <I>result = 9.02</I>
   parameter&lt;string&gt;  Value3 := <B>ReadValue(</B>file, string,  Value2/ReadPos<B>)</B>; <I>result = Test</I>
   parameter&lt;bool&gt;    Value4 := <B>ReadValue(</B>file, bool,    Value3/ReadPos<B>)</B>; <I>result = True</I>
   parameter&lt;uint32&gt;  Value5 := <B>ReadValue(</B>file, uint32,  Value4/ReadPos<B>)</B>; <I>result = 25000</I>
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

## see also

- [[ReadArray]]
- [[ReadElems]]
- [[ReadLines]]