
# Threading library

Documentation for the builtin threading library which can be used to
implement proper multithreading as long as Uncil has been compiled with
multithreading support.

This module can usually be accessed with `require("thread")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`.

## thread.sleep()
`thread.sleep(seconds)`

Halts the execution of the current thread (and yields it if other threads
are available) and returns only once `seconds` seconds have passed.
The precision of the sleep routine is platform-dependent, and it is possible
that the sleep will end up being longer than specified.

## thread.threaded
`thread.threaded`

`true` if the Uncil interpreter was compiled with multithreading support,
`false` otherwise.

## thread.threader
`thread.threader`

Returns the name of the current multithreading library, or `null` if none of
them were compiled in.

## thread.yield
`thread.yield()`

Yields the current thread and lets other threads run. To sleep, see
`thread.sleep` (which automatically yields).

If Uncil was not compiled with multithreading support, this function
does nothing.

## thread.thread

Represents a thread created with `thread.new()`

### thread.thread.halt
`thr->halt()` = `thread.thread.halt(thr)`

Tells the Uncil thread to halt. If the thread is running a C function,
attempts to halt it may not have any effect if the C function never finishes,
as the halting status will only ever be checked by the Uncil VM (or by
C code aware of such a possibility; see `unc_yield`). Halting a thread is
an irreversible operation.

### thread.thread.hasfinished
`thr->hasfinished()` = `thread.thread.hasfinished(thr)`

Returns `true` if the thread has finished running.

### thread.thread.join
`thr->join()` = `thread.thread.join(thr)`

Waits until `thr` has finished running.

### thread.thread.jointimed
`thr->jointimed([seconds])` = `thread.thread.jointimed(thr, [seconds])`

Waits until `thr` has finished running or until `seconds` have passed.
Returns `true` if thread finished or `false` if the timeout expired before
the thread had finished.

### thread.thread.new
`thread.thread.new(f, args, [daemon])`

Creates a new thread and returns it as an object with `thread.thread` as its
prototype. `f` must be a callable and it will be given `args` (an array)
as the arguments. `daemon` (default `null`) is a boolean variable. If it is
`null`, its value is inherited from the thread that calls `thread.new`.

If it is `false`, the thread is considered a normal thread and the Uncil
interpreter will not exit if any non-daemon threads are still running. If
it is `true`, the thread is considered a daemon thread; the Uncil interpreter
may exit if daemon threads are still running. If a daemon thread is running
while the interpreter exits, it will call `thread.halt` on all of them and
then exit, taking the daemon threads with it.

The behavior of daemon and non-daemon threads in environments where the Uncil
thread is embedded into other programs depends on the implementation of the
program within which the interpreter has been embedded. `unc_destroy` will,
if the main thread(s) are destroyed, wait for any non-daemon threads to finish,
halt any daemon threads remaining afterwards and keep the environment in memory
while any are still running. The host program is free to exit after
`unc_destroy` has returned, which will cause daemon threads to exit
immediately.

Daemon threads in portable (embed-compatible) Uncil code **should not acquire**
any resources with the assumption that they would be able to gracefully
release them later.

If Uncil was not compiled with multithreading support, this function
results in an error.

Unhandled errors in threads are handled in an implementation-defined way. In
the standard Uncil interpreter, the threads will print the error message and
exit.

### thread.thread.start
`thr->start()` = `thread.thread.start(thr)`

Starts the specified `thread` object. A thread may only be started once.

## thread.lock

Represents a single mutual exclusion (mutex) lock which may be held by
only one thread at any given time. Lock objects have `__open` and `__close`
implemented to run `.acquire()` and `.release()`.

Whether locks are fair is platform-dependent. What happens when a lock
goes out of scope while acquired is also platform-dependent; you should release
the lock before it goes out of scope.

### thread.lock.acquire
`lck->acquire()` = `thread.lock.acquire(lck)`

Locks `lck`, or if it is already locked, waits until it is unlocked and
then locks it.

### thread.lock.acquiretimed
`lck->acquiretimed(seconds)` = `thread.lock.acquiretimed(lck, seconds)`

Locks `lck`, or if it is already locked, waits until it is unlocked and
then locks it. The wait period is limited to `seconds` seconds. Returns
`true` if the lock was locked or `false` if the timeout ran out before the
lock could've been locked. The lock is guaranteed to be locked only if
this function returns `true`.

### thread.lock.new
`thread.lock.new()`

Creates a new lock `lck` (with the prototype `thread.lock`) and returns it.

### thread.lock.release
`lck->release()` = `thread.lock.release(lck)`

Unlocks the lock `lck`. The thread calling `release` must be the one that
acquired that lock.

## thread.rlock

A re-entrant version of `thread.lock`; unlike with `thread.lock`, the same
thread may lock the same lock multiple times, and it is unlocked only once
it has been unlocked the same number of times. rlock objects have `__open`
and `__close` implemented to run `.acquire()` and `.release()`. 

Whether locks are fair is platform-dependent. What happens when a lock
goes out of scope while acquired is also platform-dependent; you should release
the lock before it goes out of scope.

### thread.rlock.acquire
`lck->acquire()` = `thread.rlock.acquire(lck)`

Locks `lck`, or if it is already locked, waits until it is unlocked and
then locks it.

### thread.rlock.acquiretimed
`lck->acquiretimed(seconds)` = `thread.rlock.acquiretimed(lck, seconds)`

Locks `lck`, or if it is already locked, waits until it is unlocked and
then locks it. The wait period is limited to `seconds` seconds. Returns
`true` if the lock was locked or `false` if the timeout ran out before the
lock could've been locked. The lock is guaranteed to be locked only if
this function returns `true`.

### thread.rlock.new
`thread.rlock.new()`

Creates a new lock `lck` (with the prototype `thread.rlock`) and returns it.

### thread.rlock.release
`lck->release()` = `thread.rlock.release(lck)`

Unlocks the rlock `lck`. The thread calling `release` must be the one that
acquired that lock.

## thread.semaphore

A simple counting semaphore. Semaphore objects have `__open` and `__close`
implemented to run `.acquire()` and `.release()` (with defaults of 1). 

Whether semaphores are fair is platform-dependent. What happens when a
semaphore goes out of scope while acquired is also platform-dependent; you
should release the semaphore before it goes out of scope.

### thread.semaphore.acquire
`sem->acquire([count])` = `thread.semaphore.acquire(sem, [count])`

Acquires `count` (or 1 by default) permits from the semaphore, or locks
until that many permits can be acquired. In other words, atomically decreases
the semaphore counter by `count`, unless the counter would wind up being
negative, in which case the program waits until that can be avoided.

### thread.semaphore.acquiretimed
`sem->acquiretimed(seconds, [count])`
= `thread.semaphore.acquiretimed(sem, seconds, [count])`

A version of `thread.semaphore.acquire` with a timeout. Returns `true` if
the acquire was successful or `false` otherwise. All or none of the permits
are acquired.

### thread.semaphore.new
`thread.semaphore.new(count)`

Creates a new semaphore `sem` (with the prototype `thread.semaphore`) and
returns it. The counter is initialized to `count`.

### thread.semaphore.release
`sem->release([count])` = `thread.semaphore.release(sem, [count])`

Adds `count` (or 1 by default) to the semaphore counter. Semaphores may
be released by any thread, even those that have not acquired it. It is
also possible to increase the semaphore counter beyond its initial value.

## thread.monitor

A monitor, i.e. condition variable with a lock associated to it. Monitor
objects have `__open` and `__close` implemented to run `.acquire()`
and `.release()` (with defaults of 1). 

Whether monitors are fair is platform-dependent. What happens when a monitor
goes out of scope while acquired or while one or more threads are waiting is
also platform-dependent; you should release the monitor lock and ensure no
threads are waiting on a monitor before it goes out of scope.

### thread.monitor.acquire
`mon->acquire()` = `thread.monitor.acquire(mon)`

Acquires the underlying lock of the monitor.

### thread.monitor.acquiretimed
`mon->acquiretimed(seconds)` = `thread.monitor.acquiretimed(mon, seconds)`

Same as `thread.monitor.acquire`, but with a timeout. Returns `true` if the
lock was acquired or `false` if the timeout expired.

### thread.monitor.new
`thread.monitor.new([lock])`

Creates a new monitor `mon` (with the prototype `thread.monitor`) and
returns it. If `lock` is specified, it must be a `thread.lock` or
`thread.rlock`; if not specified, a new `thread.rlock` is created.

### thread.monitor.notify
`mon->notify([n])` = `thread.monitor.notify(mon, [n])`

Notifies waiting threads. The notify function will wake up at least `n` (1
if not specified) of the threads waiting (as long as there are that many
threads waiting at the time of the call). Implementations may wake more than
`n` threads. If no threads are waiting on `mon`, this function has no effect;
if there are threads waiting but fewer than `n`, this function will behave
identically to `.notifyall`.

### thread.monitor.notifyall
`mon->notifyall()` = `thread.monitor.notifyall(mon)`

Notifies all waiting threads, if any.

### thread.monitor.release
`mon->release()` = `thread.monitor.release(mon)`

Releases the underlying lock of the monitor.

### thread.monitor.wait
`mon->wait()` = `thread.monitor.wait(mon)`

Suspends the thread until it is notified. The thread must hold the monitor
lock before calling `wait()`, and the lock will be released when `wait()`
suspends the thread. It will be acquired again before `wait()` returns, which
means that after `wait()`, the thread will still hold the mutex.

### thread.monitor.waittimed
`mon->waittimed([seconds])` = `thread.monitor.waittimed(mon, [seconds])`

Same as `thread.monitor.wait`, but with a timeout. The function returns
`true` if the thread was notified and `false` if the timeout expired.
Acquiring the lock may incur an additional time overhead.
