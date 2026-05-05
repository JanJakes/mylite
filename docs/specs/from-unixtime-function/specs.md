# `FROM_UNIXTIME()`

## Scope

MyLite implements `FROM_UNIXTIME(unix_timestamp)` and
`FROM_UNIXTIME(unix_timestamp, format)` for scalar and table-backed expression
contexts.

## Behavior

- The first argument is converted as a MySQL numeric timestamp: seconds since
  `1970-01-01 00:00:00` UTC.
- MyLite currently renders the result in UTC because mutable session time-zone
  state is not yet implemented.
- One-argument calls return a `DATETIME` value.
- Two-argument calls format the converted datetime using the same format-token
  surface as the first `DATE_FORMAT()` slice.
- `NULL` timestamp or `NULL` format returns `NULL`.
- Values before `0` or after `3001-01-18 23:59:59.999999` return `NULL`.
- Decimal timestamps round to microsecond precision. A carry from fractional
  rounding advances the second.
- Integer timestamps produce zero fractional seconds precision. Decimal
  literals preserve their declared fractional precision up to 6 digits.
  String arguments use MySQL's decimal conversion behavior and render six
  fractional digits.
- Invalid numeric strings convert to `0` and emit warning 1292 from numeric
  conversion.
- Native MySQL arity errors remain deferred; unsupported arities return
  MyLite's current unsupported diagnostic.

## Grammar

No dedicated grammar production is required. `FROM_UNIXTIME` is a regular
function call accepted by the existing scalar function grammar:

```lemon
scalar_function_call ::= identifier LP function_argument_list RP.
function_argument_list ::= expr.
function_argument_list ::= expr COMMA expr.
```

## Verified Expectations

Verified against MySQL 8.4.9:

| Expression | Result |
| --- | --- |
| `FROM_UNIXTIME(0)` | `1970-01-01 00:00:00` |
| `FROM_UNIXTIME(1)` | `1970-01-01 00:00:01` |
| `FROM_UNIXTIME(946782245.123456)` | `2000-01-02 03:04:05.123456` |
| `FROM_UNIXTIME(1.1234565)` | `1970-01-01 00:00:01.123457` |
| `FROM_UNIXTIME(1.9999995)` | `1970-01-01 00:00:02.000000` |
| `FROM_UNIXTIME(32536771199.999999)` | `3001-01-18 23:59:59.999999` |
| `FROM_UNIXTIME(32536771200)` | `NULL` |
| `FROM_UNIXTIME(-1)` | `NULL` |
| `FROM_UNIXTIME(NULL)` | `NULL` |
| `FROM_UNIXTIME('bad')` | `1970-01-01 00:00:00.000000` plus warning 1292 |
| `FROM_UNIXTIME(946782245.123456, '%Y-%m-%d %H:%i:%s.%f')` | `2000-01-02 03:04:05.123456` |

## References

- MySQL 8.4 Reference Manual, Date and Time Functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
