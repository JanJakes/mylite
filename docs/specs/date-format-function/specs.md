# `DATE_FORMAT()`

## Scope

MyLite implements `DATE_FORMAT(date, format)` for scalar and table-backed
expression contexts.

## Behavior

- `NULL` date or format returns `NULL`.
- Invalid date input returns `NULL` and emits warning 1292.
- Date and datetime inputs are parsed with MyLite's temporal conversion rules.
- The result is a connection-character-set text value.
- Supported format tokens include common MySQL date/time tokens:
  `%Y`, `%y`, `%M`, `%b`, `%m`, `%c`, `%D`, `%d`, `%e`, `%j`, `%W`, `%a`,
  `%w`, `%H`, `%k`, `%h`, `%I`, `%l`, `%i`, `%s`, `%S`, `%f`, `%p`, `%r`,
  `%T`, `%U`, `%u`, `%V`, `%v`, `%X`, `%x`, and `%%`.
- Unsupported `%` tokens follow MySQL's fallback shape by returning the
  character after `%` without the percent sign.

## Verified Expectations

Verified against MySQL 8.4.9:

| Expression | Result |
| --- | --- |
| `DATE_FORMAT('2000-01-02 03:04:05.123456','%Y-%m-%d %H:%i:%s.%f %W %M %a %b %D')` | `2000-01-02 03:04:05.123456 Sunday January Sun Jan 2nd` |
| `DATE_FORMAT('2000-01-02','%c %m %e %d %j %k %h %I %l %p %r %T %S %s %U %u %V %v %X %x %% %q')` | `1 01 2 02 002 0 12 12 12 AM 12:00:00 AM 00:00:00 00 00 01 00 01 52 2000 1999 % q` |
| `DATE_FORMAT(NULL,'%Y')` | `NULL` |
| `DATE_FORMAT('bad','%Y')` | `NULL` plus warning 1292 |
| `DATE_FORMAT('2000-01-02', NULL)` | `NULL` |
