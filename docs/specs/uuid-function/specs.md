# UUID function

## Scope

This feature implements the zero-argument `UUID()` scalar function.

`UUID()` is available wherever MyLite currently evaluates supported scalar
built-ins: no-table `SELECT`, table-backed `SELECT` projection, `WHERE`,
`ORDER BY`, and supported single-table `UPDATE` and `DELETE` expression paths.
It remains out of scope for default expressions, generated columns, replication
metadata, and wire-protocol statement-based replication warnings.

`UUID_SHORT()` generation is specified separately in
`docs/specs/uuid-short-function/specs.md`.

## Sources

- MySQL 8.4 Reference Manual, Miscellaneous Functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing related MyLite UUID conversion slice:
  `docs/specs/uuid-conversion-functions/specs.md`
- Existing related MyLite UUID_SHORT slice:
  `docs/specs/uuid-short-function/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax

`UUID()` uses MyLite's ordinary scalar function-call grammar. The only
supported arity is zero.

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

`UUID(1)` and other nonzero arities are rejected through the current
unsupported scalar-function binding path. Exact native error 1582 exposure
remains part of the broader scalar arity diagnostics gap.

## Semantics

`UUID()` returns a lowercase canonical UUID string in
`aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee` format.

The generated value follows the version-1 UUID layout:

- the high nibble of the third group is `1`
- the fourth group uses the RFC variant pattern
- the node field is generated per MyLite handle from SQLite randomness, with
  the multicast bit set because it is not a hardware MAC address
- repeated calls on the same handle are made monotonic using handle-local
  timestamp and clock-sequence state

MySQL documents `UUID()` as unsafe for statement-based replication. MyLite does
not implement binary logging yet, so this slice does not emit replication
warnings.

## Verified MySQL 8.4.9 Behavior

Runtime probes used `SET NAMES utf8mb4` except where noted.

| Expression | Result shape |
| --- | --- |
| `IS_UUID(UUID())` | `1` |
| `LENGTH(UUID())` | `36` |
| `CHAR_LENGTH(UUID())` | `36` |
| `SUBSTRING(UUID(), 15, 1)` | `1` |
| `LOWER(SUBSTRING(UUID(), 20, 1)) IN ('8','9','a','b')` | `1` |
| `UUID() = UUID()` | `0` |
| `CHARSET(UUID())` | `utf8mb3` |
| `COLLATION(UUID())` | `utf8mb3_general_ci` |
| `COERCIBILITY(UUID())` | `4` |
| `UUID(1)` | error 1582 |

`mysql --column-type-info -vvv` reports `UUID()` result metadata as
`VAR_STRING`, decimals `31`, nullable, no flags. Under `SET NAMES utf8mb4` the
declared length is `144` and the field collation id is
`utf8mb4_0900_ai_ci` (`255`). Under `SET NAMES latin1`, the declared length is
`36` and the field collation id is `latin1_swedish_ci` (`8`). This differs from
`CHARSET(UUID())`, which reports the UUID value's semantic character set as
`utf8mb3`.

Table probes verified that `UPDATE t SET u = UUID()` assigns valid and distinct
UUID strings per row, and that predicates such as `UUID() <> UUID()` are
evaluated dynamically rather than folded as constants.

## MyLite Design

`UUID()` is registered as a session-dependent scalar function so it is never
included in no-table constant caches. Runtime evaluation lives with the
existing session function evaluator, alongside `RAND()` and current statement
state functions.

MyLite stores UUID generator state on the database handle, not in process-global
storage. This keeps independent handles isolated and avoids a new dependency.
SQLite's built-in randomness initializes the node and clock sequence. The
timestamp portion uses UTC wall-clock time when available and monotonically
advances within the handle when multiple calls share the same clock tick.

Metadata follows MySQL's field metadata behavior and uses the current connection
charset for field descriptors. Collation introspection follows MySQL's
expression behavior and reports `utf8mb3_general_ci` with coercibility `4`.

## Tests

Runtime tests cover:

- result shape, length, version nibble, variant nibble, and per-call inequality
- `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` introspection
- metadata under `utf8mb4` and `latin1`
- table-backed `UPDATE` assignment, per-row validity, duplicate detection, and
  dynamic predicate evaluation through `DELETE`
- unsupported nonzero arity

Parser tests cover ordinary zero-argument function-call parsing.

## Compatibility Notes

This slice does not emit statement-based replication warnings because MyLite
does not yet implement binary logging or replication mode state.
