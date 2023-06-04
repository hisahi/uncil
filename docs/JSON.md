
# JSON library

Documentation for the builtin JSON encoding/decoding library.

This module can usually be accessed with `require("json")`.

## json.decode
`json.decode(text)`

Decodes a `text` containing a JSON object into an Uncil object and returns it.

## json.decodefile
`json.decodefile(file)`

Same as `json.decode` but reads one JSON object from the open file (`io.file`).
The file must be in text mode.

Not available if the `io` module has been disabled. This function
is not available if Uncil is compiled in sandboxed mode.

## json.encode
`json.encode(obj, [spacing], [mapper])`

Converts an Uncil object `obj` into a JSON string and returns it. By default,
Uncil will generate a JSON representation that is as compact as possible. You
can change this behavior by setting a value for `spacing`; any numeric value
will cause a more "fancy", human-readable format, and the value (if an integer)
represents the indent for every step taken into a JSON array or object. Positive
values represent the number of spaces and negative values represent the number
of tabs (for example, -1 is one tab per level, 4 is four spaces per level).

`mapper` can be specified as well; it should be a function that takes in
a value and returns a value that can be encoded as JSON.  Any value not of the
following types will cause an error: null, boolean, integer, float, string,
array, table (in particular, neither objects, functions nor blobs are supported;
a mapper, if specified, must map these to other types).

## json.encodefile
`json.encodefile(file, obj, [spacing], [mapper])`

Same as `json.encode` but writes the JSON object directly into an open
file (`io.file`). Trying to access the same file from the mapper results in
undefined behavior. The file must be in text mode.

Not available if the `io` module has been disabled. This function
is not available if Uncil is compiled in sandboxed mode.
