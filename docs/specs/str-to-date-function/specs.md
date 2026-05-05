# STR_TO_DATE function

This feature implements the first MyLite slice of the MySQL `STR_TO_DATE(str,
format)` scalar function. It is based on the MySQL 8.4 date and time function
documentation and MySQL 8.4.9 runtime probes.

`STR_TO_DATE()` is the parsing counterpart to `DATE_FORMAT()`: the format
string contains literal characters and `%`-introduced temporal specifiers, and
the input string is scanned from the beginning against that format. A literal
format character must match the same input character. A specifier consumes the
corresponding date or time part. Extra input bytes after a full format match are
ignored and produce a truncation warning.

## Scope

The first slice supports MyLite's existing scalar expression surfaces:

- no-table `SELECT`;
- table projection, `WHERE`, and `ORDER BY`;
- supported single-table `UPDATE` assignment and `DELETE` predicate paths;
- metadata and charset/collation introspection for supported result kinds.

The supported parser remains the ordinary scalar function call grammar:

```lemon
expr(A) ::= function_name(NK) LP function_arguments(ARGS) RP.
{
    A = mylite_ast_function_call(NK, ARGS);
}
```

`STR_TO_DATE()` accepts exactly two arguments in this slice. Other arities are
rejected by MyLite's current unsupported-function path.

## Format Tokens

The first slice supports the tokens needed by common application formats and by
the already-supported `DATE_FORMAT()`/`TIME_FORMAT()` slice:

| Token | Parsed part |
| --- | --- |
| `%Y` | four-digit year, with shorter numeric input accepted where unambiguous |
| `%y` | two-digit year, normalized with MySQL's 1970/2069 pivot |
| `%m`, `%c` | numeric month |
| `%d`, `%e` | numeric day of month |
| `%D` | day of month with English ordinal suffix |
| `%M`, `%b` | full or abbreviated English month name, case-insensitive |
| `%j` | day of year, resolved with the parsed or default zero year |
| `%W`, `%a` | full or abbreviated English weekday name, parsed and ignored |
| `%H`, `%k` | 24-hour hour |
| `%h`, `%I`, `%l` | 12-hour hour |
| `%i` | minute |
| `%s`, `%S` | second |
| `%f` | microseconds, padded to six result decimals |
| `%p` | `AM` or `PM`, case-insensitive |
| `%r` | 12-hour time, equivalent to `%h:%i:%s %p` |
| `%T` | 24-hour time, equivalent to `%H:%i:%s` |
| `%%` | literal `%` |

Unsupported `%` tokens are treated as literal token characters, matching the
current `DATE_FORMAT()` fallback strategy. Week-year parsing with `%X`, `%x`,
`%V`, `%v`, `%U`, and `%u` remains deferred because MySQL requires additional
weekday context to resolve many week strings.

## Semantics

If either argument is `NULL`, the result is `NULL` and no warning is emitted.

The result type is determined from the format:

- date-only formats return `DATE`;
- time-only formats return `TIME`;
- mixed date/time formats return `DATETIME`;
- dynamic or otherwise nonliteral formats are described as nullable
  `DATETIME(6)`, matching MySQL's conservative metadata behavior for dynamic
  format expressions.

For nonliteral format expressions, MyLite also materializes date and datetime
results as `DATETIME(6)`. A dynamic time-only format returns `NULL` in the
current default-mode slice because the conservative dynamic result kind is
datetime and no valid date parts were parsed.

Missing parts default to zero during parsing. MySQL's runtime behavior then
depends on SQL mode. MyLite does not yet expose SQL-mode-aware expression
evaluation, so this first slice follows MySQL 8.4.9's default mode behavior:
zero or incomplete date results are rejected with a warning, while time-only
zero parts are accepted. Explicit SQL-mode support for zero and incomplete date
results remains deferred.

Invalid dates and out-of-range parts return `NULL` and append warning 1411:

```text
Incorrect datetime value: '<input>' for function str_to_date
```

If the format matches a valid prefix but the input has extra trailing bytes,
the parsed temporal value is returned and warning 1292 is appended:

```text
Truncated incorrect date value: '<input>'
Truncated incorrect time value: '<input>'
Truncated incorrect datetime value: '<input>'
```

The warning category is selected from the result kind.

## Metadata

MySQL 8.4.9 reports binary temporal metadata:

| Expression shape | Type | Length | Decimals | Charset | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| date-only literal format | `DATE` | 10 | 0 | 63 | `BINARY` |
| time-only literal format without `%f` | `TIME` | 10 | 0 | 63 | `BINARY` |
| time-only literal format with `%f` | `TIME` | 17 | 6 | 63 | `BINARY` |
| date/time literal format without `%f` | `DATETIME` | 19 | 0 | 63 | `BINARY` |
| date/time literal format with `%f` | `DATETIME` | 26 | 6 | 63 | `BINARY` |
| dynamic format | `DATETIME` | 26 | 6 | 63 | `BINARY` |

All result descriptors are nullable. `CHARSET()`, `COLLATION()`, and
`COERCIBILITY()` report `binary`, `binary`, and `5` for non-`NULL`
`STR_TO_DATE()` results.

## MySQL 8.4.9 Test Expectations

Observed MySQL 8.4.9 behavior for the first slice includes:

| SQL | Result | Warnings |
| --- | --- | --- |
| `STR_TO_DATE('01,5,2013','%d,%m,%Y')` | `2013-05-01` | none |
| `STR_TO_DATE('May 1, 2013','%M %d, %Y')` | `2013-05-01` | none |
| `STR_TO_DATE('2024-02-29 12:34:56.123456','%Y-%m-%d %H:%i:%s.%f')` | `2024-02-29 12:34:56.123456` | none |
| `STR_TO_DATE('11:12:13 PM','%h:%i:%s %p')` | `23:12:13` | none |
| `STR_TO_DATE('2024 060','%Y %j')` | `2024-02-29` | none |
| `STR_TO_DATE('09:30:17a','%h:%i:%s')` | `09:30:17` | 1292 |
| `STR_TO_DATE('abc','abc')` | `NULL` in default mode | 1411 |
| `STR_TO_DATE('9','%s')` | `00:00:09` | none |
| `STR_TO_DATE(NULL,'%Y')` | `NULL` | none |
| `STR_TO_DATE('2024',NULL)` | `NULL` | none |
| `STR_TO_DATE('2024-13-01','%Y-%m-%d')` | `NULL` | 1411 |
| `STR_TO_DATE('2024-02-29x','%Y-%m-%d')` | `2024-02-29` | 1292 |

In `sql_mode=''`, MySQL permits incomplete date results such as `0000-09-00`.
Those SQL-mode-dependent variants remain compatibility gaps until MyLite has
session SQL mode wired into expression evaluation.

## Runtime Design

The implementation lives in the scalar expression runtime and reuses MyLite's
existing temporal value text setters so result values carry temporal type tags.
The parser is intentionally small and deterministic:

- scan the format once from left to right;
- parse numeric fields with bounded digit counts;
- parse month and meridiem names case-insensitively;
- record which date and time parts were present;
- infer the result kind from the parsed format;
- validate the resulting temporal parts before materializing a result.

Warnings are appended through the existing expression warning list so strict DML
promotion works the same way as other temporal scalar functions.

## Deferred Compatibility

- SQL-mode-dependent zero date and incomplete date materialization.
- Locale-sensitive month and weekday names beyond English names observed in
  MySQL's default locale.
- Week-year and weekday-derived date reconstruction.
- Exact MySQL native error diagnostics for unsupported arities.
- Prepared statement parameter metadata refinements beyond the conservative
  dynamic-format descriptor.
