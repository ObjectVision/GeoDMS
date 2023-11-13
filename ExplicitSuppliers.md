The ExplicitSuppliers [[property]] is used to configure items that need to be updated before the item for which the property is configured is updated.

An example is a [[parameter]] with a list of files in a folder, needed to determine which files need to be read, needs to be updated with a batch command.

This parameter can be configured as ExplicitSupplier for the item that reads the file list.

ExplicitSuppliers can be combined with a semicolon (;) delimiter.