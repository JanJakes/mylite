# UUID_SHORT function

## Scope

This feature implements the zero-argument `UUID_SHORT()` scalar function.

`UUID_SHORT()` is available wherever MyLite currently evaluates supported
scalar built-ins: no-table `SELECT`, table-backed `SELECT` projection,
`WHERE`, `ORDER BY`, and supported single-table `UPDATE` and `DELETE`
expression paths. It remains out of scope for default expressions, generated
columns, replication metadata, and wire-protocol statement-based replication
warnings.

## Sources

- MySQL 8.4 Reference Manual, Miscellaneous Functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite `UUID()` generation slice:
  `docs/specs/uuid-function/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax

`UUID_SHORT()` uses MyLite's ordinary scalar function-call grammar. The only
supported arity is zero.

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

`UUID_SHORT(1)` and other nonzero arities are rejected through the current
unsupported scalar-function binding path. Exact native error 1582 exposure
remains part of the broader scalar arity diagnostics gap.

## Semantics

`UUID_SHORT()` returns a non-NULL unsigned 64-bit integer. MySQL constructs the
value from a server id byte, server startup seconds, and an incrementing
counter. MyLite has no server id variable yet, so this slice uses a
handle-local generated server-byte surrogate, handle initialization time in
seconds, and a 24-bit counter. The generated server-byte surrogate is limited
to `1`..`127` so current MyLite SQLite integer storage paths keep generated
values in signed 64-bit range while preserving unsigned result metadata.

Repeated calls on the same handle return increasing values until the 24-bit
counter wraps. On wrap, MyLite advances the stored startup-second component
before continuing from counter zero.

MySQL documents `UUID_SHORT()` as unsafe for statement-based replication.
MyLite does not implement binary logging yet, so this slice does not emit
replication warnings.

## Verified MySQL 8.4.9 Behavior

Runtime probes used `SET NAMES utf8mb4` except where noted.

| Expression | Result shape |
| --- | --- |
| `UUID_SHORT() > 0` | `1` |
| `UUID_SHORT() < UUID_SHORT()` | `1` |
| `UUID_SHORT() = UUID_SHORT()` | `0` |
| `CHARSET(UUID_SHORT())` | `binary` |
| `COLLATION(UUID_SHORT())` | `binary` |
| `COERCIBILITY(UUID_SHORT())` | `5` |
| `UUID_SHORT(1)` | error 1582 |

`mysql --column-type-info -vvv` reports `UUID_SHORT()` result metadata as
`LONGLONG`, unsigned, binary, numeric, non-NULL, declared length `21`,
decimals `0`, and binary collation id `63`.

Table probes verified that `UPDATE t SET u = UUID_SHORT()` assigns positive
and distinct unsigned values per row, and that predicates such as
`UUID_SHORT() <> UUID_SHORT()` are evaluated dynamically rather than folded as
constants.

## MyLite Design

`UUID_SHORT()` is registered as a session-dependent scalar function so it is
never included in no-table constant caches. Runtime evaluation lives with the
existing session function evaluator, alongside `UUID()`, `RAND()`, and current
statement state functions.

MyLite stores UUID_SHORT generator state on the database handle. SQLite's
built-in randomness initializes the generated server-byte surrogate and the
starting counter. The startup-second component is captured from UTC wall-clock
time when the state is first used.

Metadata follows MySQL's unsigned `LONGLONG` field metadata. Collation
introspection follows MySQL's numeric expression behavior and reports binary
charset/collation with coercibility `5`.

## Tests

Runtime tests cover:

- positive, increasing, and unequal per-call values
- `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` introspection
- metadata under `utf8mb4` and `latin1`
- table-backed `UPDATE` assignment, per-row positivity, duplicate detection,
  and dynamic predicate evaluation through `DELETE`
- unsupported nonzero arity

Parser tests cover ordinary zero-argument function-call parsing.

## Compatibility Notes

This slice uses a generated server-byte surrogate until MyLite implements
server system variables. It also does not emit statement-based replication
warnings because MyLite does not yet implement binary logging or replication
mode state.
