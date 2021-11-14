
# Coroutine library

Documentation for the builtin coroutine library which can be used to
implement programs with coroutines, or multiple running "threads" that do not
however actually run at the same time (as opposed to actual threads).

Coroutines use cooperative multitasking and only one coroutine can be running
at a time on a thread. However, it's possible to resume other coroutines
and yield from them.

This module can usually be accessed with `require("coroutine")`.

## coroutine.canyield
`coroutine.canyield()`

Returns whether the currently executing code can use `coroutine.yield()`.
Always `false` for the main thread, and generally `true` for coroutines.

## coroutine.current
`coroutine.current()`

Returns the current coroutine that is executing right now on the thread,
or `null` if called from the main routine.

## coroutine.new
`coroutine.new(f)`

Creates a new coroutine for the function `f` and returns it, as a value
with the prototype `coroutine.coroutine`. Calling `coroutine.resume` on it
will start or resume the coroutine, and `resume` will return either the
values passed to `coroutine.yield` or the final return values of that function.
C functions may not be used for coroutines.

## coroutine.yield
`coroutine.yield(...)`

If the current coroutine supports yielding, yields back to the code that
resumed the coroutine. Any values are passed as extra return values from
`coroutine.resume`. Returns whatever values were passed to `coroutine.resume`
the next time around.

Yielding from the main thread causes an error.

## coroutine.coroutine

### coroutine.canresume
`co->canresume()` = `coroutine.coroutine.canresume(co)`

Returns `true` if it is possible to call `co->resume()`. This becomes
`false` once the coroutine finishes or if it throws an error, or if the
coroutine has not yielded yet since the last resume call.

### coroutine.hasfinished
`co->hasfinished()` = `coroutine.coroutine.hasfinished(co)`

Returns `true` if the coroutine has finished, either successfully or
by an error.

### coroutine.resume
`co->resume(...)` = `coroutine.coroutine.resume(co, ...)`

Resumes the coroutine `co` from where it left off. Any values given are passed
as parameters to the function, or return values from `coroutine.yield` if it
has already yielded before.

If the current coroutine supports yielding, yields back to the code that
resumed the coroutine. Any values are passed as extra return values from
`coroutine.resume`. `coroutine.resume` returns any values passed to
`coroutine.yield`. If an error occurs in a coroutine, it is passed onwards
to the code that used `coroutine.resume`.

When a coroutine function finishes, it yields its return values for the
final time automatically. Errors in coroutines propagate up.
