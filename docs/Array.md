
# Array library

Documentation for the builtin array library, accessible as the global
variable `array` (by default) as well as by using `->` on array objects.

## array.clear
`arr->clear()` = `array.clear(arr)`

Removes all items from the array.

## array.copy
`arr->copy()` = `array.copy(arr)`

Creates a copy of the array `arr` and returns it. The copy is a shallow copy;
if any of the items in the array are reference values, they are copied
by reference.

## array.extend
`arr->extend(arr2)` = `array.extend(arr2)`

Adds `arr2` to the end of `arr` such that the new length of `arr` is a sum
of the old length and the length of `arr2`, and the existing items in `arr`
precede the elements in `arr2`.

## array.find
`arr->find(value, [index])` = `array.find(arr, value, [index])`

Finds the first instance of a value from the array, and returns its index,
or -1 if none were found.

Formally: returns the smallest value `i >= index` (`index` = 0 if
not specified) for which `arr[i] == value`, or -1 if no such `i` exists.

## array.findlast
`arr->findlast(value, [index])` = `array.findlast(arr, value, [index])`

Finds the last instance of a value from the array, and returns its index,
or -1 if none were found.

Formally: returns the smallest value `i <= index`
(`index` = `arr->length() - 1` if not specified)
for which `arr[i] == value`, or -1 if no such `i` exists.

## array.insert
`arr->insert(index, value...)` = `array.insert(index, value...)`

Inserts one or more values into the middle of an array. After a successful
`insert`, the first of these values will be located at index `index`, which
should be within the range [0, _N_], where _N_ is the length of the array.

## array.length
`arr->length()` = `array.length(tbl)`

Returns the number of items in the array `arr`.

## array.new
`array.new(size, [value])`

Creates a new array that contains `size` copies of `value` (`null` by default)
and returns it.

## array.pop
`arr->pop()` = `array.pop(arr)`

Removes the last item from the array and returns it. Causes an error if
the array is empty.

## array.push
`arr->push(value...)` = `array.push(arr, value...)`

Inserts one or more values at the end of an array.

## array.remove
`arr->remove(index, [count])` = `array.remove(arr, index, [count])`

Removes elements from the array and shifts all subsequent items down to
fill the space. The first item is removed at `index` (which should be within the
range [0, _N_], where _N_ is the length of the array), and a total of `count`
items is removed (at most if the array is not long enough). If `count` is not
specified, it is taken as 1.

## array.repeat
`arr->repeat(count)` = `array.repeat(arr, count)`

Creates a new array that contains `count` repeats of the array `arr` and
returns it.

## array.reverse
`arr->reverse()` = `array.reverse(arr)`

Reverses the order of items in the array `arr`. Unlike with strings, this
operation happens in-place.

## array.sort
`arr->sort([comparer])` = `array.sort(arr, [comparer])`

Sorts the array, i.e. reorders its items so that they are in increasing order.
The sorting is done in-place (by modifying the original array); this function
returns no values.

If `comparer` is specified, it should be a function that takes in two items
and returns a positive numeric value if the first is greater than the second,
a negative numeric value if the first is less than the second and 
zero otherwise. If not specified, the standard comparison operators will
be used. Any pair of two elements within the array must be comparable or
the sorting operation may fail.

The sorting is guaranteed to be _stable_; the relative order of two items
of equal rank will not be affected.

Undefined behavior occurs if the comparison operator, either through `comparer`
or not, is not _pure_ (`a < b` is defined only in terms of the immutable parts
of `a` and `b`, not any mutable or external factors, and likewise for other
ordering operators) or _transitive_ (`a < b, b < c` -> `a < c`, and likewise
for other ordering operators).

## array.sub
`arr->sub(begin, [end])` = `array.sub(arr, begin, [end])`

Returns a copy of a contiguous section of `arr`. `begin` is the index of the
first item to include in the copy. It may be negative in which case the
length of the array is added to it; thus `-1` as `start` will only start
the copy from the _last_ item in `arr`.

If `end` is given, it is the index of the first item after `begin` that is
_not_ included in the copy. If `end` is omitted, the copy will extend
from `begin` until the end of `arr`.
