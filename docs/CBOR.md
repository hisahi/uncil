
# CBOR library

Documentation for the builtin CBOR encoding/decoding library.

This module can usually be accessed with `require("cbor")`.

## cbor.decode
`cbor.decode(bl)`

Decodes a `bl` blob containing a CBOR item into an Uncil object and
returns it and the number of bytes read in total.

Semantic tags are returned as `cbor.semantictag` objects.

## cbor.decodefile
`cbor.decodefile(file)`

Same as `cbor.decode` but reads one CBOR item from the open file (`io.file`)
and only returns that item. The file must be in binary mode.

Not available if the `io` module has been disabled. This function
is not available if Uncil is compiled in sandboxed mode.

## cbor.encode
`cbor.encode(obj, [mapper])`

Converts an Uncil object `obj` into a CBOR item and returns it as a blob.

`mapper` can be specified as well; it should be a function that takes in
a value and returns a value that can be encoded as CBOR.  Any value not of the
following types will cause an error: null, boolean, integer, float, string,
blob, array, table (in particular, neither objects nor functions are supported,
with some exceptions; see below); a mapper, if specified, must map these
to other types).

For objects, an exception is made for objects within `cbor.semantictag` as their
prototype. If so, they are encoded as a semantic tag instead.

## cbor.encodefile
`cbor.encodefile(file, obj, [mapper])`

Same as `cbor.encode` but writes the CBOR object directly into an open
file (`io.file`). Trying to access the same file from the mapper results in
undefined behavior. The file must be in binary mode.

Not available if the `io` module has been disabled. This function
is not available if Uncil is compiled in sandboxed mode.

## cbor.semantictag
`cbor.semantictag`

A simple prototype used to represent CBOR semantic tags.

### cbor.semantictag.data
`st.data`

The tag data as a value.

### cbor.semantictag.tag
`st.tag`

The tag ID.
