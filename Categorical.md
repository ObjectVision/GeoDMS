Categorical data represent characteristics such as a person's gender, municipality or political party. Often numeric values are used to represent categories (such as 1 for male 2 for female).

Categorical data in the GeoDMS is represented by [[values units|values unit]] with the following characteristics:

- value type used for categorical data. If the number of categories exceeds 256, a uint16/32 value type can be used. The value type uint4/uint2 can also be used for categories with maximum 16 or 4 options, but not all functions in the GeoDMS work on data items with these value types.
- [[metric]]: Categorical data items in the GeoDMS are usually configured without metric.

Two types of categorical data are distinguished:

### **ordinal**

Ordinal data is categorial data for which the values have a meaningful ordering. Think e.g. of the scores of a test, which are often categorized between 1 and 10. Some mathematical/statistical operations can be applied logically on this type of data (sum, min, max, mean etc.)

### non-ordinal

Non-ordinal categorial data use numeric data values without logical order. Representation gender information with a 1 for male and 2 for female is an example of this type of data. Be cautions with mathematical/statistical operations on this type of data.