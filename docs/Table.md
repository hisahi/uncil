
# Table library

Documentation for the builtin table library, accessible as the global
variable `table` (by default) as well as by using `->` on table objects.

## table.copy
`tbl->copy()` = `table.copy(tbl)`

Creates a copy of the table `tbl` and returns it. The copy is a shallow copy;
if any of the values in the table are reference values, they are copied
by reference.

## table.length
`tbl->length()` = `table.length(tbl)`

Returns the number of keys in the table `tbl`.

## table.prune
`tbl->prune(func)` = `table.prune(tbl, func)`

Runs the function `func` on all key-value pairs within the table `tbl`. The
function is called with two arguments: the key and the value. The function
is expected to return a value. If the value is true when converted to boolean,
the key-value pair is removed from the table.

This function is designed for iterating over and possibly removing key-value
pairs from a table. Doing so by using a `for` iterator and `delete` would not
work, as that would be modifying the table while it is being iterated.
`table.prune` uses a custom iterator that can handle the removal of key-value
pairs as long as it is doing the removal by itself.

The table may not be modified by external code or the function given as the
argument while a prune operation is in progress, or undefined behavior
will occur.

