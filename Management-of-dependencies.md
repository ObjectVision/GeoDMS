The GeoDMS controls dependencies in calculations.

In an [[expression]] like A = B + C, items B and C are called suppliers.

To calculate item A, first all suppliers (B and C) need to be calculated. An administration of the suppliers in calculations is essential for reliable results.

In this administration also status information is managed. If a supplier is already valid (calculated earlier and the input for the supplier is not changed), it does not have to be recalculated. This improves the efficiency in calculating.