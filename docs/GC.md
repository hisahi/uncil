
# Garbage collector control library

Documentation for the builtin garbage collector control library that is used to
control the builtin garbage collector. Uncil uses a mark-and-sweep garbage
collector in addition to reference counting to track used objects and free
unused ones.

This module can usually be accessed with `require("gc")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`. This module
is not available if Uncil is compiled in sandboxed mode.

## gc.collect
`gc.collect()`

Instructs the garbage collector to run immediately. While running, the GC
will cause all other threads to be paused until the collection is finished.

## gc.disable
`gc.disable()`

Disables the garbage collector from running automatically. It can still be
manually invoked with `gc.collect()` or if memory allocation fails.

## gc.enable
`gc.enable()`

Re-enables automatic execution of the garbage collector.

## gc.enabled
`gc.enabled()`

Returns `true` if the garbage collector is enabled and can run automatically,
and `false` otherwise.

## gc.getthreshold
`gc.getthreshold()`

Returns an integer that represents the "threshold" for the garbage collector,
with lower values causing it to be activated more frequently. Currently this
represents the limit for the number of new entities that any one Uncil thread
may allocate before GC occurs (the count is incremented for every new
allocation and decremented with every deallocation). Once that limit is
reached, GC will run.

## gc.getusage
`gc.getusage()`

Returns an integer that is an approximation of the number of bytes in total
that Uncil has currently allocated. The actual heap memory usage will be larger
due to memory overhead associated with every allocated memory block (which
depends on the allocator used).

## gc.setthreshold
`gc.setthreshold(threshold)`

Sets the new GC threshold. `threshold` must be an integer. See `getthreshold`
for more information about what the threshold values actually signify.
