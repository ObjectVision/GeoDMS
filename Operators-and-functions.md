[Operators](https://en.wikipedia.org/wiki/Operator_(mathematics)) and [functions](https://en.wikipedia.org/wiki/Function_(mathematics)) are
used in expressions to calculate with [[data items|data item]] and or [literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)).

## operators

An [[operator]] is a symbolic presentation of a function to be applied on [operands](https://en.wikipedia.org/wiki/Operand).

## functions

Functions express dependences between items. A function associates a single output to each input element. Functions in the GeoDMS are categorized in the following function groups:

-   **[[Arithmetic|Arithmetic functions]]**: basic [mathematical functions](https://en.wikipedia.org/wiki/Mathematics), like [[add]], [[divide|div]] or
[[sqrt]]
-   **[[Ordering|Ordering functions]]**: to compare/order data items like [[eq]], [[less than|lt]], [[argmax]] or [[sort]]
-   **[[Aggregation|Aggregation functions]]**: to aggregate data items to other domain units like [[sum]] or [[mean]]
-   **[[Conversion|Conversion functions]]**: to convert [[value types|value type]], round data items or use different notations
-   **[[Classify|Classify functions]]**: to classify [quantities](https://en.wikipedia.org/wiki/Quantity) to class units 
-   **[[Transcendental|Transcendental functions]]**: functions [transcending algebra](https://en.wikipedia.org/wiki/Transcendental_function),
    like [[exponent]] and [[logarithm]]
-   **[[Predicates|Predicates functions]]**: to check conditions, like [[IsDefined]] of [[IsNull]].
-   **[[Logical|Logical functions]]**: to provide basic comparisons, returning in boolean data items like [[iif]] or [[and]]
-   **[[Relational|Relational functions]]**: to relate data items of different domain units like [[lookup]] or create new domain units like [[unique]]
-   **[[Selection|Selection functions]]**: to create new domain units for selections of data from other domain units like [[select_with_attr_by_cond]]
-   **[[Rescale|Rescale functions]]**: to scale data items to new distributions
-   **[[Constant|Constant functions]]**: to define constant values like [[pi]] or [[true]]
-   **[[Trigonometric|Trigonometric functions]]**: operate on angles like [[sine|sin]] or [[cosine|cos]].
-   **[[Geometric|Geometric functions]]**: geometric operations on [[points|point]], [[arcs|arc]] and/or [[polygons|polygon]]
-   **[[Network|Network functions]]**: to build and calculate [network topologies](https://en.wikipedia.org/wiki/Network_topology) like [[connect]] or     [[impedance|Impedance-functions]]
-   **[[Grid|Grid functions]]**: to calculate with data items of [[grid (two-dimensional) domains|Grid Domain]], like [[potential]] or [[district]]
-   **[[String|String functions]]**: operate on data items with string value types like [[left]] or [[strcount]]
-   **[[File, Folder and Read|File, Folder and Read functions]]**: operate on files and folders, like [[MakeDir]] or [[storage_name]]
-   **[[Unit|Unit functions]]**: to define and get information of [[unit]] items, like [[range]]and [[lowerbound]]
-   **[[Matrix|Matrix functions]]**: to perform [matrix](https://en.wikipedia.org/wiki/Matrix_(mathematics)) calculations, like 
[[matrix multiplication]] or [[matrix inverse]]
-   **[[Sequence|Sequence functions]]**: process data items with one-dimensional [sequences](https://en.wikipedia.org/wiki/Sequence).
-   **[[MetaScript|MetaScript functions]]**:to generate script (like [[for_each]]) or request information on [[tree items|tree item]] like [[SubItem_PropValues]]
-   **[[Allocation|Allocation functions]]**: mainly used for [Land Use Allocation](https://github.com/ObjectVision/LandUseModelling/wiki/Allocation)
-   **[[Miscellaneous|Miscellaneous functions]]**: remaining functions not categorized in other groups, like [[rnd_uniform]] or [[PropValue]]