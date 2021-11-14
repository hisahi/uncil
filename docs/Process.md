
# Process library

Documentation for the builtin process library. The features it provides
may not be available on all platforms.

This module can usually be accessed with `require("process")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`.

**WARNING!** If Uncil is used as a library in another application, the
parent application may not handle SIGCHLD by calling `wait`/`waitpid` for any
and all child processes; calling it for any child process launched through
`process.open` or `process.munge` results in undefined behavior. It is partially
for this reason that the library is disabled by default except on the
standalone interpreter.

## process.munge
`process.munge(path, args, data, [cwd], [env])`

Runs a program stored in `path` (full path) with command-line arguments and
pipes `data`, a blob, in as its `stdin`. Waits until the process finishes and
then returns three values; the exit code, everything written to `stdout` as a
blob and everything written to `stderr` as a blob. Prevents deadlocks, but may
consume large amounts of memory if there is a lot of data.

## process.open
`process.open(path, [args], [cwd], [env], [stdin], [stdout], [stderr])`

Runs a program, possibly with command-line arguments, and returns it as a
job object (`process.job`). `path` is the full path to the program to run.
`args` should be an array of command-line arguments that is passed to the
program (empty if not specified).

`cwd` is the path to the working directory as a string, or `null` to inherit.
`env` is the environment as a key-value table, or `null` to inherit. Note that
the keys in `env` are the only environment variables the process will get.

`stdin`, `stdout` and `stderr` all have a similar format. If they are given
a file object, the corresponding standard stream of the process will be
redirected to that file. With `stdin`, the file must support read operations,
while with `stdout` and `stderr` it must support write operations. Any file that
is specified will be treated as if it were opened in binary mode.
Special values include:

* `null`: inherit stdin, stdout or stderr from the interpreter (default).
* `false`: provide a closed file.
* `true`: create a new pipe (as if with `io.pipe()`) which can then be
          accessed through the job object.
* `"null"`: provides /dev/null or equivalent (stdout, stderr ignored,
            stdin immediately EOF's)
* `"stdout"` (stderr only): redirect to stdout.

## process.job

Represents a process started with `process.open`. Any of the variables may
technically be assigned, but this has no effect on the actual underlying
processes themselves. All job objects returned by `process.open` have
this table as their prototypes. A job that goes out of scope (`__close`) may,
if the process is still running, keep running it in the background until
completion, or the process may be killed immediately; this depends on
the platform.

### process.job.exitcode
`job->exitcode()` = `process.job.exitcode(job)`

Returns the exit code from the process, or `null` if it has not finished yet.

### process.job.halt
`job->halt()` = `process.job.halt(job)`

Sends a signal (SIGTERM) to the process telling it to stop running. Note that
SIGTERM may be ignored by the program. If the process has already finished
running, does nothing.

### process.job.running
`job->running()` = `process.job.running(job)`

Returns `true` if the process is still running or `false` if it has finished.

### process.job.signal
`job->signal(sig)` = `process.job.signal(job, sig)`

Sends a signal to the process. If the process has already finished running,
does nothing.

For POSIX, the following signals are available:
* `process.SIGHUP`
* `process.SIGINT`
* `process.SIGQUIT`
* `process.SIGILL`
* `process.SIGABRT`
* `process.SIGFPE`
* `process.SIGKILL`
* `process.SIGSEGV`
* `process.SIGPIPE`
* `process.SIGALRM`
* `process.SIGTERM`
* `process.SIGUSR1`
* `process.SIGUSR2`
* `process.SIGCHLD`
* `process.SIGCONT`
* `process.SIGSTOP`
* `process.SIGTSTP`

### process.job.stderr
`job->stderr()` = `process.job.stderr(job)`

Returns the `stderr` of the process if `true` was provided to `stderr` to create
a pipe or if a file was given; otherwise `null`.

If a file was given to `process.open` as the stdout, it will also be available
ḧere. The file however will be locked to the process for as long as it is
running.

Note the possibility of deadlocks if reading from stderr and the process is
waiting for data to be written to stdin. See `process.munge`.

### process.job.stdin
`job->stdin()` = `process.job.stdin(job)`

Returns the `stdin` of the process if `true` was provided to `stdin` to create
a pipe or if a file was given; otherwise `null`.

If a file was given to `process.open` as the stdout, it will also be available
ḧere. The file however will be locked to the process for as long as it is
running.

Note the possibility of deadlocks if writing to stdin and the process is unable
to write to stdout or stderr because they are already full. See `process.munge`.

### process.job.stdout
`job->stdout()` = `process.job.stdout(job)`

Returns the `stdout` of the process if `true` was provided to `stdout` to create
a pipe or if a file was given; otherwise `null`.

If a file was given to `process.open` as the stdout, it will also be available
ḧere. The file however will be locked to the process for as long as it is
running.

Note the possibility of deadlocks if reading from stdout and the process is
waiting for data to be written to stdin. See `process.munge`.

### process.job.wait
`job->wait()` = `process.job.wait(job)`

Waits until the process has finished running. If it already has,
returns instantly. Note that a `wait` may cause a deadlock if stdout or stderr
is piped and you are not reading the pipe. Returns the process exit code.

### process.job.waittimed
`job->waittimed()` = `process.job.waittimed(job, seconds)`

Waits until the process has finished running or until `seconds` seconds have
passed. Returns the process exit code if the process finished and `null`
if the timeout ran out before it finished.
