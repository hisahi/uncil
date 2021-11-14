
# Blob library

Documentation for the builtin blob library, accessible as the global
variable `blob` (by default) as well as by using `->` on blob objects.

## blob.copy
`bl->copy()` = `blob.copy(bl)`

Creates a copy of the blob `bl` and returns it.

## blob.fill
`bl->fill(value, begin, [end])` = `blob.fill(bl, value, begin, [end])`

Fills a contiguous section of the blob with the byte `value`. `begin` is the
index of the first byte to replace. It may be negative in which case the
length of the blob is added to it; thus `-1` as `start` will only start
the fill from the _last_ byte in `bl`.

If `end` is given, it is the index of the first byte after `begin` that will
_not_ be replaced. If `end` is omitted, the fill will extend from `begin`
until the end of `bl`.

## blob.find
`bl->find(bl2, [index])` = `blob.find(bl, bl2, [index])`

Finds the first instance of the subblob `bl2` from the blob and returns its
index, or -1 if none were found. If `index` is specified, begins the search
in `bl` at that index.

## blob.findlast
`bl->findlast(bl2, [index])` = `blob.findlast(bl, bl2, [index])`

Finds the last instance of the subblob `bl2` from the blob and returns its
index, or -1 if none were found. If `index` is specified, begins the search
in `bl` at that index (proceeding backwards).

## blob.from
`blob.from(value...)`

If one or more integers are given, creates a new blob that contains those bytes
and returns it.

If a single array is given, behaves as above except that the integers are taken
from the array.

If a single blob is given, creates a copy of it and returns it.

## blob.insert
`bl->insert(index, value...)` = `blob.insert(index, value...)`

Inserts one or more bytes into the middle of an blob. After a successful
`insert`, the first of these bytes will be located at index `index`, which
should be within the range [0, _N_], where _N_ is the length of the blob.
If a single blob or array is given, the values to insert are taken from it
instead.

## blob.length
`bl->length()` = `blob.length(tbl)`

Returns the number of bytes in the blob `bl`.

## blob.new
`blob.new(size)`

Creates a new blob that contains `size` bytes with the value zero
and returns it.

## blob.push
`bl->push(value...)` = `blob.push(bl, value...)`

Inserts one or more bytes (integers between 0-255) at the end of an blob.
If a single blob or array is given, the values to insert are taken from it
instead.

## blob.remove
`bl->remove(index, [count])` = `blob.remove(bl, index, [count])`

Removes bytes from the blob and shifts all subsequent bytes down to fill
the space. The first byte is removed at `index` (which should be within the
range [0, _N_], where _N_ is the length of the array), and a total of `count`
bytes is removed (at most if the blob is not long enough). If `count` is not
specified, it is taken as 1.

## blob.repeat
`bl->repeat(count)` = `blob.repeat(bl, count)`

Creates a new blob that contains `count` repeats of the blob `bl` and
returns it.

## blob.resize
`bl->resize(size)` = `blob.resize(size)`

Resizes the blob `bl` in place. If the resize succeeds, the blob will afterwards
be `size` bytes long. Any bytes are preserved as long as they fit into the new
blob, starting from the beginning. If the blob is expanded, the new bytes will
have the value 0.

## blob.reverse
`bl->reverse()` = `blob.reverse(bl)`

Reverses the order of bytes in the blob `bl`. Unlike with strings, this
operation happens in-place.

## blob.size
`bl->size()` = `blob.size(tbl)` (alias of `blob.length`)

Returns the number of bytes in the blob `bl`.

## blob.sub
`bl->sub(begin, [end])` = `blob.sub(bl, begin, [end])`

Returns a copy of a contiguous section of `bl`. `begin` is the index of the
first byte to include in the copy. It may be negative in which case the
length of the blob is added to it; thus `-1` as `start` will only start
the copy from the _last_ byte in `bl`.

If `end` is given, it is the index of the first byte after `begin` that is
_not_ included in the copy. If `end` is omitted, the copy will extend
from `begin` until the end of `bl`.
