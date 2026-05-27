# Baseline `CONVERT ... USING` Character Sets

## Purpose

This slice expands the scalar `CONVERT(expr USING charset)` surface from the
current `utf8mb4`-only baseline to the next MySQL-visible character-set cases
needed by compatibility tests. It also admits scalar postfix `COLLATE` for the
same metadata path.

The feature is deliberately scalar-only. It does not broaden table, column,
database, `SET NAMES`, or DML storage character-set support.

## Compatibility Authorities

- Official MySQL 8.4 reference manual, cast functions:
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- Official MySQL 8.4 reference manual, character-set introducers and `COLLATE`:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-collate.html>
- Official MySQL 8.4 reference manual, collation coercibility:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html>
- Observed MySQL 8.4.9 runtime behavior from
  `mylite-mysql-849 mysql:8.4.9`.

The MyLite grammar and runtime behavior below are independently authored from
the documentation and runtime observations. No MySQL implementation or grammar
source is used.

## Supported Surface

Admit these scalar forms in no-source `SELECT`, `SELECT ... FROM DUAL`, and
`DO` where scalar projections are already supported:

```sql
CONVERT(value USING utf8mb4)
CONVERT(value USING utf8)
CONVERT(value USING utf8mb3)
CONVERT(value USING latin1)
CONVERT(value USING 'utf8mb4')
CONVERT(value USING 'utf8')
CONVERT(value USING 'utf8mb3')
CONVERT(value USING 'latin1')
collatable_value COLLATE collation_name
collatable_value COLLATE 'collation_name'
```

The admitted `value` forms for `CONVERT(... USING ...)` are the existing scalar text-conversion inputs:
ordinary string values, decimal integer values in the current scalar envelope,
`TRUE`, `FALSE`, and `NULL`.

The admitted direct `collatable_value` forms are ordinary string, decimal
integer, boolean, `NULL`, hex, and bit literals, optional unary signs on integer
literals, `CAST(... AS BINARY)`, `CONVERT(..., BINARY)`, `CONVERT(... USING
BINARY)`, and the `CONVERT(... USING charset)` forms above. Broader scalar
children such as `DATABASE()` and `CONCAT()` under postfix `COLLATE` are
deferred unless they are used only as already-admitted metadata inputs below.

Supported conversion result metadata:

| Requested charset | Result charset | Default collation | Warning |
| --- | --- | --- | --- |
| `utf8mb4` | `utf8mb4` | `utf8mb4_0900_ai_ci` | none |
| `utf8` | `utf8mb3` | `utf8mb3_general_ci` | warning 3719 |
| `utf8mb3` | `utf8mb3` | `utf8mb3_general_ci` | warning 1287 |
| `latin1` | `latin1` | `latin1_swedish_ci` | none |

Supported scalar collations:

| Collation | Character set | Result metadata id |
| --- | --- | ---: |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 |
| admitted existing `utf8mb4_*` collations | `utf8mb4` | existing descriptor id |
| `utf8mb3_general_ci` | `utf8mb3` | 33 |
| `utf8mb3_bin` | `utf8mb3` | 83 |
| `latin1_swedish_ci` | `latin1` | 8 |
| `latin1_bin` | `latin1` | 47 |

For this slice, `latin1` conversion is ASCII-only for scalar values. MySQL can
transcode non-ASCII `utf8mb4` input to `latin1` bytes when representable, but
MyLite does not yet carry a general transcoder. Non-ASCII scalar values in
`CONVERT(... USING latin1)` fail with a deterministic MyLite capability
diagnostic until a real transcoding slice exists.

## Parser And AST

Existing grammar already admits `CONVERT(value USING option_name)`. This slice
adds postfix scalar `COLLATE`:

```lemon
expression(A) ::= expression(V) COLLATE(C) option_name(N). {
  A = mylite_sql_parser_make_collate_expression(state, V, C, N);
}
```

`option_name` is the existing identifier-or-string option-name node used by
charset/collation grammar. Quoted names are decoded with NUL rejection.

The AST adds `MYLITE_SQL_AST_COLLATE_EXPRESSION` with two children:

1. scalar expression
2. collation option name

## Runtime Semantics

`CONVERT(value USING charset)` evaluates `value` through MyLite's existing
scalar text-conversion helper, then attaches the charset/collation metadata
for the requested target. The physical scalar text bytes are not sent to
SQLite for further charset conversion. SQLite storage and schema text are not
changed.

`collatable_value COLLATE collation` validates that the collation belongs to the
expression metadata character set, evaluates the admitted child expression, and
returns the same scalar value with explicit collation metadata. `NULL`, hex,
bit, and binary string values keep MySQL's `binary` metadata, so nonbinary
collations on those values fail with MySQL's collation/charset mismatch
diagnostic. Numeric and boolean literals are admitted with the current
connection metadata when explicitly collated, matching the verified MySQL scalar
behavior.
`COERCIBILITY(expr COLLATE x)` returns `0`.

`CHARSET()`, `COLLATION()`, and `COERCIBILITY()` introspect the scalar metadata
from these conversion and collation expressions. The current `CONCAT()` metadata
slice observes admitted explicit-collation scalar arguments and rejects
conflicting explicit `CONCAT()` collations with MySQL's illegal-mix diagnostic.
An admitted explicit collation wins over binary implicit scalar arguments for
the metadata functions in this slice.
Result-column collation ids use MySQL ids for the admitted scalar metadata names.

Warnings are appended once per evaluated `CONVERT(... USING utf8)` or
`CONVERT(... USING utf8mb3)` occurrence, including nested occurrences under
`CHARSET()`, `COLLATION()`, or `COERCIBILITY()`. The existing diagnostics
snapshot behavior remains unchanged: reading `@@warning_count` in the same
scalar `SELECT` observes the previous diagnostic snapshot like the existing
runtime convention.

## Diagnostics

Runtime-verified MySQL diagnostics:

| Case | Error |
| --- | --- |
| unknown `USING` charset | `1115 / 42000` unknown character set |
| unknown postfix collation | `1273 / HY000` unknown collation |
| postfix collation not valid for expression charset | `1253 / 42000` collation not valid for character set |
| conflicting explicit `CONCAT()` collations | `1267 / HY000` illegal mix of collations |

MyLite-specific deterministic capability diagnostics:

| Case | Diagnostic |
| --- | --- |
| unsupported scalar value type | `CONVERT USING charset supports only string, integer, boolean, and NULL values` |
| unsupported non-ASCII `latin1` scalar value | `CONVERT USING latin1 supports only ASCII scalar values` |
| embedded NUL in charset or collation option name | option-name NUL diagnostic |
| unsupported `COLLATE` child metadata | `COLLATE supports only scalar values with known character set metadata` |

## Boundaries

This slice does not implement:

- table-backed `CONVERT(... USING charset)` or postfix `COLLATE`
- predicates, grouping, ordering, joins, DML assignments, or defaults using the
  new scalar conversion forms
- direct postfix `COLLATE` on general scalar functions such as `DATABASE()` or
  `CONCAT()`
- general charset transcoding
- non-ASCII `latin1` conversion
- full `utf8mb3` validation
- broader `SET NAMES`, `CREATE DATABASE`, `CREATE TABLE`, or column charset
  support for `latin1` / `utf8mb3`
- collation-aware comparison, ordering, grouping, distinct, or set operations
- protocol-grade charset metadata outside the existing result descriptor path

## MySQL 8.4.9 Runtime Observations

Observed against `mylite-mysql-849`:

- `CONVERT('Customer' USING utf8)` returns `Customer`, has charset `utf8mb3`,
  collation `utf8mb3_general_ci`, coercibility `2`, and warning 3719.
- `CONVERT('Customer' USING utf8mb3)` returns `Customer`, has charset
  `utf8mb3`, collation `utf8mb3_general_ci`, coercibility `2`, and warning
  1287.
- `CONVERT('Customer' USING latin1)` returns `Customer`, has charset `latin1`,
  collation `latin1_swedish_ci`, coercibility `2`, and no warning.
- `CONVERT('Customer' USING utf8mb4) COLLATE utf8mb4_bin` returns `Customer`,
  has charset `utf8mb4`, collation `utf8mb4_bin`, coercibility `0`, and no
  warning.
- `CONVERT('Customer' USING latin1) COLLATE latin1_bin` returns `Customer`,
  has charset `latin1`, collation `latin1_bin`, coercibility `0`, and no
  warning.
- `CONCAT('a' COLLATE utf8mb4_bin, 'b')` has charset `utf8mb4`, collation
  `utf8mb4_bin`, and coercibility `0`.
- `CONCAT('a' COLLATE utf8mb4_bin, 'b' COLLATE utf8mb4_0900_ai_ci)` fails with
  `1267 / HY000`.
- `NULL COLLATE utf8mb4_bin`, `X'41' COLLATE utf8mb4_bin`, and `B'101'
  COLLATE utf8mb4_bin` fail with `1253 / 42000` because the child expression
  metadata character set is `binary`.
- A mismatched collation such as `CONVERT('Customer' USING utf8mb4) COLLATE
  latin1_swedish_ci` fails with `1253 / 42000`.

## Tests

Add MySQL-runtime expectation coverage for:

- direct scalar values for `utf8mb4`, `utf8`, `utf8mb3`, `latin1`, and quoted
  charset names
- `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` metadata for the admitted
  conversion forms
- postfix `COLLATE` metadata and coercibility
- deprecation warning counts and warning rows for `utf8` and `utf8mb3`
- unknown charset, unknown collation, and mismatched collation diagnostics
- MyLite-specific non-ASCII `latin1` capability rejection

Fast C tests cover the same supported scalar result, metadata, warnings, and
diagnostic behavior without adding a new test framework.
