
# Date/time library

Documentation for the builtin date/time library for processing dates according
to the Gregorian calendar. Some of the features it provides may not be
available on all platforms.

This module can usually be accessed with `require("time")`.

## time.clock
`time.clock()`

Returns the processor time consumed by the program. This is a wrapper around
the C standard library function `clock()`. This value may be shared
between threads and thus may be meaningless in multi-threaded programs.

## time.time
`time.time()`

Returns the number of seconds that have elapsed since the epoch. The precision,
epoch and whether leap seconds are included is platform-dependent, but
on *nix systems and Windows, the epoch is January 1, 1970, 00:00:00 UTC and
leap seconds are not included, so called "Unix time", while the precision is
at least down to one second. You can use `time.unix` to check if time is in
Unix time.

This function may return two `null` values if the underlying call fails, but
this should not happen on most systems.

## time.timefrac
`time.timefrac()`

Returns the number of seconds that have elapsed since the epoch as an integer
or integral floating-point number and the fractional number of seconds
as a floating-point value in [0, 1[. If `time.unix` is `false`, this function
may simply return the same result as `time.time()` and a `0`.

This function may return two `null` values if the underlying call fails, but
this should not happen on most systems.

## time.unix
`time.unix`

`true` if `time.time()` and `time.timefrac()` are guaranteed to return
Unix timestamps, and functions taking return values from `time.time()`, such
as `time.gmtime` and `time.localtime`, also accept Unix timestamps.
This value is platform-dependent.

## time.datetime

Represents a date/time as obtained from `gmtime` or `localtime`.

### time.datetime.second

The second as a number between 0-59; the value 60 is also allowed on many
platforms and is used to represent a leap second.

### time.datetime.minute

The minute as a number between 0-59.

### time.datetime.hour

The hour as a number between 0-23.

### time.datetime.day

The day of the month as a number between 1-31.

### time.datetime.month

The month as a number between 1-12 (not 0-11).

### time.datetime.year

The year.

### time.datetime.weekday

The day of the week as a number between 1-7. 1 represents Monday and 7
represents Sunday. This value is ignored by `time.mktime()`.

### time.datetime.yearday

The day of the year as a number between 1-366.
This value is ignored by `time.mktime()`.

### time.datetime.dst

Whether daylight saving time is in effect; `true` if yes, `false` if no,
and `null` if not known.

### time.datetime.us

The microsecond as a number between 0-999999.
This value may be ignored by `time.mktime()`.

### time.datetime.convert
`dt->convert()` = `time.datetime.convert(dt, tz0, tz1)`

Converta a datetime `dt` from the timezone `tz0` to timezone `tz1`. Returns
the converted datetime. weekday and yearday in `dt` are ignored but correctly
set in the returned datetime.

Roughly equivalent to `time.fromtime(time.totime(dt, tz0), tz1)`.

### time.datetime.fromiso
`time.datetime.fromiso(iso)`

Converts a string `iso`, assumed to be in ISO 8601 format with
the following conditions:

* Date and time information must both be included. Date comes first, then a
  letter `T` acts as a delimiter before the time information
* The year must be a positive integer greater than or equal to 1583, with
  no sign
* Year, month and day are separated by hyphens or not at all (8 digits)
* The time zone info, if present, must be `Z` or a `+`/`-` followed by
  hours and minutes or just hours, possibly separated by a colon
* Week or ordinal dates are not supported
* Truncated representations are not supported
* No other info, such as durations or intervals, is supported

The first return value is a timezone-agnostic `time.datetime`, while the second
return value is the number of seconds east of UTC according to the time zone
information present in the timestamp (or `null` if there wasn't any).

### time.datetime.fromtime
`time.datetime.fromtime(ts, [tz])`

Converts a timestamp `ts`, as that returned by `time.time()`, with the time zone
`tz` (or UTC if none specified) into a `time.datetime` object and returns it.

### time.datetime.gmtime
`time.datetime.gmtime()`
`dt->gmtime()` = `time.datetime.gmtime(dt)`

Converts the current time or the given value (which should be that given by
`time.time()`) into a `time.datetime` object according to Unix time.

### time.datetime.localtime
`time.datetime.localtime()`
`dt->localtime()` = `time.datetime.localtime(dt)`

Converts the current time or the given value (which should be that given by
`time.time()`) into a `time.datetime` object according to the local timezone.

### time.datetime.mktime
`dt->mktime()` = `time.datetime.mktime(dt)`

Converts a `time.datetime`, assumed to be in local time, into a numeric value
like which can be returned by `time.time()`. If not representable, an error
is thrown. The `dst` value in the `datetime` is taken into consideration;
it should be `true` if DST is in effect, `false` if not, and `null` if not
known.

If `tz` is the UTC timezone, `time.datetime.mktime(time.datetime.localtime())`
should return the same value as `time.time()` (assuming the two functions
would be called simultaneously).

### time.datetime.new
`time.datetime.new(year, month, day, hour, minute, second, dst)`

Creates a new `time.datetime` with the specified year, month, day, hour, minute,
second and DST flag values. `weekday` and `yearday` are computed automatically.

### time.datetime.toiso
`dt->toiso([tz])` = `time.datetime.toiso(dt, [tz])`

Converts a `time.datetime` into ISO 8601 format. If `tz` is given as a
`time.timezone`, the time zone information is also embedded into the result.
Note that according to the standard, timestamps without time zone information
are assumed to be in local time.

### time.datetime.totime
`dt->totime()` = `time.datetime.totime(dt, [tz])`

Converts a `time.datetime` with the time zone `tz` (or UTC if none specified)
into an Unix timestamp and returns it. If the time zone has daylight saving
time, the `dst` value in the `datetime` is taken into consideration; it
should be `true` if DST is in effect, `false` if not, and `null` if not known.
For time zones without DST (such as UTC), `datetime.dst` is ignored.

If `tz` is the UTC timezone, `time.totime(time.gmtime())` should return the
same value as `time.time()` (assuming they would be called simultaneously).

### time.datetime.totimefrac
`dt->totimefrac()` = `time.datetime.totimefrac(dt, [tz])`

Same as `time.totime`, but returns the integer and fractional parts separately.

## time.timezone

Represents a time zone.

### time.timezone.dst
`tz->dst()` = `time.timezone.dst(tz)`

Returns `true` if the time zone has (or may have) DST or `false` if it does not.

### time.timezone.islocal
`tz->islocal()` = `time.timezone.islocal(tz)`

Returns `true` if the time zone is the local pseudo-time zone.

### time.timezone.name
`tz->name()` = `time.timezone.name(tz)`

Returns the name of the time zone in an implementation-defined, but usually
human-readable, format.

### time.timezone.namedst
`tz->namedst()` = `time.timezone.namedst(tz)`

Returns the name of the time zone's DST version in an implementation-defined,
but usually human-readable, format.

### time.timezone.now
`tz->now()` = `time.timezone.now(tz)`

Returns the current time in the time zone `tz` as a datetime `dt`.
Equivalent to `time.datetime.fromtime(time.time(), tz)`.

### time.timezone.offset
`tz->offset()` = `time.timezone.offset(tz)`

Returns the offset of the time zone as the number of seconds east of UTC
without daylight saving time, or `null` if not known (possible for
pseudo-time zones, such as `time.timezone.local`).

### time.timezone.offsetdst
`tz->offsetdst()` = `time.timezone.offsetdst(tz)`

Returns the offset of the time zone as the number of seconds east of UTC
with daylight saving time, or `null` if not known (possible for
pseudo-time zones, such as `time.timezone.local`).

### time.timezone.get
`time.timezone.get(name)`

Returns the time zone with the given name, or `null` if no time zone with that
name was available. If name is `null`, returns the currently configured time
zone (which does not update automatically unlike `time.timezone.local`). The
return value, if not null, has the prototype `time.timezone`.

### time.timezone.local
`time.timezone.local()`

Returns the local time zone or `null` if not available. This time zone
is a pseudo time zone; it always represents the local time zone whenever it
is used, regardless of when `local()` was called. Thus, if the user changes
the time zone after `local`, the time zone is automatically updated.
If you want to get the current time zone that says constant even if the system
time zone is changed, use `time.timezone.get`.
The return value, if not null, has the prototype `time.timezone`.

### time.timezone.utc
`time.timezone.utc()`

Returns the "UTC" (Unix time) time zone. The return value has
the prototype `time.timezone`.
