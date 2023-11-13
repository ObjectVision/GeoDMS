The GeoDMS splits [[expressions|expression]] into atomic steps, called subexpressions.

Assume the following expression is configured: A + B + C. The GeoDMS first calculates the result of the subexpression: A + B. Next, item C is added to the intermediary result.

## reasons

This splitting up of expressions in their atomic steps is done for two reasons:

1.  If in another expression the result of the subexpression: A + B is used too, for example in an expression: A + B + D, the result of the subexpression A + B can be re-used.
2.  Expressions can refer to a large number of [[data items|data item]]. If all these data items need to be loaded and kept in internal memory, the size of this memory can become a problem. By dividing expression in atomic steps, this problem can be solved.


